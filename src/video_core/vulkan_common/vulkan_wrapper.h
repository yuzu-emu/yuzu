// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <exception>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

#define VK_NO_PROTOTYPES
#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#endif
#include <vulkan/vulkan.h>

// Sanitize macros
#ifdef CreateEvent
#undef CreateEvent
#endif
#ifdef CreateSemaphore
#undef CreateSemaphore
#endif

#include "common/common_types.h"

#ifdef _MSC_VER
#pragma warning(disable : 26812) // Disable prefer enum class over enum
#endif

namespace Vulkan::vk {

/**
 * Span for Vulkan arrays.
 * Based on std::span but optimized for array access instead of iterators.
 * Size returns uint32_t instead of size_t to ease interaction with Vulkan functions.
 */
template <typename T>
class Span {
public:
    using value_type = T;
    using size_type = u32;
    using difference_type = std::ptrdiff_t;
    using reference = const T&;
    using const_reference = const T&;
    using pointer = const T*;
    using const_pointer = const T*;
    using iterator = const T*;
    using const_iterator = const T*;

    /// Construct an empty span.
    constexpr Span() noexcept = default;

    /// Construct an empty span
    constexpr Span(std::nullptr_t) noexcept {}

    /// Construct a span from a single element.
    constexpr Span(const T& value) noexcept : ptr{&value}, num{1} {}

    /// Construct a span from a range.
    template <typename Range>
    // requires std::data(const Range&)
    // requires std::size(const Range&)
    constexpr Span(const Range& range) : ptr{std::data(range)}, num{std::size(range)} {}

    /// Construct a span from a pointer and a size.
    /// This is inteded for subranges.
    constexpr Span(const T* ptr_, std::size_t num_) noexcept : ptr{ptr_}, num{num_} {}

    /// Returns the data pointer by the span.
    constexpr const T* data() const noexcept {
        return ptr;
    }

    /// Returns the number of elements in the span.
    /// @note Returns a 32 bits integer because most Vulkan functions expect this type.
    constexpr u32 size() const noexcept {
        return static_cast<u32>(num);
    }

    /// Returns true when the span is empty.
    constexpr bool empty() const noexcept {
        return num == 0;
    }

    /// Returns a reference to the element in the passed index.
    /// @pre: index < size()
    constexpr const T& operator[](std::size_t index) const noexcept {
        return ptr[index];
    }

    /// Returns an iterator to the beginning of the span.
    constexpr const T* begin() const noexcept {
        return ptr;
    }

    /// Returns an iterator to the end of the span.
    constexpr const T* end() const noexcept {
        return ptr + num;
    }

    /// Returns an iterator to the beginning of the span.
    constexpr const T* cbegin() const noexcept {
        return ptr;
    }

    /// Returns an iterator to the end of the span.
    constexpr const T* cend() const noexcept {
        return ptr + num;
    }

private:
    const T* ptr = nullptr;
    std::size_t num = 0;
};

/// Vulkan exception generated from a VkResult.
class Exception final : public std::exception {
public:
    /// Construct the exception with a result.
    /// @pre result != VK_SUCCESS
    explicit Exception(VkResult result_) : result{result_} {}
    virtual ~Exception() = default;

    const char* what() const noexcept override;

private:
    VkResult result;
};

/// Converts a VkResult enum into a rodata string
const char* ToString(VkResult) noexcept;

/// Throws a Vulkan exception if result is not success.
inline void Check(VkResult result) {
    if (result != VK_SUCCESS) {
        throw Exception(result);
    }
}

/// Throws a Vulkan exception if result is an error.
/// @return result
inline VkResult Filter(VkResult result) {
    if (result < 0) {
        throw Exception(result);
    }
    return result;
}

/// Table holding Vulkan instance function pointers.
struct InstanceDispatch {
    PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr{};

    PFN_vkCreateInstance vkCreateInstance{};
    PFN_vkDestroyInstance vkDestroyInstance{};
    PFN_vkEnumerateInstanceExtensionProperties vkEnumerateInstanceExtensionProperties{};
    PFN_vkEnumerateInstanceLayerProperties vkEnumerateInstanceLayerProperties{};

    PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT{};
    PFN_vkCreateDevice vkCreateDevice{};
    PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT{};
    PFN_vkDestroyDevice vkDestroyDevice{};
    PFN_vkDestroySurfaceKHR vkDestroySurfaceKHR{};
    PFN_vkEnumerateDeviceExtensionProperties vkEnumerateDeviceExtensionProperties{};
    PFN_vkEnumeratePhysicalDevices vkEnumeratePhysicalDevices{};
    PFN_vkGetDeviceProcAddr vkGetDeviceProcAddr{};
    PFN_vkGetPhysicalDeviceFeatures2KHR vkGetPhysicalDeviceFeatures2KHR{};
    PFN_vkGetPhysicalDeviceFormatProperties vkGetPhysicalDeviceFormatProperties{};
    PFN_vkGetPhysicalDeviceMemoryProperties vkGetPhysicalDeviceMemoryProperties{};
    PFN_vkGetPhysicalDeviceProperties vkGetPhysicalDeviceProperties{};
    PFN_vkGetPhysicalDeviceProperties2KHR vkGetPhysicalDeviceProperties2KHR{};
    PFN_vkGetPhysicalDeviceQueueFamilyProperties vkGetPhysicalDeviceQueueFamilyProperties{};
    PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR vkGetPhysicalDeviceSurfaceCapabilitiesKHR{};
    PFN_vkGetPhysicalDeviceSurfaceFormatsKHR vkGetPhysicalDeviceSurfaceFormatsKHR{};
    PFN_vkGetPhysicalDeviceSurfacePresentModesKHR vkGetPhysicalDeviceSurfacePresentModesKHR{};
    PFN_vkGetPhysicalDeviceSurfaceSupportKHR vkGetPhysicalDeviceSurfaceSupportKHR{};
    PFN_vkGetSwapchainImagesKHR vkGetSwapchainImagesKHR{};
    PFN_vkQueuePresentKHR vkQueuePresentKHR{};
};

/// Table holding Vulkan device function pointers.
struct DeviceDispatch : InstanceDispatch {
    PFN_vkAcquireNextImageKHR vkAcquireNextImageKHR{};
    PFN_vkAllocateCommandBuffers vkAllocateCommandBuffers{};
    PFN_vkAllocateDescriptorSets vkAllocateDescriptorSets{};
    PFN_vkAllocateMemory vkAllocateMemory{};
    PFN_vkBeginCommandBuffer vkBeginCommandBuffer{};
    PFN_vkBindBufferMemory vkBindBufferMemory{};
    PFN_vkBindImageMemory vkBindImageMemory{};
    PFN_vkCmdBeginDebugUtilsLabelEXT vkCmdBeginDebugUtilsLabelEXT{};
    PFN_vkCmdBeginQuery vkCmdBeginQuery{};
    PFN_vkCmdBeginRenderPass vkCmdBeginRenderPass{};
    PFN_vkCmdBeginTransformFeedbackEXT vkCmdBeginTransformFeedbackEXT{};
    PFN_vkCmdBindDescriptorSets vkCmdBindDescriptorSets{};
    PFN_vkCmdBindIndexBuffer vkCmdBindIndexBuffer{};
    PFN_vkCmdBindPipeline vkCmdBindPipeline{};
    PFN_vkCmdBindTransformFeedbackBuffersEXT vkCmdBindTransformFeedbackBuffersEXT{};
    PFN_vkCmdBindVertexBuffers vkCmdBindVertexBuffers{};
    PFN_vkCmdBindVertexBuffers2EXT vkCmdBindVertexBuffers2EXT{};
    PFN_vkCmdBlitImage vkCmdBlitImage{};
    PFN_vkCmdClearAttachments vkCmdClearAttachments{};
    PFN_vkCmdCopyBuffer vkCmdCopyBuffer{};
    PFN_vkCmdCopyBufferToImage vkCmdCopyBufferToImage{};
    PFN_vkCmdCopyImage vkCmdCopyImage{};
    PFN_vkCmdCopyImageToBuffer vkCmdCopyImageToBuffer{};
    PFN_vkCmdDispatch vkCmdDispatch{};
    PFN_vkCmdDraw vkCmdDraw{};
    PFN_vkCmdDrawIndexed vkCmdDrawIndexed{};
    PFN_vkCmdEndDebugUtilsLabelEXT vkCmdEndDebugUtilsLabelEXT{};
    PFN_vkCmdEndQuery vkCmdEndQuery{};
    PFN_vkCmdEndRenderPass vkCmdEndRenderPass{};
    PFN_vkCmdEndTransformFeedbackEXT vkCmdEndTransformFeedbackEXT{};
    PFN_vkCmdFillBuffer vkCmdFillBuffer{};
    PFN_vkCmdPipelineBarrier vkCmdPipelineBarrier{};
    PFN_vkCmdPushConstants vkCmdPushConstants{};
    PFN_vkCmdPushDescriptorSetWithTemplateKHR vkCmdPushDescriptorSetWithTemplateKHR{};
    PFN_vkCmdResolveImage vkCmdResolveImage{};
    PFN_vkCmdSetBlendConstants vkCmdSetBlendConstants{};
    PFN_vkCmdSetCullModeEXT vkCmdSetCullModeEXT{};
    PFN_vkCmdSetDepthBias vkCmdSetDepthBias{};
    PFN_vkCmdSetDepthBounds vkCmdSetDepthBounds{};
    PFN_vkCmdSetDepthBoundsTestEnableEXT vkCmdSetDepthBoundsTestEnableEXT{};
    PFN_vkCmdSetDepthCompareOpEXT vkCmdSetDepthCompareOpEXT{};
    PFN_vkCmdSetDepthTestEnableEXT vkCmdSetDepthTestEnableEXT{};
    PFN_vkCmdSetDepthWriteEnableEXT vkCmdSetDepthWriteEnableEXT{};
    PFN_vkCmdSetEvent vkCmdSetEvent{};
    PFN_vkCmdSetFrontFaceEXT vkCmdSetFrontFaceEXT{};
    PFN_vkCmdSetLineWidth vkCmdSetLineWidth{};
    PFN_vkCmdSetPrimitiveTopologyEXT vkCmdSetPrimitiveTopologyEXT{};
    PFN_vkCmdSetScissor vkCmdSetScissor{};
    PFN_vkCmdSetStencilCompareMask vkCmdSetStencilCompareMask{};
    PFN_vkCmdSetStencilOpEXT vkCmdSetStencilOpEXT{};
    PFN_vkCmdSetStencilReference vkCmdSetStencilReference{};
    PFN_vkCmdSetStencilTestEnableEXT vkCmdSetStencilTestEnableEXT{};
    PFN_vkCmdSetStencilWriteMask vkCmdSetStencilWriteMask{};
    PFN_vkCmdSetVertexInputEXT vkCmdSetVertexInputEXT{};
    PFN_vkCmdSetViewport vkCmdSetViewport{};
    PFN_vkCmdWaitEvents vkCmdWaitEvents{};
    PFN_vkCreateBuffer vkCreateBuffer{};
    PFN_vkCreateBufferView vkCreateBufferView{};
    PFN_vkCreateCommandPool vkCreateCommandPool{};
    PFN_vkCreateComputePipelines vkCreateComputePipelines{};
    PFN_vkCreateDescriptorPool vkCreateDescriptorPool{};
    PFN_vkCreateDescriptorSetLayout vkCreateDescriptorSetLayout{};
    PFN_vkCreateDescriptorUpdateTemplateKHR vkCreateDescriptorUpdateTemplateKHR{};
    PFN_vkCreateEvent vkCreateEvent{};
    PFN_vkCreateFence vkCreateFence{};
    PFN_vkCreateFramebuffer vkCreateFramebuffer{};
    PFN_vkCreateGraphicsPipelines vkCreateGraphicsPipelines{};
    PFN_vkCreateImage vkCreateImage{};
    PFN_vkCreateImageView vkCreateImageView{};
    PFN_vkCreatePipelineLayout vkCreatePipelineLayout{};
    PFN_vkCreateQueryPool vkCreateQueryPool{};
    PFN_vkCreateRenderPass vkCreateRenderPass{};
    PFN_vkCreateSampler vkCreateSampler{};
    PFN_vkCreateSemaphore vkCreateSemaphore{};
    PFN_vkCreateShaderModule vkCreateShaderModule{};
    PFN_vkCreateSwapchainKHR vkCreateSwapchainKHR{};
    PFN_vkDestroyBuffer vkDestroyBuffer{};
    PFN_vkDestroyBufferView vkDestroyBufferView{};
    PFN_vkDestroyCommandPool vkDestroyCommandPool{};
    PFN_vkDestroyDescriptorPool vkDestroyDescriptorPool{};
    PFN_vkDestroyDescriptorSetLayout vkDestroyDescriptorSetLayout{};
    PFN_vkDestroyDescriptorUpdateTemplateKHR vkDestroyDescriptorUpdateTemplateKHR{};
    PFN_vkDestroyEvent vkDestroyEvent{};
    PFN_vkDestroyFence vkDestroyFence{};
    PFN_vkDestroyFramebuffer vkDestroyFramebuffer{};
    PFN_vkDestroyImage vkDestroyImage{};
    PFN_vkDestroyImageView vkDestroyImageView{};
    PFN_vkDestroyPipeline vkDestroyPipeline{};
    PFN_vkDestroyPipelineLayout vkDestroyPipelineLayout{};
    PFN_vkDestroyQueryPool vkDestroyQueryPool{};
    PFN_vkDestroyRenderPass vkDestroyRenderPass{};
    PFN_vkDestroySampler vkDestroySampler{};
    PFN_vkDestroySemaphore vkDestroySemaphore{};
    PFN_vkDestroyShaderModule vkDestroyShaderModule{};
    PFN_vkDestroySwapchainKHR vkDestroySwapchainKHR{};
    PFN_vkDeviceWaitIdle vkDeviceWaitIdle{};
    PFN_vkEndCommandBuffer vkEndCommandBuffer{};
    PFN_vkFreeCommandBuffers vkFreeCommandBuffers{};
    PFN_vkFreeDescriptorSets vkFreeDescriptorSets{};
    PFN_vkFreeMemory vkFreeMemory{};
    PFN_vkGetBufferMemoryRequirements2 vkGetBufferMemoryRequirements2{};
    PFN_vkGetDeviceQueue vkGetDeviceQueue{};
    PFN_vkGetEventStatus vkGetEventStatus{};
    PFN_vkGetFenceStatus vkGetFenceStatus{};
    PFN_vkGetImageMemoryRequirements vkGetImageMemoryRequirements{};
    PFN_vkGetMemoryFdKHR vkGetMemoryFdKHR{};
#ifdef _WIN32
    PFN_vkGetMemoryWin32HandleKHR vkGetMemoryWin32HandleKHR{};
#endif
    PFN_vkGetQueryPoolResults vkGetQueryPoolResults{};
    PFN_vkGetSemaphoreCounterValueKHR vkGetSemaphoreCounterValueKHR{};
    PFN_vkMapMemory vkMapMemory{};
    PFN_vkQueueSubmit vkQueueSubmit{};
    PFN_vkResetFences vkResetFences{};
    PFN_vkResetQueryPoolEXT vkResetQueryPoolEXT{};
    PFN_vkSetDebugUtilsObjectNameEXT vkSetDebugUtilsObjectNameEXT{};
    PFN_vkSetDebugUtilsObjectTagEXT vkSetDebugUtilsObjectTagEXT{};
    PFN_vkUnmapMemory vkUnmapMemory{};
    PFN_vkUpdateDescriptorSetWithTemplateKHR vkUpdateDescriptorSetWithTemplateKHR{};
    PFN_vkUpdateDescriptorSets vkUpdateDescriptorSets{};
    PFN_vkWaitForFences vkWaitForFences{};
    PFN_vkWaitSemaphoresKHR vkWaitSemaphoresKHR{};
};

/// Loads instance agnostic function pointers.
/// @return True on success, false on error.
bool Load(InstanceDispatch&) noexcept;

/// Loads instance function pointers.
/// @return True on success, false on error.
bool Load(VkInstance, InstanceDispatch&) noexcept;

void Destroy(VkInstance, const InstanceDispatch&) noexcept;
void Destroy(VkDevice, const InstanceDispatch&) noexcept;

void Destroy(VkDevice, VkBuffer, const DeviceDispatch&) noexcept;
void Destroy(VkDevice, VkBufferView, const DeviceDispatch&) noexcept;
void Destroy(VkDevice, VkCommandPool, const DeviceDispatch&) noexcept;
void Destroy(VkDevice, VkDescriptorPool, const DeviceDispatch&) noexcept;
void Destroy(VkDevice, VkDescriptorSetLayout, const DeviceDispatch&) noexcept;
void Destroy(VkDevice, VkDescriptorUpdateTemplateKHR, const DeviceDispatch&) noexcept;
void Destroy(VkDevice, VkDeviceMemory, const DeviceDispatch&) noexcept;
void Destroy(VkDevice, VkEvent, const DeviceDispatch&) noexcept;
void Destroy(VkDevice, VkFence, const DeviceDispatch&) noexcept;
void Destroy(VkDevice, VkFramebuffer, const DeviceDispatch&) noexcept;
void Destroy(VkDevice, VkImage, const DeviceDispatch&) noexcept;
void Destroy(VkDevice, VkImageView, const DeviceDispatch&) noexcept;
void Destroy(VkDevice, VkPipeline, const DeviceDispatch&) noexcept;
void Destroy(VkDevice, VkPipelineLayout, const DeviceDispatch&) noexcept;
void Destroy(VkDevice, VkQueryPool, const DeviceDispatch&) noexcept;
void Destroy(VkDevice, VkRenderPass, const DeviceDispatch&) noexcept;
void Destroy(VkDevice, VkSampler, const DeviceDispatch&) noexcept;
void Destroy(VkDevice, VkSwapchainKHR, const DeviceDispatch&) noexcept;
void Destroy(VkDevice, VkSemaphore, const DeviceDispatch&) noexcept;
void Destroy(VkDevice, VkShaderModule, const DeviceDispatch&) noexcept;
void Destroy(VkInstance, VkDebugUtilsMessengerEXT, const InstanceDispatch&) noexcept;
void Destroy(VkInstance, VkSurfaceKHR, const InstanceDispatch&) noexcept;

VkResult Free(VkDevice, VkDescriptorPool, Span<VkDescriptorSet>, const DeviceDispatch&) noexcept;
VkResult Free(VkDevice, VkCommandPool, Span<VkCommandBuffer>, const DeviceDispatch&) noexcept;

template <typename Type, typename OwnerType, typename Dispatch>
class Handle;

/// Handle with an owning type.
/// Analogue to std::unique_ptr.
template <typename Type, typename OwnerType, typename Dispatch>
class Handle {
public:
    /// Construct a handle and hold it's ownership.
    explicit Handle(Type handle_, OwnerType owner_, const Dispatch& dld_) noexcept
        : handle{handle_}, owner{owner_}, dld{&dld_} {}

    /// Construct an empty handle.
    Handle() = default;

    /// Construct an empty handle.
    Handle(std::nullptr_t) {}

    /// Copying Vulkan objects is not supported and will never be.
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;

    /// Construct a handle transfering the ownership from another handle.
    Handle(Handle&& rhs) noexcept
        : handle{std::exchange(rhs.handle, nullptr)}, owner{rhs.owner}, dld{rhs.dld} {}

    /// Assign the current handle transfering the ownership from another handle.
    /// Destroys any previously held object.
    Handle& operator=(Handle&& rhs) noexcept {
        Release();
        handle = std::exchange(rhs.handle, nullptr);
        owner = rhs.owner;
        dld = rhs.dld;
        return *this;
    }

    /// Destroys the current handle if it existed.
    ~Handle() noexcept {
        Release();
    }

    /// Destroys any held object.
    void reset() noexcept {
        Release();
        handle = nullptr;
    }

    /// Returns the address of the held object.
    /// Intended for Vulkan structures that expect a pointer to an array.
    const Type* address() const noexcept {
        return std::addressof(handle);
    }

    /// Returns the held Vulkan handle.
    Type operator*() const noexcept {
        return handle;
    }

    /// Returns true when there's a held object.
    explicit operator bool() const noexcept {
        return handle != nullptr;
    }

protected:
    Type handle = nullptr;
    OwnerType owner = nullptr;
    const Dispatch* dld = nullptr;

private:
    /// Destroys the held object if it exists.
    void Release() noexcept {
        if (handle) {
            Destroy(owner, handle, *dld);
        }
    }
};

/// Dummy type used to specify a handle has no owner.
struct NoOwner {};

/// Handle without an owning type.
/// Analogue to std::unique_ptr
template <typename Type, typename Dispatch>
class Handle<Type, NoOwner, Dispatch> {
public:
    /// Construct a handle and hold it's ownership.
    explicit Handle(Type handle_, const Dispatch& dld_) noexcept : handle{handle_}, dld{&dld_} {}

    /// Construct an empty handle.
    Handle() noexcept = default;

    /// Copying Vulkan objects is not supported and will never be.
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;

    /// Construct a handle transfering ownership from another handle.
    Handle(Handle&& rhs) noexcept : handle{std::exchange(rhs.handle, nullptr)}, dld{rhs.dld} {}

    /// Assign the current handle transfering the ownership from another handle.
    /// Destroys any previously held object.
    Handle& operator=(Handle&& rhs) noexcept {
        Release();
        handle = std::exchange(rhs.handle, nullptr);
        dld = rhs.dld;
        return *this;
    }

    /// Destroys the current handle if it existed.
    ~Handle() noexcept {
        Release();
    }

    /// Destroys any held object.
    void reset() noexcept {
        Release();
        handle = nullptr;
    }

    /// Returns the address of the held object.
    /// Intended for Vulkan structures that expect a pointer to an array.
    const Type* address() const noexcept {
        return std::addressof(handle);
    }

    /// Returns the held Vulkan handle.
    Type operator*() const noexcept {
        return handle;
    }

    /// Returns true when there's a held object.
    operator bool() const noexcept {
        return handle != nullptr;
    }

protected:
    Type handle = nullptr;
    const Dispatch* dld = nullptr;

private:
    /// Destroys the held object if it exists.
    void Release() noexcept {
        if (handle) {
            Destroy(handle, *dld);
        }
    }
};

/// Array of a pool allocation.
/// Analogue to std::vector
template <typename AllocationType, typename PoolType>
class PoolAllocations {
public:
    /// Construct an empty allocation.
    PoolAllocations() = default;

    /// Construct an allocation. Errors are reported through IsOutOfPoolMemory().
    explicit PoolAllocations(std::unique_ptr<AllocationType[]> allocations_, std::size_t num_,
                             VkDevice device_, PoolType pool_, const DeviceDispatch& dld_) noexcept
        : allocations{std::move(allocations_)}, num{num_}, device{device_}, pool{pool_},
          dld{&dld_} {}

    /// Copying Vulkan allocations is not supported and will never be.
    PoolAllocations(const PoolAllocations&) = delete;
    PoolAllocations& operator=(const PoolAllocations&) = delete;

    /// Construct an allocation transfering ownership from another allocation.
    PoolAllocations(PoolAllocations&& rhs) noexcept
        : allocations{std::move(rhs.allocations)}, num{rhs.num}, device{rhs.device}, pool{rhs.pool},
          dld{rhs.dld} {}

    /// Assign an allocation transfering ownership from another allocation.
    /// Releases any previously held allocation.
    PoolAllocations& operator=(PoolAllocations&& rhs) noexcept {
        Release();
        allocations = std::move(rhs.allocations);
        num = rhs.num;
        device = rhs.device;
        pool = rhs.pool;
        dld = rhs.dld;
        return *this;
    }

    /// Destroys any held allocation.
    ~PoolAllocations() {
        Release();
    }

    /// Returns the number of allocations.
    std::size_t size() const noexcept {
        return num;
    }

    /// Returns a pointer to the array of allocations.
    AllocationType const* data() const noexcept {
        return allocations.get();
    }

    /// Returns the allocation in the specified index.
    /// @pre index < size()
    AllocationType operator[](std::size_t index) const noexcept {
        return allocations[index];
    }

    /// True when a pool fails to construct.
    bool IsOutOfPoolMemory() const noexcept {
        return !device;
    }

private:
    /// Destroys the held allocations if they exist.
    void Release() noexcept {
        if (!allocations) {
            return;
        }
        const Span<AllocationType> span(allocations.get(), num);
        const VkResult result = Free(device, pool, span, *dld);
        // There's no way to report errors from a destructor.
        if (result != VK_SUCCESS) {
            std::terminate();
        }
    }

    std::unique_ptr<AllocationType[]> allocations;
    std::size_t num = 0;
    VkDevice device = nullptr;
    PoolType pool = nullptr;
    const DeviceDispatch* dld = nullptr;
};

using DebugUtilsMessenger = Handle<VkDebugUtilsMessengerEXT, VkInstance, InstanceDispatch>;
using DescriptorSetLayout = Handle<VkDescriptorSetLayout, VkDevice, DeviceDispatch>;
using DescriptorUpdateTemplateKHR = Handle<VkDescriptorUpdateTemplateKHR, VkDevice, DeviceDispatch>;
using Pipeline = Handle<VkPipeline, VkDevice, DeviceDispatch>;
using PipelineLayout = Handle<VkPipelineLayout, VkDevice, DeviceDispatch>;
using QueryPool = Handle<VkQueryPool, VkDevice, DeviceDispatch>;
using RenderPass = Handle<VkRenderPass, VkDevice, DeviceDispatch>;
using Sampler = Handle<VkSampler, VkDevice, DeviceDispatch>;
using SurfaceKHR = Handle<VkSurfaceKHR, VkInstance, InstanceDispatch>;

using DescriptorSets = PoolAllocations<VkDescriptorSet, VkDescriptorPool>;
using CommandBuffers = PoolAllocations<VkCommandBuffer, VkCommandPool>;

/// Vulkan instance owning handle.
class Instance : public Handle<VkInstance, NoOwner, InstanceDispatch> {
    using Handle<VkInstance, NoOwner, InstanceDispatch>::Handle;

public:
    /// Creates a Vulkan instance.
    /// @throw Exception on initialization error.
    static Instance Create(u32 version, Span<const char*> layers, Span<const char*> extensions,
                           InstanceDispatch& dispatch);

    /// Enumerates physical devices.
    /// @return Physical devices and an empty handle on failure.
    /// @throw Exception on Vulkan error.
    std::vector<VkPhysicalDevice> EnumeratePhysicalDevices() const;

    /// Creates a debug callback messenger.
    /// @throw Exception on creation failure.
    DebugUtilsMessenger CreateDebugUtilsMessenger(
        const VkDebugUtilsMessengerCreateInfoEXT& create_info) const;

    /// Returns dispatch table.
    const InstanceDispatch& Dispatch() const noexcept {
        return *dld;
    }
};

class Queue {
public:
    /// Construct an empty queue handle.
    constexpr Queue() noexcept = default;

    /// Construct a queue handle.
    constexpr Queue(VkQueue queue_, const DeviceDispatch& dld_) noexcept
        : queue{queue_}, dld{&dld_} {}

    VkResult Submit(Span<VkSubmitInfo> submit_infos,
                    VkFence fence = VK_NULL_HANDLE) const noexcept {
        return dld->vkQueueSubmit(queue, submit_infos.size(), submit_infos.data(), fence);
    }

    VkResult Present(const VkPresentInfoKHR& present_info) const noexcept {
        return dld->vkQueuePresentKHR(queue, &present_info);
    }

private:
    VkQueue queue = nullptr;
    const DeviceDispatch* dld = nullptr;
};

class Buffer : public Handle<VkBuffer, VkDevice, DeviceDispatch> {
    using Handle<VkBuffer, VkDevice, DeviceDispatch>::Handle;

public:
    /// Attaches a memory allocation.
    void BindMemory(VkDeviceMemory memory, VkDeviceSize offset) const;

    /// Set object name.
    void SetObjectNameEXT(const char* name) const;
};

class BufferView : public Handle<VkBufferView, VkDevice, DeviceDispatch> {
    using Handle<VkBufferView, VkDevice, DeviceDispatch>::Handle;

public:
    /// Set object name.
    void SetObjectNameEXT(const char* name) const;
};

class Image : public Handle<VkImage, VkDevice, DeviceDispatch> {
    using Handle<VkImage, VkDevice, DeviceDispatch>::Handle;

public:
    /// Attaches a memory allocation.
    void BindMemory(VkDeviceMemory memory, VkDeviceSize offset) const;

    /// Set object name.
    void SetObjectNameEXT(const char* name) const;
};

class ImageView : public Handle<VkImageView, VkDevice, DeviceDispatch> {
    using Handle<VkImageView, VkDevice, DeviceDispatch>::Handle;

public:
    /// Set object name.
    void SetObjectNameEXT(const char* name) const;
};

class DeviceMemory : public Handle<VkDeviceMemory, VkDevice, DeviceDispatch> {
    using Handle<VkDeviceMemory, VkDevice, DeviceDispatch>::Handle;

public:
    int GetMemoryFdKHR() const;

#ifdef _WIN32
    HANDLE GetMemoryWin32HandleKHR() const;
#endif

    /// Set object name.
    void SetObjectNameEXT(const char* name) const;

    u8* Map(VkDeviceSize offset, VkDeviceSize size) const {
        void* data;
        Check(dld->vkMapMemory(owner, handle, offset, size, 0, &data));
        return static_cast<u8*>(data);
    }

    void Unmap() const noexcept {
        dld->vkUnmapMemory(owner, handle);
    }
};

class Fence : public Handle<VkFence, VkDevice, DeviceDispatch> {
    using Handle<VkFence, VkDevice, DeviceDispatch>::Handle;

public:
    /// Set object name.
    void SetObjectNameEXT(const char* name) const;

    VkResult Wait(u64 timeout = std::numeric_limits<u64>::max()) const noexcept {
        return dld->vkWaitForFences(owner, 1, &handle, true, timeout);
    }

    VkResult GetStatus() const noexcept {
        return dld->vkGetFenceStatus(owner, handle);
    }

    void Reset() const {
        Check(dld->vkResetFences(owner, 1, &handle));
    }
};

class Framebuffer : public Handle<VkFramebuffer, VkDevice, DeviceDispatch> {
    using Handle<VkFramebuffer, VkDevice, DeviceDispatch>::Handle;

public:
    /// Set object name.
    void SetObjectNameEXT(const char* name) const;
};

class DescriptorPool : public Handle<VkDescriptorPool, VkDevice, DeviceDispatch> {
    using Handle<VkDescriptorPool, VkDevice, DeviceDispatch>::Handle;

public:
    DescriptorSets Allocate(const VkDescriptorSetAllocateInfo& ai) const;

    /// Set object name.
    void SetObjectNameEXT(const char* name) const;
};

class CommandPool : public Handle<VkCommandPool, VkDevice, DeviceDispatch> {
    using Handle<VkCommandPool, VkDevice, DeviceDispatch>::Handle;

public:
    CommandBuffers Allocate(std::size_t num_buffers,
                            VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY) const;

    /// Set object name.
    void SetObjectNameEXT(const char* name) const;
};

class SwapchainKHR : public Handle<VkSwapchainKHR, VkDevice, DeviceDispatch> {
    using Handle<VkSwapchainKHR, VkDevice, DeviceDispatch>::Handle;

public:
    std::vector<VkImage> GetImages() const;
};

class Event : public Handle<VkEvent, VkDevice, DeviceDispatch> {
    using Handle<VkEvent, VkDevice, DeviceDispatch>::Handle;

public:
    /// Set object name.
    void SetObjectNameEXT(const char* name) const;

    VkResult GetStatus() const noexcept {
        return dld->vkGetEventStatus(owner, handle);
    }
};

class ShaderModule : public Handle<VkShaderModule, VkDevice, DeviceDispatch> {
    using Handle<VkShaderModule, VkDevice, DeviceDispatch>::Handle;

public:
    /// Set object name.
    void SetObjectNameEXT(const char* name) const;
};

class Semaphore : public Handle<VkSemaphore, VkDevice, DeviceDispatch> {
    using Handle<VkSemaphore, VkDevice, DeviceDispatch>::Handle;

public:
    /// Set object name.
    void SetObjectNameEXT(const char* name) const;

    [[nodiscard]] u64 GetCounter() const {
        u64 value;
        Check(dld->vkGetSemaphoreCounterValueKHR(owner, handle, &value));
        return value;
    }

    /**
     * Waits for a timeline semaphore on the host.
     *
     * @param value   Value to wait
     * @param timeout Time in nanoseconds to timeout
     * @return        True on successful wait, false on timeout
     */
    bool Wait(u64 value, u64 timeout = std::numeric_limits<u64>::max()) const {
        const VkSemaphoreWaitInfoKHR wait_info{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO_KHR,
            .pNext = nullptr,
            .flags = 0,
            .semaphoreCount = 1,
            .pSemaphores = &handle,
            .pValues = &value,
        };
        const VkResult result = dld->vkWaitSemaphoresKHR(owner, &wait_info, timeout);
        switch (result) {
        case VK_SUCCESS:
            return true;
        case VK_TIMEOUT:
            return false;
        default:
            throw Exception(result);
        }
    }
};

class Device : public Handle<VkDevice, NoOwner, DeviceDispatch> {
    using Handle<VkDevice, NoOwner, DeviceDispatch>::Handle;

public:
    static Device Create(VkPhysicalDevice physical_device, Span<VkDeviceQueueCreateInfo> queues_ci,
                         Span<const char*> enabled_extensions, const void* next,
                         DeviceDispatch& dispatch);

    Queue GetQueue(u32 family_index) const noexcept;

    Buffer CreateBuffer(const VkBufferCreateInfo& ci) const;

    BufferView CreateBufferView(const VkBufferViewCreateInfo& ci) const;

    Image CreateImage(const VkImageCreateInfo& ci) const;

    ImageView CreateImageView(const VkImageViewCreateInfo& ci) const;

    Semaphore CreateSemaphore() const;

    Semaphore CreateSemaphore(const VkSemaphoreCreateInfo& ci) const;

    Fence CreateFence(const VkFenceCreateInfo& ci) const;

    DescriptorPool CreateDescriptorPool(const VkDescriptorPoolCreateInfo& ci) const;

    RenderPass CreateRenderPass(const VkRenderPassCreateInfo& ci) const;

    DescriptorSetLayout CreateDescriptorSetLayout(const VkDescriptorSetLayoutCreateInfo& ci) const;

    PipelineLayout CreatePipelineLayout(const VkPipelineLayoutCreateInfo& ci) const;

    Pipeline CreateGraphicsPipeline(const VkGraphicsPipelineCreateInfo& ci) const;

    Pipeline CreateComputePipeline(const VkComputePipelineCreateInfo& ci) const;

    Sampler CreateSampler(const VkSamplerCreateInfo& ci) const;

    Framebuffer CreateFramebuffer(const VkFramebufferCreateInfo& ci) const;

    CommandPool CreateCommandPool(const VkCommandPoolCreateInfo& ci) const;

    DescriptorUpdateTemplateKHR CreateDescriptorUpdateTemplateKHR(
        const VkDescriptorUpdateTemplateCreateInfoKHR& ci) const;

    QueryPool CreateQueryPool(const VkQueryPoolCreateInfo& ci) const;

    ShaderModule CreateShaderModule(const VkShaderModuleCreateInfo& ci) const;

    Event CreateEvent() const;

    SwapchainKHR CreateSwapchainKHR(const VkSwapchainCreateInfoKHR& ci) const;

    DeviceMemory TryAllocateMemory(const VkMemoryAllocateInfo& ai) const noexcept;

    DeviceMemory AllocateMemory(const VkMemoryAllocateInfo& ai) const;

    VkMemoryRequirements GetBufferMemoryRequirements(VkBuffer buffer,
                                                     void* pnext = nullptr) const noexcept;

    VkMemoryRequirements GetImageMemoryRequirements(VkImage image) const noexcept;

    void UpdateDescriptorSets(Span<VkWriteDescriptorSet> writes,
                              Span<VkCopyDescriptorSet> copies) const noexcept;

    void UpdateDescriptorSet(VkDescriptorSet set, VkDescriptorUpdateTemplateKHR update_template,
                             const void* data) const noexcept {
        dld->vkUpdateDescriptorSetWithTemplateKHR(handle, set, update_template, data);
    }

    VkResult AcquireNextImageKHR(VkSwapchainKHR swapchain, u64 timeout, VkSemaphore semaphore,
                                 VkFence fence, u32* image_index) const noexcept {
        return dld->vkAcquireNextImageKHR(handle, swapchain, timeout, semaphore, fence,
                                          image_index);
    }

    VkResult WaitIdle() const noexcept {
        return dld->vkDeviceWaitIdle(handle);
    }

    void ResetQueryPoolEXT(VkQueryPool query_pool, u32 first, u32 count) const noexcept {
        dld->vkResetQueryPoolEXT(handle, query_pool, first, count);
    }

    VkResult GetQueryResults(VkQueryPool query_pool, u32 first, u32 count, std::size_t data_size,
                             void* data, VkDeviceSize stride,
                             VkQueryResultFlags flags) const noexcept {
        return dld->vkGetQueryPoolResults(handle, query_pool, first, count, data_size, data, stride,
                                          flags);
    }
};

class PhysicalDevice {
public:
    constexpr PhysicalDevice() noexcept = default;

    constexpr PhysicalDevice(VkPhysicalDevice physical_device_,
                             const InstanceDispatch& dld_) noexcept
        : physical_device{physical_device_}, dld{&dld_} {}

    constexpr operator VkPhysicalDevice() const noexcept {
        return physical_device;
    }

    VkPhysicalDeviceProperties GetProperties() const noexcept;

    void GetProperties2KHR(VkPhysicalDeviceProperties2KHR&) const noexcept;

    VkPhysicalDeviceFeatures GetFeatures() const noexcept;

    void GetFeatures2KHR(VkPhysicalDeviceFeatures2KHR&) const noexcept;

    VkFormatProperties GetFormatProperties(VkFormat) const noexcept;

    std::vector<VkExtensionProperties> EnumerateDeviceExtensionProperties() const;

    std::vector<VkQueueFamilyProperties> GetQueueFamilyProperties() const;

    bool GetSurfaceSupportKHR(u32 queue_family_index, VkSurfaceKHR) const;

    VkSurfaceCapabilitiesKHR GetSurfaceCapabilitiesKHR(VkSurfaceKHR) const;

    std::vector<VkSurfaceFormatKHR> GetSurfaceFormatsKHR(VkSurfaceKHR) const;

    std::vector<VkPresentModeKHR> GetSurfacePresentModesKHR(VkSurfaceKHR) const;

    VkPhysicalDeviceMemoryProperties GetMemoryProperties() const noexcept;

private:
    VkPhysicalDevice physical_device = nullptr;
    const InstanceDispatch* dld = nullptr;
};

class CommandBuffer {
public:
    CommandBuffer() noexcept = default;

    explicit CommandBuffer(VkCommandBuffer handle_, const DeviceDispatch& dld_) noexcept
        : handle{handle_}, dld{&dld_} {}

    const VkCommandBuffer* address() const noexcept {
        return &handle;
    }

    void Begin(const VkCommandBufferBeginInfo& begin_info) const {
        Check(dld->vkBeginCommandBuffer(handle, &begin_info));
    }

    void End() const {
        Check(dld->vkEndCommandBuffer(handle));
    }

    void BeginRenderPass(const VkRenderPassBeginInfo& renderpass_bi,
                         VkSubpassContents contents) const noexcept {
        dld->vkCmdBeginRenderPass(handle, &renderpass_bi, contents);
    }

    void EndRenderPass() const noexcept {
        dld->vkCmdEndRenderPass(handle);
    }

    void BeginQuery(VkQueryPool query_pool, u32 query, VkQueryControlFlags flags) const noexcept {
        dld->vkCmdBeginQuery(handle, query_pool, query, flags);
    }

    void EndQuery(VkQueryPool query_pool, u32 query) const noexcept {
        dld->vkCmdEndQuery(handle, query_pool, query);
    }

    void BindDescriptorSets(VkPipelineBindPoint bind_point, VkPipelineLayout layout, u32 first,
                            Span<VkDescriptorSet> sets, Span<u32> dynamic_offsets) const noexcept {
        dld->vkCmdBindDescriptorSets(handle, bind_point, layout, first, sets.size(), sets.data(),
                                     dynamic_offsets.size(), dynamic_offsets.data());
    }

    void PushDescriptorSetWithTemplateKHR(VkDescriptorUpdateTemplateKHR update_template,
                                          VkPipelineLayout layout, u32 set,
                                          const void* data) const noexcept {
        dld->vkCmdPushDescriptorSetWithTemplateKHR(handle, update_template, layout, set, data);
    }

    void BindPipeline(VkPipelineBindPoint bind_point, VkPipeline pipeline) const noexcept {
        dld->vkCmdBindPipeline(handle, bind_point, pipeline);
    }

    void BindIndexBuffer(VkBuffer buffer, VkDeviceSize offset,
                         VkIndexType index_type) const noexcept {
        dld->vkCmdBindIndexBuffer(handle, buffer, offset, index_type);
    }

    void BindVertexBuffers(u32 first, u32 count, const VkBuffer* buffers,
                           const VkDeviceSize* offsets) const noexcept {
        dld->vkCmdBindVertexBuffers(handle, first, count, buffers, offsets);
    }

    void BindVertexBuffer(u32 binding, VkBuffer buffer, VkDeviceSize offset) const noexcept {
        BindVertexBuffers(binding, 1, &buffer, &offset);
    }

    void Draw(u32 vertex_count, u32 instance_count, u32 first_vertex,
              u32 first_instance) const noexcept {
        dld->vkCmdDraw(handle, vertex_count, instance_count, first_vertex, first_instance);
    }

    void DrawIndexed(u32 index_count, u32 instance_count, u32 first_index, u32 vertex_offset,
                     u32 first_instance) const noexcept {
        dld->vkCmdDrawIndexed(handle, index_count, instance_count, first_index, vertex_offset,
                              first_instance);
    }

    void ClearAttachments(Span<VkClearAttachment> attachments,
                          Span<VkClearRect> rects) const noexcept {
        dld->vkCmdClearAttachments(handle, attachments.size(), attachments.data(), rects.size(),
                                   rects.data());
    }

    void BlitImage(VkImage src_image, VkImageLayout src_layout, VkImage dst_image,
                   VkImageLayout dst_layout, Span<VkImageBlit> regions,
                   VkFilter filter) const noexcept {
        dld->vkCmdBlitImage(handle, src_image, src_layout, dst_image, dst_layout, regions.size(),
                            regions.data(), filter);
    }

    void ResolveImage(VkImage src_image, VkImageLayout src_layout, VkImage dst_image,
                      VkImageLayout dst_layout, Span<VkImageResolve> regions) {
        dld->vkCmdResolveImage(handle, src_image, src_layout, dst_image, dst_layout, regions.size(),
                               regions.data());
    }

    void Dispatch(u32 x, u32 y, u32 z) const noexcept {
        dld->vkCmdDispatch(handle, x, y, z);
    }

    void PipelineBarrier(VkPipelineStageFlags src_stage_mask, VkPipelineStageFlags dst_stage_mask,
                         VkDependencyFlags dependency_flags, Span<VkMemoryBarrier> memory_barriers,
                         Span<VkBufferMemoryBarrier> buffer_barriers,
                         Span<VkImageMemoryBarrier> image_barriers) const noexcept {
        dld->vkCmdPipelineBarrier(handle, src_stage_mask, dst_stage_mask, dependency_flags,
                                  memory_barriers.size(), memory_barriers.data(),
                                  buffer_barriers.size(), buffer_barriers.data(),
                                  image_barriers.size(), image_barriers.data());
    }

    void PipelineBarrier(VkPipelineStageFlags src_stage_mask, VkPipelineStageFlags dst_stage_mask,
                         VkDependencyFlags dependency_flags = 0) const noexcept {
        PipelineBarrier(src_stage_mask, dst_stage_mask, dependency_flags, {}, {}, {});
    }

    void PipelineBarrier(VkPipelineStageFlags src_stage_mask, VkPipelineStageFlags dst_stage_mask,
                         VkDependencyFlags dependency_flags,
                         const VkMemoryBarrier& memory_barrier) const noexcept {
        PipelineBarrier(src_stage_mask, dst_stage_mask, dependency_flags, memory_barrier, {}, {});
    }

    void PipelineBarrier(VkPipelineStageFlags src_stage_mask, VkPipelineStageFlags dst_stage_mask,
                         VkDependencyFlags dependency_flags,
                         const VkBufferMemoryBarrier& buffer_barrier) const noexcept {
        PipelineBarrier(src_stage_mask, dst_stage_mask, dependency_flags, {}, buffer_barrier, {});
    }

    void PipelineBarrier(VkPipelineStageFlags src_stage_mask, VkPipelineStageFlags dst_stage_mask,
                         VkDependencyFlags dependency_flags,
                         const VkImageMemoryBarrier& image_barrier) const noexcept {
        PipelineBarrier(src_stage_mask, dst_stage_mask, dependency_flags, {}, {}, image_barrier);
    }

    void CopyBufferToImage(VkBuffer src_buffer, VkImage dst_image, VkImageLayout dst_image_layout,
                           Span<VkBufferImageCopy> regions) const noexcept {
        dld->vkCmdCopyBufferToImage(handle, src_buffer, dst_image, dst_image_layout, regions.size(),
                                    regions.data());
    }

    void CopyBuffer(VkBuffer src_buffer, VkBuffer dst_buffer,
                    Span<VkBufferCopy> regions) const noexcept {
        dld->vkCmdCopyBuffer(handle, src_buffer, dst_buffer, regions.size(), regions.data());
    }

    void CopyImage(VkImage src_image, VkImageLayout src_layout, VkImage dst_image,
                   VkImageLayout dst_layout, Span<VkImageCopy> regions) const noexcept {
        dld->vkCmdCopyImage(handle, src_image, src_layout, dst_image, dst_layout, regions.size(),
                            regions.data());
    }

    void CopyImageToBuffer(VkImage src_image, VkImageLayout src_layout, VkBuffer dst_buffer,
                           Span<VkBufferImageCopy> regions) const noexcept {
        dld->vkCmdCopyImageToBuffer(handle, src_image, src_layout, dst_buffer, regions.size(),
                                    regions.data());
    }

    void FillBuffer(VkBuffer dst_buffer, VkDeviceSize dst_offset, VkDeviceSize size,
                    u32 data) const noexcept {
        dld->vkCmdFillBuffer(handle, dst_buffer, dst_offset, size, data);
    }

    void PushConstants(VkPipelineLayout layout, VkShaderStageFlags flags, u32 offset, u32 size,
                       const void* values) const noexcept {
        dld->vkCmdPushConstants(handle, layout, flags, offset, size, values);
    }

    template <typename T>
    void PushConstants(VkPipelineLayout layout, VkShaderStageFlags flags,
                       const T& data) const noexcept {
        static_assert(std::is_trivially_copyable_v<T>, "<data> is not trivially copyable");
        dld->vkCmdPushConstants(handle, layout, flags, 0, static_cast<u32>(sizeof(T)), &data);
    }

    void SetViewport(u32 first, Span<VkViewport> viewports) const noexcept {
        dld->vkCmdSetViewport(handle, first, viewports.size(), viewports.data());
    }

    void SetScissor(u32 first, Span<VkRect2D> scissors) const noexcept {
        dld->vkCmdSetScissor(handle, first, scissors.size(), scissors.data());
    }

    void SetBlendConstants(const float blend_constants[4]) const noexcept {
        dld->vkCmdSetBlendConstants(handle, blend_constants);
    }

    void SetStencilCompareMask(VkStencilFaceFlags face_mask, u32 compare_mask) const noexcept {
        dld->vkCmdSetStencilCompareMask(handle, face_mask, compare_mask);
    }

    void SetStencilReference(VkStencilFaceFlags face_mask, u32 reference) const noexcept {
        dld->vkCmdSetStencilReference(handle, face_mask, reference);
    }

    void SetStencilWriteMask(VkStencilFaceFlags face_mask, u32 write_mask) const noexcept {
        dld->vkCmdSetStencilWriteMask(handle, face_mask, write_mask);
    }

    void SetDepthBias(float constant_factor, float clamp, float slope_factor) const noexcept {
        dld->vkCmdSetDepthBias(handle, constant_factor, clamp, slope_factor);
    }

    void SetDepthBounds(float min_depth_bounds, float max_depth_bounds) const noexcept {
        dld->vkCmdSetDepthBounds(handle, min_depth_bounds, max_depth_bounds);
    }

    void SetEvent(VkEvent event, VkPipelineStageFlags stage_flags) const noexcept {
        dld->vkCmdSetEvent(handle, event, stage_flags);
    }

    void WaitEvents(Span<VkEvent> events, VkPipelineStageFlags src_stage_mask,
                    VkPipelineStageFlags dst_stage_mask, Span<VkMemoryBarrier> memory_barriers,
                    Span<VkBufferMemoryBarrier> buffer_barriers,
                    Span<VkImageMemoryBarrier> image_barriers) const noexcept {
        dld->vkCmdWaitEvents(handle, events.size(), events.data(), src_stage_mask, dst_stage_mask,
                             memory_barriers.size(), memory_barriers.data(), buffer_barriers.size(),
                             buffer_barriers.data(), image_barriers.size(), image_barriers.data());
    }

    void BindVertexBuffers2EXT(u32 first_binding, u32 binding_count, const VkBuffer* buffers,
                               const VkDeviceSize* offsets, const VkDeviceSize* sizes,
                               const VkDeviceSize* strides) const noexcept {
        dld->vkCmdBindVertexBuffers2EXT(handle, first_binding, binding_count, buffers, offsets,
                                        sizes, strides);
    }

    void SetCullModeEXT(VkCullModeFlags cull_mode) const noexcept {
        dld->vkCmdSetCullModeEXT(handle, cull_mode);
    }

    void SetDepthBoundsTestEnableEXT(bool enable) const noexcept {
        dld->vkCmdSetDepthBoundsTestEnableEXT(handle, enable ? VK_TRUE : VK_FALSE);
    }

    void SetDepthCompareOpEXT(VkCompareOp compare_op) const noexcept {
        dld->vkCmdSetDepthCompareOpEXT(handle, compare_op);
    }

    void SetDepthTestEnableEXT(bool enable) const noexcept {
        dld->vkCmdSetDepthTestEnableEXT(handle, enable ? VK_TRUE : VK_FALSE);
    }

    void SetDepthWriteEnableEXT(bool enable) const noexcept {
        dld->vkCmdSetDepthWriteEnableEXT(handle, enable ? VK_TRUE : VK_FALSE);
    }

    void SetFrontFaceEXT(VkFrontFace front_face) const noexcept {
        dld->vkCmdSetFrontFaceEXT(handle, front_face);
    }

    void SetLineWidth(float line_width) const noexcept {
        dld->vkCmdSetLineWidth(handle, line_width);
    }

    void SetPrimitiveTopologyEXT(VkPrimitiveTopology primitive_topology) const noexcept {
        dld->vkCmdSetPrimitiveTopologyEXT(handle, primitive_topology);
    }

    void SetStencilOpEXT(VkStencilFaceFlags face_mask, VkStencilOp fail_op, VkStencilOp pass_op,
                         VkStencilOp depth_fail_op, VkCompareOp compare_op) const noexcept {
        dld->vkCmdSetStencilOpEXT(handle, face_mask, fail_op, pass_op, depth_fail_op, compare_op);
    }

    void SetStencilTestEnableEXT(bool enable) const noexcept {
        dld->vkCmdSetStencilTestEnableEXT(handle, enable ? VK_TRUE : VK_FALSE);
    }

    void SetVertexInputEXT(
        vk::Span<VkVertexInputBindingDescription2EXT> bindings,
        vk::Span<VkVertexInputAttributeDescription2EXT> attributes) const noexcept {
        dld->vkCmdSetVertexInputEXT(handle, bindings.size(), bindings.data(), attributes.size(),
                                    attributes.data());
    }

    void BindTransformFeedbackBuffersEXT(u32 first, u32 count, const VkBuffer* buffers,
                                         const VkDeviceSize* offsets,
                                         const VkDeviceSize* sizes) const noexcept {
        dld->vkCmdBindTransformFeedbackBuffersEXT(handle, first, count, buffers, offsets, sizes);
    }

    void BeginTransformFeedbackEXT(u32 first_counter_buffer, u32 counter_buffers_count,
                                   const VkBuffer* counter_buffers,
                                   const VkDeviceSize* counter_buffer_offsets) const noexcept {
        dld->vkCmdBeginTransformFeedbackEXT(handle, first_counter_buffer, counter_buffers_count,
                                            counter_buffers, counter_buffer_offsets);
    }

    void EndTransformFeedbackEXT(u32 first_counter_buffer, u32 counter_buffers_count,
                                 const VkBuffer* counter_buffers,
                                 const VkDeviceSize* counter_buffer_offsets) const noexcept {
        dld->vkCmdEndTransformFeedbackEXT(handle, first_counter_buffer, counter_buffers_count,
                                          counter_buffers, counter_buffer_offsets);
    }

    void BeginDebugUtilsLabelEXT(const char* label, std::span<float, 4> color) const noexcept {
        const VkDebugUtilsLabelEXT label_info{
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
            .pNext = nullptr,
            .pLabelName = label,
            .color{color[0], color[1], color[2], color[3]},
        };
        dld->vkCmdBeginDebugUtilsLabelEXT(handle, &label_info);
    }

    void EndDebugUtilsLabelEXT() const noexcept {
        dld->vkCmdEndDebugUtilsLabelEXT(handle);
    }

private:
    VkCommandBuffer handle;
    const DeviceDispatch* dld;
};

u32 AvailableVersion(const InstanceDispatch& dld) noexcept;

std::optional<std::vector<VkExtensionProperties>> EnumerateInstanceExtensionProperties(
    const InstanceDispatch& dld);

std::optional<std::vector<VkLayerProperties>> EnumerateInstanceLayerProperties(
    const InstanceDispatch& dld);

} // namespace Vulkan::vk
