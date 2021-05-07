// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

#include "video_core/renderer_vulkan/vk_query_cache.h"
#include "video_core/renderer_vulkan/vk_resource_pool.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/vulkan_common/vulkan_device.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Vulkan {

using VideoCore::QueryType;

namespace {

constexpr std::array QUERY_TARGETS = {VK_QUERY_TYPE_OCCLUSION};

constexpr VkQueryType GetTarget(QueryType type) {
    return QUERY_TARGETS[static_cast<std::size_t>(type)];
}

} // Anonymous namespace

QueryPool::QueryPool(const Device& device_, VKScheduler& scheduler, QueryType type_)
    : ResourcePool{scheduler.GetMasterSemaphore(), GROW_STEP}, device{device_}, type{type_} {}

QueryPool::~QueryPool() = default;

std::pair<VkQueryPool, u32> QueryPool::Commit() {
    std::size_t index;
    do {
        index = CommitResource();
    } while (usage[index]);
    usage[index] = true;

    return {*pools[index / GROW_STEP], static_cast<u32>(index % GROW_STEP)};
}

void QueryPool::Allocate(std::size_t begin, std::size_t end) {
    usage.resize(end);

    pools.push_back(device.GetLogical().CreateQueryPool({
        .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .queryType = GetTarget(type),
        .queryCount = static_cast<u32>(end - begin),
        .pipelineStatistics = 0,
    }));
}

void QueryPool::Reserve(std::pair<VkQueryPool, u32> query) {
    const auto it =
        std::find_if(pools.begin(), pools.end(), [query_pool = query.first](vk::QueryPool& pool) {
            return query_pool == *pool;
        });
    ASSERT(it != std::end(pools));

    const std::ptrdiff_t pool_index = std::distance(std::begin(pools), it);
    usage[pool_index * GROW_STEP + static_cast<std::ptrdiff_t>(query.second)] = false;
}

VKQueryCache::VKQueryCache(VideoCore::RasterizerInterface& rasterizer_,
                           Tegra::Engines::Maxwell3D& maxwell3d_, Tegra::MemoryManager& gpu_memory_,
                           const Device& device_, VKScheduler& scheduler_)
    : QueryCacheBase{rasterizer_, maxwell3d_, gpu_memory_}, device{device_}, scheduler{scheduler_},
      query_pools{
          QueryPool{device_, scheduler_, QueryType::SamplesPassed},
      } {}

VKQueryCache::~VKQueryCache() {
    // TODO(Rodrigo): This is a hack to destroy all HostCounter instances before the base class
    // destructor is called. The query cache should be redesigned to have a proper ownership model
    // instead of using shared pointers.
    for (size_t query_type = 0; query_type < VideoCore::NumQueryTypes; ++query_type) {
        auto& stream = Stream(static_cast<QueryType>(query_type));
        stream.Update(false);
        stream.Reset();
    }
}

std::pair<VkQueryPool, u32> VKQueryCache::AllocateQuery(QueryType type) {
    return query_pools[static_cast<std::size_t>(type)].Commit();
}

void VKQueryCache::Reserve(QueryType type, std::pair<VkQueryPool, u32> query) {
    query_pools[static_cast<std::size_t>(type)].Reserve(query);
}

HostCounter::HostCounter(VKQueryCache& cache_, std::shared_ptr<HostCounter> dependency_,
                         QueryType type_)
    : HostCounterBase{std::move(dependency_)}, cache{cache_}, type{type_},
      query{cache_.AllocateQuery(type_)}, tick{cache_.GetScheduler().CurrentTick()} {
    const vk::Device* logical = &cache.GetDevice().GetLogical();
    cache.GetScheduler().Record([logical, query = query](vk::CommandBuffer cmdbuf) {
        logical->ResetQueryPoolEXT(query.first, query.second, 1);
        cmdbuf.BeginQuery(query.first, query.second, VK_QUERY_CONTROL_PRECISE_BIT);
    });
}

HostCounter::~HostCounter() {
    cache.Reserve(type, query);
}

void HostCounter::EndQuery() {
    cache.GetScheduler().Record(
        [query = query](vk::CommandBuffer cmdbuf) { cmdbuf.EndQuery(query.first, query.second); });
}

u64 HostCounter::BlockingQuery() const {
    cache.GetScheduler().Wait(tick);
    u64 data;
    const VkResult query_result = cache.GetDevice().GetLogical().GetQueryResults(
        query.first, query.second, 1, sizeof(data), &data, sizeof(data), VK_QUERY_RESULT_64_BIT);

    switch (query_result) {
    case VK_SUCCESS:
        return data;
    case VK_ERROR_DEVICE_LOST:
        cache.GetDevice().ReportLoss();
        [[fallthrough]];
    default:
        throw vk::Exception(query_result);
    }
}

} // namespace Vulkan
