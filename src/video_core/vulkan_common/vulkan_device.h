// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "common/common_types.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Vulkan {

class NsightAftermathTracker;

/// Format usage descriptor.
enum class FormatType { Linear, Optimal, Buffer };

/// Subgroup size of the guest emulated hardware (Nvidia has 32 threads per subgroup).
const u32 GuestWarpSize = 32;

/// Handles data specific to a physical device.
class Device {
public:
    explicit Device(VkInstance instance, vk::PhysicalDevice physical, VkSurfaceKHR surface,
                    const vk::InstanceDispatch& dld);
    ~Device();

    /**
     * Returns a format supported by the device for the passed requeriments.
     * @param wanted_format The ideal format to be returned. It may not be the returned format.
     * @param wanted_usage The usage that must be fulfilled even if the format is not supported.
     * @param format_type Format type usage.
     * @returns A format supported by the device.
     */
    VkFormat GetSupportedFormat(VkFormat wanted_format, VkFormatFeatureFlags wanted_usage,
                                FormatType format_type) const;

    /// Reports a device loss.
    void ReportLoss() const;

    /// Reports a shader to Nsight Aftermath.
    void SaveShader(std::span<const u32> spirv) const;

    /// Returns the name of the VkDriverId reported from Vulkan.
    std::string GetDriverName() const;

    /// Returns the dispatch loader with direct function pointers of the device.
    const vk::DeviceDispatch& GetDispatchLoader() const {
        return dld;
    }

    /// Returns the logical device.
    const vk::Device& GetLogical() const {
        return logical;
    }

    /// Returns the physical device.
    vk::PhysicalDevice GetPhysical() const {
        return physical;
    }

    /// Returns the main graphics queue.
    vk::Queue GetGraphicsQueue() const {
        return graphics_queue;
    }

    /// Returns the main present queue.
    vk::Queue GetPresentQueue() const {
        return present_queue;
    }

    /// Returns main graphics queue family index.
    u32 GetGraphicsFamily() const {
        return graphics_family;
    }

    /// Returns main present queue family index.
    u32 GetPresentFamily() const {
        return present_family;
    }

    /// Returns the current Vulkan API version provided in Vulkan-formatted version numbers.
    u32 ApiVersion() const {
        return properties.apiVersion;
    }

    /// Returns the current driver version provided in Vulkan-formatted version numbers.
    u32 GetDriverVersion() const {
        return properties.driverVersion;
    }

    /// Returns the device name.
    std::string_view GetModelName() const {
        return properties.deviceName;
    }

    /// Returns the driver ID.
    VkDriverIdKHR GetDriverID() const {
        return driver_id;
    }

    /// Returns uniform buffer alignment requeriment.
    VkDeviceSize GetUniformBufferAlignment() const {
        return properties.limits.minUniformBufferOffsetAlignment;
    }

    /// Returns storage alignment requeriment.
    VkDeviceSize GetStorageBufferAlignment() const {
        return properties.limits.minStorageBufferOffsetAlignment;
    }

    /// Returns the maximum range for storage buffers.
    VkDeviceSize GetMaxStorageBufferRange() const {
        return properties.limits.maxStorageBufferRange;
    }

    /// Returns the maximum size for push constants.
    VkDeviceSize GetMaxPushConstantsSize() const {
        return properties.limits.maxPushConstantsSize;
    }

    /// Returns the maximum size for shared memory.
    u32 GetMaxComputeSharedMemorySize() const {
        return properties.limits.maxComputeSharedMemorySize;
    }

    /// Returns float control properties of the device.
    const VkPhysicalDeviceFloatControlsPropertiesKHR& FloatControlProperties() const {
        return float_controls;
    }

    /// Returns true if ASTC is natively supported.
    bool IsOptimalAstcSupported() const {
        return is_optimal_astc_supported;
    }

    /// Returns true if the device supports float16 natively
    bool IsFloat16Supported() const {
        return is_float16_supported;
    }

    /// Returns true if the device warp size can potentially be bigger than guest's warp size.
    bool IsWarpSizePotentiallyBiggerThanGuest() const {
        return is_warp_potentially_bigger;
    }

    /// Returns true if the device can be forced to use the guest warp size.
    bool IsGuestWarpSizeSupported(VkShaderStageFlagBits stage) const {
        return guest_warp_stages & stage;
    }

    /// Returns the maximum number of push descriptors.
    u32 MaxPushDescriptors() const {
        return max_push_descriptors;
    }

    /// Returns true if formatless image load is supported.
    bool IsFormatlessImageLoadSupported() const {
        return is_formatless_image_load_supported;
    }

    /// Returns true if shader int64 is supported.
    bool IsShaderInt64Supported() const {
        return is_shader_int64_supported;
    }

    /// Returns true if shader int16 is supported.
    bool IsShaderInt16Supported() const {
        return is_shader_int16_supported;
    }

    // Returns true if depth bounds is supported.
    bool IsDepthBoundsSupported() const {
        return is_depth_bounds_supported;
    }

    /// Returns true when blitting from and to depth stencil images is supported.
    bool IsBlitDepthStencilSupported() const {
        return is_blit_depth_stencil_supported;
    }

    /// Returns true if the device supports VK_NV_viewport_swizzle.
    bool IsNvViewportSwizzleSupported() const {
        return nv_viewport_swizzle;
    }

    /// Returns true if the device supports VK_NV_viewport_array2.
    bool IsNvViewportArray2Supported() const {
        return nv_viewport_array2;
    }

    /// Returns true if the device supports VK_NV_geometry_shader_passthrough.
    bool IsNvGeometryShaderPassthroughSupported() const {
        return nv_geometry_shader_passthrough;
    }

    /// Returns true if the device supports VK_KHR_uniform_buffer_standard_layout.
    bool IsKhrUniformBufferStandardLayoutSupported() const {
        return khr_uniform_buffer_standard_layout;
    }

    /// Returns true if the device supports VK_KHR_spirv_1_4.
    bool IsKhrSpirv1_4Supported() const {
        return khr_spirv_1_4;
    }

    /// Returns true if the device supports VK_KHR_push_descriptor.
    bool IsKhrPushDescriptorSupported() const {
        return khr_push_descriptor;
    }

    /// Returns true if the device supports VK_KHR_workgroup_memory_explicit_layout.
    bool IsKhrWorkgroupMemoryExplicitLayoutSupported() const {
        return khr_workgroup_memory_explicit_layout;
    }

    /// Returns true if the device supports VK_EXT_index_type_uint8.
    bool IsExtIndexTypeUint8Supported() const {
        return ext_index_type_uint8;
    }

    /// Returns true if the device supports VK_EXT_sampler_filter_minmax.
    bool IsExtSamplerFilterMinmaxSupported() const {
        return ext_sampler_filter_minmax;
    }

    /// Returns true if the device supports VK_EXT_depth_range_unrestricted.
    bool IsExtDepthRangeUnrestrictedSupported() const {
        return ext_depth_range_unrestricted;
    }

    /// Returns true if the device supports VK_EXT_shader_viewport_index_layer.
    bool IsExtShaderViewportIndexLayerSupported() const {
        return ext_shader_viewport_index_layer;
    }

    /// Returns true if the device supports VK_EXT_subgroup_size_control.
    bool IsExtSubgroupSizeControlSupported() const {
        return ext_subgroup_size_control;
    }

    /// Returns true if the device supports VK_EXT_transform_feedback.
    bool IsExtTransformFeedbackSupported() const {
        return ext_transform_feedback;
    }

    /// Returns true if the device supports VK_EXT_custom_border_color.
    bool IsExtCustomBorderColorSupported() const {
        return ext_custom_border_color;
    }

    /// Returns true if the device supports VK_EXT_extended_dynamic_state.
    bool IsExtExtendedDynamicStateSupported() const {
        return ext_extended_dynamic_state;
    }

    /// Returns true if the device supports VK_EXT_line_rasterization.
    bool IsExtLineRasterizationSupported() const {
        return ext_line_rasterization;
    }

    /// Returns true if the device supports VK_EXT_vertex_input_dynamic_state.
    bool IsExtVertexInputDynamicStateSupported() const {
        return ext_vertex_input_dynamic_state;
    }

    /// Returns true if the device supports VK_EXT_shader_stencil_export.
    bool IsExtShaderStencilExportSupported() const {
        return ext_shader_stencil_export;
    }

    /// Returns true if the device supports VK_EXT_conservative_rasterization.
    bool IsExtConservativeRasterizationSupported() const {
        return ext_conservative_rasterization;
    }

    /// Returns true if the device supports VK_EXT_provoking_vertex.
    bool IsExtProvokingVertexSupported() const {
        return ext_provoking_vertex;
    }

    /// Returns true if the device supports VK_KHR_shader_atomic_int64.
    bool IsExtShaderAtomicInt64Supported() const {
        return ext_shader_atomic_int64;
    }

    /// Returns true when a known debugging tool is attached.
    bool HasDebuggingToolAttached() const {
        return has_renderdoc || has_nsight_graphics;
    }

    /// Returns the vendor name reported from Vulkan.
    std::string_view GetVendorName() const {
        return vendor_name;
    }

    /// Returns the list of available extensions.
    const std::vector<std::string>& GetAvailableExtensions() const {
        return supported_extensions;
    }

    u64 GetDeviceLocalMemory() const {
        return device_access_memory;
    }

private:
    /// Checks if the physical device is suitable.
    void CheckSuitability(bool requires_swapchain) const;

    /// Loads extensions into a vector and stores available ones in this object.
    std::vector<const char*> LoadExtensions(bool requires_surface);

    /// Sets up queue families.
    void SetupFamilies(VkSurfaceKHR surface);

    /// Sets up device features.
    void SetupFeatures();

    /// Sets up device properties.
    void SetupProperties();

    /// Collects telemetry information from the device.
    void CollectTelemetryParameters();

    /// Collects information about attached tools.
    void CollectToolingInfo();

    /// Collects information about the device's local memory.
    void CollectPhysicalMemoryInfo();

    /// Returns a list of queue initialization descriptors.
    std::vector<VkDeviceQueueCreateInfo> GetDeviceQueueCreateInfos() const;

    /// Returns true if ASTC textures are natively supported.
    bool IsOptimalAstcSupported(const VkPhysicalDeviceFeatures& features) const;

    /// Returns true if the device natively supports blitting depth stencil images.
    bool TestDepthStencilBlits() const;

    /// Returns true if a format is supported.
    bool IsFormatSupported(VkFormat wanted_format, VkFormatFeatureFlags wanted_usage,
                           FormatType format_type) const;

    VkInstance instance;                                         ///< Vulkan instance.
    vk::DeviceDispatch dld;                                      ///< Device function pointers.
    vk::PhysicalDevice physical;                                 ///< Physical device.
    VkPhysicalDeviceProperties properties;                       ///< Device properties.
    VkPhysicalDeviceFloatControlsPropertiesKHR float_controls{}; ///< Float control properties.
    vk::Device logical;                                          ///< Logical device.
    vk::Queue graphics_queue;                                    ///< Main graphics queue.
    vk::Queue present_queue;                                     ///< Main present queue.
    u32 instance_version{};                                      ///< Vulkan onstance version.
    u32 graphics_family{};                      ///< Main graphics queue family index.
    u32 present_family{};                       ///< Main present queue family index.
    VkDriverIdKHR driver_id{};                  ///< Driver ID.
    VkShaderStageFlags guest_warp_stages{};     ///< Stages where the guest warp size can be forced.
    u64 device_access_memory{};                 ///< Total size of device local memory in bytes.
    u32 max_push_descriptors{};                 ///< Maximum number of push descriptors
    bool is_optimal_astc_supported{};           ///< Support for native ASTC.
    bool is_float16_supported{};                ///< Support for float16 arithmetics.
    bool is_warp_potentially_bigger{};          ///< Host warp size can be bigger than guest.
    bool is_formatless_image_load_supported{};  ///< Support for shader image read without format.
    bool is_depth_bounds_supported{};           ///< Support for depth bounds.
    bool is_shader_float64_supported{};         ///< Support for float64.
    bool is_shader_int64_supported{};           ///< Support for int64.
    bool is_shader_int16_supported{};           ///< Support for int16.
    bool is_shader_storage_image_multisample{}; ///< Support for image operations on MSAA images.
    bool is_blit_depth_stencil_supported{};     ///< Support for blitting from and to depth stencil.
    bool nv_viewport_swizzle{};                 ///< Support for VK_NV_viewport_swizzle.
    bool nv_viewport_array2{};                  ///< Support for VK_NV_viewport_array2.
    bool nv_geometry_shader_passthrough{};      ///< Support for VK_NV_geometry_shader_passthrough.
    bool khr_uniform_buffer_standard_layout{};  ///< Support for scalar uniform buffer layouts.
    bool khr_spirv_1_4{};                       ///< Support for VK_KHR_spirv_1_4.
    bool khr_workgroup_memory_explicit_layout{}; ///< Support for explicit workgroup layouts.
    bool khr_push_descriptor{};                  ///< Support for VK_KHR_push_descritor.
    bool ext_index_type_uint8{};                 ///< Support for VK_EXT_index_type_uint8.
    bool ext_sampler_filter_minmax{};            ///< Support for VK_EXT_sampler_filter_minmax.
    bool ext_depth_range_unrestricted{};         ///< Support for VK_EXT_depth_range_unrestricted.
    bool ext_shader_viewport_index_layer{}; ///< Support for VK_EXT_shader_viewport_index_layer.
    bool ext_tooling_info{};                ///< Support for VK_EXT_tooling_info.
    bool ext_subgroup_size_control{};       ///< Support for VK_EXT_subgroup_size_control.
    bool ext_transform_feedback{};          ///< Support for VK_EXT_transform_feedback.
    bool ext_custom_border_color{};         ///< Support for VK_EXT_custom_border_color.
    bool ext_extended_dynamic_state{};      ///< Support for VK_EXT_extended_dynamic_state.
    bool ext_line_rasterization{};          ///< Support for VK_EXT_line_rasterization.
    bool ext_vertex_input_dynamic_state{};  ///< Support for VK_EXT_vertex_input_dynamic_state.
    bool ext_shader_stencil_export{};       ///< Support for VK_EXT_shader_stencil_export.
    bool ext_shader_atomic_int64{};         ///< Support for VK_KHR_shader_atomic_int64.
    bool ext_conservative_rasterization{};  ///< Support for VK_EXT_conservative_rasterization.
    bool ext_provoking_vertex{};            ///< Support for VK_EXT_provoking_vertex.
    bool nv_device_diagnostics_config{};    ///< Support for VK_NV_device_diagnostics_config.
    bool has_renderdoc{};                   ///< Has RenderDoc attached
    bool has_nsight_graphics{};             ///< Has Nsight Graphics attached

    // Telemetry parameters
    std::string vendor_name;                       ///< Device's driver name.
    std::vector<std::string> supported_extensions; ///< Reported Vulkan extensions.

    /// Format properties dictionary.
    std::unordered_map<VkFormat, VkFormatProperties> format_properties;

    /// Nsight Aftermath GPU crash tracker
    std::unique_ptr<NsightAftermathTracker> nsight_aftermath_tracker;
};

} // namespace Vulkan
