// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <array>
#include <cstring>
#include <memory>
#include <tuple>
#include <vector>

#include "common/assert.h"
#include "common/common_types.h"
#include "common/math_util.h"
#include "core/core.h"
#include "core/frontend/emu_window.h"
#include "core/memory.h"
#include "video_core/gpu.h"
#include "video_core/host_shaders/vulkan_present_frag_spv.h"
#include "video_core/host_shaders/vulkan_present_vert_spv.h"
#include "video_core/renderer_vulkan/renderer_vulkan.h"
#include "video_core/renderer_vulkan/vk_blit_screen.h"
#include "video_core/renderer_vulkan/vk_master_semaphore.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/vk_shader_util.h"
#include "video_core/renderer_vulkan/vk_swapchain.h"
#include "video_core/surface.h"
#include "video_core/textures/decoders.h"
#include "video_core/vulkan_common/vulkan_device.h"
#include "video_core/vulkan_common/vulkan_memory_allocator.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Vulkan {

namespace {

struct ScreenRectVertex {
    ScreenRectVertex() = default;
    explicit ScreenRectVertex(f32 x, f32 y, f32 u, f32 v) : position{{x, y}}, tex_coord{{u, v}} {}

    std::array<f32, 2> position;
    std::array<f32, 2> tex_coord;

    static VkVertexInputBindingDescription GetDescription() {
        return {
            .binding = 0,
            .stride = sizeof(ScreenRectVertex),
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        };
    }

    static std::array<VkVertexInputAttributeDescription, 2> GetAttributes() {
        return {{
            {
                .location = 0,
                .binding = 0,
                .format = VK_FORMAT_R32G32_SFLOAT,
                .offset = offsetof(ScreenRectVertex, position),
            },
            {
                .location = 1,
                .binding = 0,
                .format = VK_FORMAT_R32G32_SFLOAT,
                .offset = offsetof(ScreenRectVertex, tex_coord),
            },
        }};
    }
};

constexpr std::array<f32, 4 * 4> MakeOrthographicMatrix(f32 width, f32 height) {
    // clang-format off
    return { 2.f / width, 0.f,          0.f, 0.f,
             0.f,         2.f / height, 0.f, 0.f,
             0.f,         0.f,          1.f, 0.f,
            -1.f,        -1.f,          0.f, 1.f};
    // clang-format on
}

u32 GetBytesPerPixel(const Tegra::FramebufferConfig& framebuffer) {
    using namespace VideoCore::Surface;
    return BytesPerBlock(PixelFormatFromGPUPixelFormat(framebuffer.pixel_format));
}

std::size_t GetSizeInBytes(const Tegra::FramebufferConfig& framebuffer) {
    return static_cast<std::size_t>(framebuffer.stride) *
           static_cast<std::size_t>(framebuffer.height) * GetBytesPerPixel(framebuffer);
}

VkFormat GetFormat(const Tegra::FramebufferConfig& framebuffer) {
    switch (framebuffer.pixel_format) {
    case Tegra::FramebufferConfig::PixelFormat::A8B8G8R8_UNORM:
        return VK_FORMAT_A8B8G8R8_UNORM_PACK32;
    case Tegra::FramebufferConfig::PixelFormat::RGB565_UNORM:
        return VK_FORMAT_R5G6B5_UNORM_PACK16;
    default:
        UNIMPLEMENTED_MSG("Unknown framebuffer pixel format: {}",
                          static_cast<u32>(framebuffer.pixel_format));
        return VK_FORMAT_A8B8G8R8_UNORM_PACK32;
    }
}

} // Anonymous namespace

struct VKBlitScreen::BufferData {
    struct {
        std::array<f32, 4 * 4> modelview_matrix;
    } uniform;

    std::array<ScreenRectVertex, 4> vertices;

    // Unaligned image data goes here
};

VKBlitScreen::VKBlitScreen(Core::Memory::Memory& cpu_memory_,
                           Core::Frontend::EmuWindow& render_window_, const Device& device_,
                           MemoryAllocator& memory_allocator_, VKSwapchain& swapchain_,
                           VKScheduler& scheduler_, const VKScreenInfo& screen_info_)
    : cpu_memory{cpu_memory_}, render_window{render_window_}, device{device_},
      memory_allocator{memory_allocator_}, swapchain{swapchain_}, scheduler{scheduler_},
      image_count{swapchain.GetImageCount()}, screen_info{screen_info_} {
    resource_ticks.resize(image_count);

    CreateStaticResources();
    CreateDynamicResources();
}

VKBlitScreen::~VKBlitScreen() = default;

void VKBlitScreen::Recreate() {
    CreateDynamicResources();
}

VkSemaphore VKBlitScreen::Draw(const Tegra::FramebufferConfig& framebuffer, bool use_accelerated) {
    RefreshResources(framebuffer);

    // Finish any pending renderpass
    scheduler.RequestOutsideRenderPassOperationContext();

    const std::size_t image_index = swapchain.GetImageIndex();

    scheduler.Wait(resource_ticks[image_index]);
    resource_ticks[image_index] = scheduler.CurrentTick();

    UpdateDescriptorSet(image_index,
                        use_accelerated ? screen_info.image_view : *raw_image_views[image_index]);

    BufferData data;
    SetUniformData(data, framebuffer);
    SetVertexData(data, framebuffer);

    const std::span<u8> mapped_span = buffer_commit.Map();
    std::memcpy(mapped_span.data(), &data, sizeof(data));

    if (!use_accelerated) {
        const u64 image_offset = GetRawImageOffset(framebuffer, image_index);

        const VAddr framebuffer_addr = framebuffer.address + framebuffer.offset;
        const u8* const host_ptr = cpu_memory.GetPointer(framebuffer_addr);
        const size_t size_bytes = GetSizeInBytes(framebuffer);

        // TODO(Rodrigo): Read this from HLE
        constexpr u32 block_height_log2 = 4;
        const u32 bytes_per_pixel = GetBytesPerPixel(framebuffer);
        Tegra::Texture::UnswizzleTexture(
            mapped_span.subspan(image_offset, size_bytes), std::span(host_ptr, size_bytes),
            bytes_per_pixel, framebuffer.width, framebuffer.height, 1, block_height_log2, 0);

        const VkBufferImageCopy copy{
            .bufferOffset = image_offset,
            .bufferRowLength = 0,
            .bufferImageHeight = 0,
            .imageSubresource =
                {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .mipLevel = 0,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
            .imageOffset = {.x = 0, .y = 0, .z = 0},
            .imageExtent =
                {
                    .width = framebuffer.width,
                    .height = framebuffer.height,
                    .depth = 1,
                },
        };
        scheduler.Record([this, copy, image_index](vk::CommandBuffer cmdbuf) {
            const VkImage image = *raw_images[image_index];
            const VkImageMemoryBarrier base_barrier{
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .pNext = nullptr,
                .srcAccessMask = 0,
                .dstAccessMask = 0,
                .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
                .newLayout = VK_IMAGE_LAYOUT_GENERAL,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = image,
                .subresourceRange{
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
            };
            VkImageMemoryBarrier read_barrier = base_barrier;
            read_barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
            read_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            read_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;

            VkImageMemoryBarrier write_barrier = base_barrier;
            write_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            write_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            cmdbuf.PipelineBarrier(VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                                   read_barrier);
            cmdbuf.CopyBufferToImage(*buffer, image, VK_IMAGE_LAYOUT_GENERAL, copy);
            cmdbuf.PipelineBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT,
                                   VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, write_barrier);
        });
    }
    scheduler.Record([this, image_index, size = swapchain.GetSize()](vk::CommandBuffer cmdbuf) {
        const VkClearValue clear_color{
            .color = {.float32 = {0.0f, 0.0f, 0.0f, 0.0f}},
        };
        const VkRenderPassBeginInfo renderpass_bi{
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .pNext = nullptr,
            .renderPass = *renderpass,
            .framebuffer = *framebuffers[image_index],
            .renderArea =
                {
                    .offset = {0, 0},
                    .extent = size,
                },
            .clearValueCount = 1,
            .pClearValues = &clear_color,
        };
        const VkViewport viewport{
            .x = 0.0f,
            .y = 0.0f,
            .width = static_cast<float>(size.width),
            .height = static_cast<float>(size.height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
        };
        const VkRect2D scissor{
            .offset = {0, 0},
            .extent = size,
        };
        cmdbuf.BeginRenderPass(renderpass_bi, VK_SUBPASS_CONTENTS_INLINE);
        cmdbuf.BindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
        cmdbuf.SetViewport(0, viewport);
        cmdbuf.SetScissor(0, scissor);

        cmdbuf.BindVertexBuffer(0, *buffer, offsetof(BufferData, vertices));
        cmdbuf.BindDescriptorSets(VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline_layout, 0,
                                  descriptor_sets[image_index], {});
        cmdbuf.Draw(4, 1, 0, 0);
        cmdbuf.EndRenderPass();
    });
    return *semaphores[image_index];
}

void VKBlitScreen::CreateStaticResources() {
    CreateShaders();
    CreateSemaphores();
    CreateDescriptorPool();
    CreateDescriptorSetLayout();
    CreateDescriptorSets();
    CreatePipelineLayout();
    CreateSampler();
}

void VKBlitScreen::CreateDynamicResources() {
    CreateRenderPass();
    CreateFramebuffers();
    CreateGraphicsPipeline();
}

void VKBlitScreen::RefreshResources(const Tegra::FramebufferConfig& framebuffer) {
    if (framebuffer.width == raw_width && framebuffer.height == raw_height && !raw_images.empty()) {
        return;
    }
    raw_width = framebuffer.width;
    raw_height = framebuffer.height;
    ReleaseRawImages();

    CreateStagingBuffer(framebuffer);
    CreateRawImages(framebuffer);
}

void VKBlitScreen::CreateShaders() {
    vertex_shader = BuildShader(device, VULKAN_PRESENT_VERT_SPV);
    fragment_shader = BuildShader(device, VULKAN_PRESENT_FRAG_SPV);
}

void VKBlitScreen::CreateSemaphores() {
    semaphores.resize(image_count);
    std::ranges::generate(semaphores, [this] { return device.GetLogical().CreateSemaphore(); });
}

void VKBlitScreen::CreateDescriptorPool() {
    const std::array<VkDescriptorPoolSize, 2> pool_sizes{{
        {
            .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = static_cast<u32>(image_count),
        },
        {
            .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = static_cast<u32>(image_count),
        },
    }};

    const VkDescriptorPoolCreateInfo ci{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = static_cast<u32>(image_count),
        .poolSizeCount = static_cast<u32>(pool_sizes.size()),
        .pPoolSizes = pool_sizes.data(),
    };
    descriptor_pool = device.GetLogical().CreateDescriptorPool(ci);
}

void VKBlitScreen::CreateRenderPass() {
    const VkAttachmentDescription color_attachment{
        .flags = 0,
        .format = swapchain.GetImageFormat(),
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    };

    const VkAttachmentReference color_attachment_ref{
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_GENERAL,
    };

    const VkSubpassDescription subpass_description{
        .flags = 0,
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .inputAttachmentCount = 0,
        .pInputAttachments = nullptr,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_attachment_ref,
        .pResolveAttachments = nullptr,
        .pDepthStencilAttachment = nullptr,
        .preserveAttachmentCount = 0,
        .pPreserveAttachments = nullptr,
    };

    const VkSubpassDependency dependency{
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dependencyFlags = 0,
    };

    const VkRenderPassCreateInfo renderpass_ci{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .attachmentCount = 1,
        .pAttachments = &color_attachment,
        .subpassCount = 1,
        .pSubpasses = &subpass_description,
        .dependencyCount = 1,
        .pDependencies = &dependency,
    };

    renderpass = device.GetLogical().CreateRenderPass(renderpass_ci);
}

void VKBlitScreen::CreateDescriptorSetLayout() {
    const std::array<VkDescriptorSetLayoutBinding, 2> layout_bindings{{
        {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            .pImmutableSamplers = nullptr,
        },
        {
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = nullptr,
        },
    }};

    const VkDescriptorSetLayoutCreateInfo ci{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .bindingCount = static_cast<u32>(layout_bindings.size()),
        .pBindings = layout_bindings.data(),
    };

    descriptor_set_layout = device.GetLogical().CreateDescriptorSetLayout(ci);
}

void VKBlitScreen::CreateDescriptorSets() {
    const std::vector layouts(image_count, *descriptor_set_layout);

    const VkDescriptorSetAllocateInfo ai{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = nullptr,
        .descriptorPool = *descriptor_pool,
        .descriptorSetCount = static_cast<u32>(image_count),
        .pSetLayouts = layouts.data(),
    };

    descriptor_sets = descriptor_pool.Allocate(ai);
}

void VKBlitScreen::CreatePipelineLayout() {
    const VkPipelineLayoutCreateInfo ci{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .setLayoutCount = 1,
        .pSetLayouts = descriptor_set_layout.address(),
        .pushConstantRangeCount = 0,
        .pPushConstantRanges = nullptr,
    };
    pipeline_layout = device.GetLogical().CreatePipelineLayout(ci);
}

void VKBlitScreen::CreateGraphicsPipeline() {
    const std::array<VkPipelineShaderStageCreateInfo, 2> shader_stages{{
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = *vertex_shader,
            .pName = "main",
            .pSpecializationInfo = nullptr,
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = *fragment_shader,
            .pName = "main",
            .pSpecializationInfo = nullptr,
        },
    }};

    const auto vertex_binding_description = ScreenRectVertex::GetDescription();
    const auto vertex_attrs_description = ScreenRectVertex::GetAttributes();

    const VkPipelineVertexInputStateCreateInfo vertex_input_ci{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &vertex_binding_description,
        .vertexAttributeDescriptionCount = u32{vertex_attrs_description.size()},
        .pVertexAttributeDescriptions = vertex_attrs_description.data(),
    };

    const VkPipelineInputAssemblyStateCreateInfo input_assembly_ci{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
        .primitiveRestartEnable = VK_FALSE,
    };

    const VkPipelineViewportStateCreateInfo viewport_state_ci{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .viewportCount = 1,
        .pViewports = nullptr,
        .scissorCount = 1,
        .pScissors = nullptr,
    };

    const VkPipelineRasterizationStateCreateInfo rasterization_ci{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_CLOCKWISE,
        .depthBiasEnable = VK_FALSE,
        .depthBiasConstantFactor = 0.0f,
        .depthBiasClamp = 0.0f,
        .depthBiasSlopeFactor = 0.0f,
        .lineWidth = 1.0f,
    };

    const VkPipelineMultisampleStateCreateInfo multisampling_ci{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable = VK_FALSE,
        .minSampleShading = 0.0f,
        .pSampleMask = nullptr,
        .alphaToCoverageEnable = VK_FALSE,
        .alphaToOneEnable = VK_FALSE,
    };

    const VkPipelineColorBlendAttachmentState color_blend_attachment{
        .blendEnable = VK_FALSE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_ZERO,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };

    const VkPipelineColorBlendStateCreateInfo color_blend_ci{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_COPY,
        .attachmentCount = 1,
        .pAttachments = &color_blend_attachment,
        .blendConstants = {0.0f, 0.0f, 0.0f, 0.0f},
    };

    static constexpr std::array dynamic_states{
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };
    const VkPipelineDynamicStateCreateInfo dynamic_state_ci{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .dynamicStateCount = static_cast<u32>(dynamic_states.size()),
        .pDynamicStates = dynamic_states.data(),
    };

    const VkGraphicsPipelineCreateInfo pipeline_ci{
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .stageCount = static_cast<u32>(shader_stages.size()),
        .pStages = shader_stages.data(),
        .pVertexInputState = &vertex_input_ci,
        .pInputAssemblyState = &input_assembly_ci,
        .pTessellationState = nullptr,
        .pViewportState = &viewport_state_ci,
        .pRasterizationState = &rasterization_ci,
        .pMultisampleState = &multisampling_ci,
        .pDepthStencilState = nullptr,
        .pColorBlendState = &color_blend_ci,
        .pDynamicState = &dynamic_state_ci,
        .layout = *pipeline_layout,
        .renderPass = *renderpass,
        .subpass = 0,
        .basePipelineHandle = 0,
        .basePipelineIndex = 0,
    };

    pipeline = device.GetLogical().CreateGraphicsPipeline(pipeline_ci);
}

void VKBlitScreen::CreateSampler() {
    const VkSamplerCreateInfo ci{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_NEAREST,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
        .mipLodBias = 0.0f,
        .anisotropyEnable = VK_FALSE,
        .maxAnisotropy = 0.0f,
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_NEVER,
        .minLod = 0.0f,
        .maxLod = 0.0f,
        .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE,
    };

    sampler = device.GetLogical().CreateSampler(ci);
}

void VKBlitScreen::CreateFramebuffers() {
    const VkExtent2D size{swapchain.GetSize()};
    framebuffers.resize(image_count);

    VkFramebufferCreateInfo ci{
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .renderPass = *renderpass,
        .attachmentCount = 1,
        .pAttachments = nullptr,
        .width = size.width,
        .height = size.height,
        .layers = 1,
    };

    for (std::size_t i = 0; i < image_count; ++i) {
        const VkImageView image_view{swapchain.GetImageViewIndex(i)};
        ci.pAttachments = &image_view;
        framebuffers[i] = device.GetLogical().CreateFramebuffer(ci);
    }
}

void VKBlitScreen::ReleaseRawImages() {
    for (const u64 tick : resource_ticks) {
        scheduler.Wait(tick);
    }
    raw_images.clear();
    raw_buffer_commits.clear();
    buffer.reset();
    buffer_commit = MemoryCommit{};
}

void VKBlitScreen::CreateStagingBuffer(const Tegra::FramebufferConfig& framebuffer) {
    const VkBufferCreateInfo ci{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .size = CalculateBufferSize(framebuffer),
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                 VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
    };

    buffer = device.GetLogical().CreateBuffer(ci);
    buffer_commit = memory_allocator.Commit(buffer, MemoryUsage::Upload);
}

void VKBlitScreen::CreateRawImages(const Tegra::FramebufferConfig& framebuffer) {
    raw_images.resize(image_count);
    raw_image_views.resize(image_count);
    raw_buffer_commits.resize(image_count);

    for (size_t i = 0; i < image_count; ++i) {
        raw_images[i] = device.GetLogical().CreateImage(VkImageCreateInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = GetFormat(framebuffer),
            .extent =
                {
                    .width = framebuffer.width,
                    .height = framebuffer.height,
                    .depth = 1,
                },
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_LINEAR,
            .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = nullptr,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        });
        raw_buffer_commits[i] = memory_allocator.Commit(raw_images[i], MemoryUsage::DeviceLocal);
        raw_image_views[i] = device.GetLogical().CreateImageView(VkImageViewCreateInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .image = *raw_images[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = GetFormat(framebuffer),
            .components =
                {
                    .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .a = VK_COMPONENT_SWIZZLE_IDENTITY,
                },
            .subresourceRange =
                {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
        });
    }
}

void VKBlitScreen::UpdateDescriptorSet(std::size_t image_index, VkImageView image_view) const {
    const VkDescriptorBufferInfo buffer_info{
        .buffer = *buffer,
        .offset = offsetof(BufferData, uniform),
        .range = sizeof(BufferData::uniform),
    };

    const VkWriteDescriptorSet ubo_write{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,
        .dstSet = descriptor_sets[image_index],
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .pImageInfo = nullptr,
        .pBufferInfo = &buffer_info,
        .pTexelBufferView = nullptr,
    };

    const VkDescriptorImageInfo image_info{
        .sampler = *sampler,
        .imageView = image_view,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
    };

    const VkWriteDescriptorSet sampler_write{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,
        .dstSet = descriptor_sets[image_index],
        .dstBinding = 1,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = &image_info,
        .pBufferInfo = nullptr,
        .pTexelBufferView = nullptr,
    };

    device.GetLogical().UpdateDescriptorSets(std::array{ubo_write, sampler_write}, {});
}

void VKBlitScreen::SetUniformData(BufferData& data,
                                  const Tegra::FramebufferConfig& framebuffer) const {
    const auto& layout = render_window.GetFramebufferLayout();
    data.uniform.modelview_matrix =
        MakeOrthographicMatrix(static_cast<f32>(layout.width), static_cast<f32>(layout.height));
}

void VKBlitScreen::SetVertexData(BufferData& data,
                                 const Tegra::FramebufferConfig& framebuffer) const {
    const auto& framebuffer_transform_flags = framebuffer.transform_flags;
    const auto& framebuffer_crop_rect = framebuffer.crop_rect;

    static constexpr Common::Rectangle<f32> texcoords{0.f, 0.f, 1.f, 1.f};
    auto left = texcoords.left;
    auto right = texcoords.right;

    switch (framebuffer_transform_flags) {
    case Tegra::FramebufferConfig::TransformFlags::Unset:
        break;
    case Tegra::FramebufferConfig::TransformFlags::FlipV:
        // Flip the framebuffer vertically
        left = texcoords.right;
        right = texcoords.left;
        break;
    default:
        UNIMPLEMENTED_MSG("Unsupported framebuffer_transform_flags={}",
                          static_cast<u32>(framebuffer_transform_flags));
        break;
    }

    UNIMPLEMENTED_IF(framebuffer_crop_rect.top != 0);
    UNIMPLEMENTED_IF(framebuffer_crop_rect.left != 0);

    // Scale the output by the crop width/height. This is commonly used with 1280x720 rendering
    // (e.g. handheld mode) on a 1920x1080 framebuffer.
    f32 scale_u = 1.0f;
    f32 scale_v = 1.0f;
    if (framebuffer_crop_rect.GetWidth() > 0) {
        scale_u = static_cast<f32>(framebuffer_crop_rect.GetWidth()) /
                  static_cast<f32>(screen_info.width);
    }
    if (framebuffer_crop_rect.GetHeight() > 0) {
        scale_v = static_cast<f32>(framebuffer_crop_rect.GetHeight()) /
                  static_cast<f32>(screen_info.height);
    }

    const auto& screen = render_window.GetFramebufferLayout().screen;
    const auto x = static_cast<f32>(screen.left);
    const auto y = static_cast<f32>(screen.top);
    const auto w = static_cast<f32>(screen.GetWidth());
    const auto h = static_cast<f32>(screen.GetHeight());
    data.vertices[0] = ScreenRectVertex(x, y, texcoords.top * scale_u, left * scale_v);
    data.vertices[1] = ScreenRectVertex(x + w, y, texcoords.bottom * scale_u, left * scale_v);
    data.vertices[2] = ScreenRectVertex(x, y + h, texcoords.top * scale_u, right * scale_v);
    data.vertices[3] = ScreenRectVertex(x + w, y + h, texcoords.bottom * scale_u, right * scale_v);
}

u64 VKBlitScreen::CalculateBufferSize(const Tegra::FramebufferConfig& framebuffer) const {
    return sizeof(BufferData) + GetSizeInBytes(framebuffer) * image_count;
}

u64 VKBlitScreen::GetRawImageOffset(const Tegra::FramebufferConfig& framebuffer,
                                    std::size_t image_index) const {
    constexpr auto first_image_offset = static_cast<u64>(sizeof(BufferData));
    return first_image_offset + GetSizeInBytes(framebuffer) * image_index;
}

} // namespace Vulkan
