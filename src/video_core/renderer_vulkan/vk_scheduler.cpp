// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <utility>

#include "common/microprofile.h"
#include "common/thread.h"
#include "video_core/renderer_vulkan/vk_command_pool.h"
#include "video_core/renderer_vulkan/vk_master_semaphore.h"
#include "video_core/renderer_vulkan/vk_query_cache.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/vk_state_tracker.h"
#include "video_core/renderer_vulkan/vk_texture_cache.h"
#include "video_core/vulkan_common/vulkan_device.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Vulkan {

MICROPROFILE_DECLARE(Vulkan_WaitForWorker);

void VKScheduler::CommandChunk::ExecuteAll(vk::CommandBuffer cmdbuf) {
    auto command = first;
    while (command != nullptr) {
        auto next = command->GetNext();
        command->Execute(cmdbuf);
        command->~Command();
        command = next;
    }
    submit = false;
    command_offset = 0;
    first = nullptr;
    last = nullptr;
}

VKScheduler::VKScheduler(const Device& device_, StateTracker& state_tracker_)
    : device{device_}, state_tracker{state_tracker_},
      master_semaphore{std::make_unique<MasterSemaphore>(device)},
      command_pool{std::make_unique<CommandPool>(*master_semaphore, device)} {
    AcquireNewChunk();
    AllocateWorkerCommandBuffer();
    worker_thread = std::thread(&VKScheduler::WorkerThread, this);
}

VKScheduler::~VKScheduler() {
    {
        std::lock_guard lock{work_mutex};
        quit = true;
    }
    work_cv.notify_all();
    worker_thread.join();
}

void VKScheduler::Flush(VkSemaphore semaphore) {
    SubmitExecution(semaphore);
    AllocateNewContext();
}

void VKScheduler::Finish(VkSemaphore semaphore) {
    const u64 presubmit_tick = CurrentTick();
    SubmitExecution(semaphore);
    WaitWorker();
    Wait(presubmit_tick);
    AllocateNewContext();
}

void VKScheduler::WaitWorker() {
    MICROPROFILE_SCOPE(Vulkan_WaitForWorker);
    DispatchWork();

    std::unique_lock lock{work_mutex};
    wait_cv.wait(lock, [this] { return work_queue.empty(); });
}

void VKScheduler::DispatchWork() {
    if (chunk->Empty()) {
        return;
    }
    {
        std::lock_guard lock{work_mutex};
        work_queue.push(std::move(chunk));
    }
    work_cv.notify_one();
    AcquireNewChunk();
}

void VKScheduler::RequestRenderpass(const Framebuffer* framebuffer) {
    const VkRenderPass renderpass = framebuffer->RenderPass();
    const VkFramebuffer framebuffer_handle = framebuffer->Handle();
    const VkExtent2D render_area = framebuffer->RenderArea();
    if (renderpass == state.renderpass && framebuffer_handle == state.framebuffer &&
        render_area.width == state.render_area.width &&
        render_area.height == state.render_area.height) {
        return;
    }
    EndRenderPass();
    state.renderpass = renderpass;
    state.framebuffer = framebuffer_handle;
    state.render_area = render_area;

    Record([renderpass, framebuffer_handle, render_area](vk::CommandBuffer cmdbuf) {
        const VkRenderPassBeginInfo renderpass_bi{
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .pNext = nullptr,
            .renderPass = renderpass,
            .framebuffer = framebuffer_handle,
            .renderArea =
                {
                    .offset = {.x = 0, .y = 0},
                    .extent = render_area,
                },
            .clearValueCount = 0,
            .pClearValues = nullptr,
        };
        cmdbuf.BeginRenderPass(renderpass_bi, VK_SUBPASS_CONTENTS_INLINE);
    });
    num_renderpass_images = framebuffer->NumImages();
    renderpass_images = framebuffer->Images();
    renderpass_image_ranges = framebuffer->ImageRanges();
}

void VKScheduler::RequestOutsideRenderPassOperationContext() {
    EndRenderPass();
}

bool VKScheduler::UpdateGraphicsPipeline(GraphicsPipeline* pipeline) {
    if (state.graphics_pipeline == pipeline) {
        return false;
    }
    state.graphics_pipeline = pipeline;
    return true;
}

void VKScheduler::WorkerThread() {
    Common::SetCurrentThreadName("yuzu:VulkanWorker");
    do {
        if (work_queue.empty()) {
            wait_cv.notify_all();
        }
        std::unique_ptr<CommandChunk> work;
        {
            std::unique_lock lock{work_mutex};
            work_cv.wait(lock, [this] { return !work_queue.empty() || quit; });
            if (quit) {
                continue;
            }
            work = std::move(work_queue.front());
            work_queue.pop();
        }
        const bool has_submit = work->HasSubmit();
        work->ExecuteAll(current_cmdbuf);
        if (has_submit) {
            AllocateWorkerCommandBuffer();
        }
        std::lock_guard reserve_lock{reserve_mutex};
        chunk_reserve.push_back(std::move(work));
    } while (!quit);
}

void VKScheduler::AllocateWorkerCommandBuffer() {
    current_cmdbuf = vk::CommandBuffer(command_pool->Commit(), device.GetDispatchLoader());
    current_cmdbuf.Begin({
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr,
    });
}

void VKScheduler::SubmitExecution(VkSemaphore semaphore) {
    EndPendingOperations();
    InvalidateState();

    const u64 signal_value = master_semaphore->NextTick();
    Record([semaphore, signal_value, this](vk::CommandBuffer cmdbuf) {
        cmdbuf.End();

        const u32 num_signal_semaphores = semaphore ? 2U : 1U;

        const u64 wait_value = signal_value - 1;
        const VkPipelineStageFlags wait_stage_mask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

        const VkSemaphore timeline_semaphore = master_semaphore->Handle();
        const std::array signal_values{signal_value, u64(0)};
        const std::array signal_semaphores{timeline_semaphore, semaphore};

        const VkTimelineSemaphoreSubmitInfoKHR timeline_si{
            .sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO_KHR,
            .pNext = nullptr,
            .waitSemaphoreValueCount = 1,
            .pWaitSemaphoreValues = &wait_value,
            .signalSemaphoreValueCount = num_signal_semaphores,
            .pSignalSemaphoreValues = signal_values.data(),
        };
        const VkSubmitInfo submit_info{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext = &timeline_si,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &timeline_semaphore,
            .pWaitDstStageMask = &wait_stage_mask,
            .commandBufferCount = 1,
            .pCommandBuffers = cmdbuf.address(),
            .signalSemaphoreCount = num_signal_semaphores,
            .pSignalSemaphores = signal_semaphores.data(),
        };
        switch (const VkResult result = device.GetGraphicsQueue().Submit(submit_info)) {
        case VK_SUCCESS:
            break;
        case VK_ERROR_DEVICE_LOST:
            device.ReportLoss();
            [[fallthrough]];
        default:
            vk::Check(result);
        }
    });
    chunk->MarkSubmit();
    DispatchWork();
}

void VKScheduler::AllocateNewContext() {
    // Enable counters once again. These are disabled when a command buffer is finished.
    if (query_cache) {
        query_cache->UpdateCounters();
    }
}

void VKScheduler::InvalidateState() {
    state.graphics_pipeline = nullptr;
    state_tracker.InvalidateCommandBufferState();
}

void VKScheduler::EndPendingOperations() {
    query_cache->DisableStreams();
    EndRenderPass();
}

void VKScheduler::EndRenderPass() {
    if (!state.renderpass) {
        return;
    }
    Record([num_images = num_renderpass_images, images = renderpass_images,
            ranges = renderpass_image_ranges](vk::CommandBuffer cmdbuf) {
        std::array<VkImageMemoryBarrier, 9> barriers;
        for (size_t i = 0; i < num_images; ++i) {
            barriers[i] = VkImageMemoryBarrier{
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .pNext = nullptr,
                .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                                 VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                .dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT |
                                 VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                                 VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                                 VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                                 VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
                .newLayout = VK_IMAGE_LAYOUT_GENERAL,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = images[i],
                .subresourceRange = ranges[i],
            };
        }
        cmdbuf.EndRenderPass();
        cmdbuf.PipelineBarrier(VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                                   VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT |
                                   VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                               VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, nullptr, nullptr,
                               vk::Span(barriers.data(), num_images));
    });
    state.renderpass = nullptr;
    num_renderpass_images = 0;
}

void VKScheduler::AcquireNewChunk() {
    std::lock_guard lock{reserve_mutex};
    if (chunk_reserve.empty()) {
        chunk = std::make_unique<CommandChunk>();
        return;
    }
    chunk = std::move(chunk_reserve.back());
    chunk_reserve.pop_back();
}

} // namespace Vulkan
