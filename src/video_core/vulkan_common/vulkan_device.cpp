// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <bitset>
#include <chrono>
#include <optional>
#include <string_view>
#include <thread>
#include <unordered_set>
#include <utility>
#include <vector>

#include "common/assert.h"
#include "common/settings.h"
#include "video_core/vulkan_common/nsight_aftermath_tracker.h"
#include "video_core/vulkan_common/vulkan_device.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Vulkan {
namespace {
namespace Alternatives {
constexpr std::array DEPTH24_UNORM_STENCIL8_UINT{
    VK_FORMAT_D32_SFLOAT_S8_UINT,
    VK_FORMAT_D16_UNORM_S8_UINT,
    VK_FORMAT_UNDEFINED,
};

constexpr std::array DEPTH16_UNORM_STENCIL8_UINT{
    VK_FORMAT_D24_UNORM_S8_UINT,
    VK_FORMAT_D32_SFLOAT_S8_UINT,
    VK_FORMAT_UNDEFINED,
};
} // namespace Alternatives

constexpr std::array REQUIRED_EXTENSIONS{
    VK_KHR_MAINTENANCE1_EXTENSION_NAME,
    VK_KHR_STORAGE_BUFFER_STORAGE_CLASS_EXTENSION_NAME,
    VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME,
    VK_KHR_16BIT_STORAGE_EXTENSION_NAME,
    VK_KHR_8BIT_STORAGE_EXTENSION_NAME,
    VK_KHR_DRIVER_PROPERTIES_EXTENSION_NAME,
    VK_KHR_DESCRIPTOR_UPDATE_TEMPLATE_EXTENSION_NAME,
    VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME,
    VK_KHR_SAMPLER_MIRROR_CLAMP_TO_EDGE_EXTENSION_NAME,
    VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME,
    VK_KHR_VARIABLE_POINTERS_EXTENSION_NAME,
    VK_EXT_VERTEX_ATTRIBUTE_DIVISOR_EXTENSION_NAME,
    VK_EXT_SHADER_SUBGROUP_BALLOT_EXTENSION_NAME,
    VK_EXT_SHADER_SUBGROUP_VOTE_EXTENSION_NAME,
    VK_EXT_ROBUSTNESS_2_EXTENSION_NAME,
    VK_EXT_HOST_QUERY_RESET_EXTENSION_NAME,
    VK_EXT_SHADER_DEMOTE_TO_HELPER_INVOCATION_EXTENSION_NAME,
#ifdef _WIN32
    VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME,
#endif
#ifdef __unix__
    VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
#endif
};

template <typename T>
void SetNext(void**& next, T& data) {
    *next = &data;
    next = &data.pNext;
}

constexpr const VkFormat* GetFormatAlternatives(VkFormat format) {
    switch (format) {
    case VK_FORMAT_D24_UNORM_S8_UINT:
        return Alternatives::DEPTH24_UNORM_STENCIL8_UINT.data();
    case VK_FORMAT_D16_UNORM_S8_UINT:
        return Alternatives::DEPTH16_UNORM_STENCIL8_UINT.data();
    default:
        return nullptr;
    }
}

VkFormatFeatureFlags GetFormatFeatures(VkFormatProperties properties, FormatType format_type) {
    switch (format_type) {
    case FormatType::Linear:
        return properties.linearTilingFeatures;
    case FormatType::Optimal:
        return properties.optimalTilingFeatures;
    case FormatType::Buffer:
        return properties.bufferFeatures;
    default:
        return {};
    }
}

std::unordered_map<VkFormat, VkFormatProperties> GetFormatProperties(vk::PhysicalDevice physical) {
    static constexpr std::array formats{
        VK_FORMAT_A8B8G8R8_UNORM_PACK32,
        VK_FORMAT_A8B8G8R8_UINT_PACK32,
        VK_FORMAT_A8B8G8R8_SNORM_PACK32,
        VK_FORMAT_A8B8G8R8_SINT_PACK32,
        VK_FORMAT_A8B8G8R8_SRGB_PACK32,
        VK_FORMAT_B5G6R5_UNORM_PACK16,
        VK_FORMAT_A2B10G10R10_UNORM_PACK32,
        VK_FORMAT_A2B10G10R10_UINT_PACK32,
        VK_FORMAT_A1R5G5B5_UNORM_PACK16,
        VK_FORMAT_R32G32B32A32_SFLOAT,
        VK_FORMAT_R32G32B32A32_SINT,
        VK_FORMAT_R32G32B32A32_UINT,
        VK_FORMAT_R32G32_SFLOAT,
        VK_FORMAT_R32G32_SINT,
        VK_FORMAT_R32G32_UINT,
        VK_FORMAT_R16G16B16A16_SINT,
        VK_FORMAT_R16G16B16A16_UINT,
        VK_FORMAT_R16G16B16A16_SNORM,
        VK_FORMAT_R16G16B16A16_UNORM,
        VK_FORMAT_R16G16_UNORM,
        VK_FORMAT_R16G16_SNORM,
        VK_FORMAT_R16G16_SFLOAT,
        VK_FORMAT_R16G16_SINT,
        VK_FORMAT_R16_UNORM,
        VK_FORMAT_R16_SNORM,
        VK_FORMAT_R16_UINT,
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_FORMAT_R8G8_UNORM,
        VK_FORMAT_R8G8_SNORM,
        VK_FORMAT_R8G8_SINT,
        VK_FORMAT_R8G8_UINT,
        VK_FORMAT_R8_UNORM,
        VK_FORMAT_R8_SNORM,
        VK_FORMAT_R8_SINT,
        VK_FORMAT_R8_UINT,
        VK_FORMAT_B10G11R11_UFLOAT_PACK32,
        VK_FORMAT_R32_SFLOAT,
        VK_FORMAT_R32_UINT,
        VK_FORMAT_R32_SINT,
        VK_FORMAT_R16_SFLOAT,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_FORMAT_B8G8R8A8_UNORM,
        VK_FORMAT_B8G8R8A8_SRGB,
        VK_FORMAT_R4G4B4A4_UNORM_PACK16,
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D16_UNORM,
        VK_FORMAT_D16_UNORM_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_BC1_RGBA_UNORM_BLOCK,
        VK_FORMAT_BC2_UNORM_BLOCK,
        VK_FORMAT_BC3_UNORM_BLOCK,
        VK_FORMAT_BC4_UNORM_BLOCK,
        VK_FORMAT_BC4_SNORM_BLOCK,
        VK_FORMAT_BC5_UNORM_BLOCK,
        VK_FORMAT_BC5_SNORM_BLOCK,
        VK_FORMAT_BC7_UNORM_BLOCK,
        VK_FORMAT_BC6H_UFLOAT_BLOCK,
        VK_FORMAT_BC6H_SFLOAT_BLOCK,
        VK_FORMAT_BC1_RGBA_SRGB_BLOCK,
        VK_FORMAT_BC2_SRGB_BLOCK,
        VK_FORMAT_BC3_SRGB_BLOCK,
        VK_FORMAT_BC7_SRGB_BLOCK,
        VK_FORMAT_ASTC_4x4_UNORM_BLOCK,
        VK_FORMAT_ASTC_4x4_SRGB_BLOCK,
        VK_FORMAT_ASTC_5x4_UNORM_BLOCK,
        VK_FORMAT_ASTC_5x4_SRGB_BLOCK,
        VK_FORMAT_ASTC_5x5_UNORM_BLOCK,
        VK_FORMAT_ASTC_5x5_SRGB_BLOCK,
        VK_FORMAT_ASTC_6x5_UNORM_BLOCK,
        VK_FORMAT_ASTC_6x5_SRGB_BLOCK,
        VK_FORMAT_ASTC_6x6_UNORM_BLOCK,
        VK_FORMAT_ASTC_6x6_SRGB_BLOCK,
        VK_FORMAT_ASTC_8x5_UNORM_BLOCK,
        VK_FORMAT_ASTC_8x5_SRGB_BLOCK,
        VK_FORMAT_ASTC_8x6_UNORM_BLOCK,
        VK_FORMAT_ASTC_8x6_SRGB_BLOCK,
        VK_FORMAT_ASTC_8x8_UNORM_BLOCK,
        VK_FORMAT_ASTC_8x8_SRGB_BLOCK,
        VK_FORMAT_ASTC_10x5_UNORM_BLOCK,
        VK_FORMAT_ASTC_10x5_SRGB_BLOCK,
        VK_FORMAT_ASTC_10x6_UNORM_BLOCK,
        VK_FORMAT_ASTC_10x6_SRGB_BLOCK,
        VK_FORMAT_ASTC_10x8_UNORM_BLOCK,
        VK_FORMAT_ASTC_10x8_SRGB_BLOCK,
        VK_FORMAT_ASTC_10x10_UNORM_BLOCK,
        VK_FORMAT_ASTC_10x10_SRGB_BLOCK,
        VK_FORMAT_ASTC_12x10_UNORM_BLOCK,
        VK_FORMAT_ASTC_12x10_SRGB_BLOCK,
        VK_FORMAT_ASTC_12x12_UNORM_BLOCK,
        VK_FORMAT_ASTC_12x12_SRGB_BLOCK,
        VK_FORMAT_ASTC_8x6_UNORM_BLOCK,
        VK_FORMAT_ASTC_8x6_SRGB_BLOCK,
        VK_FORMAT_ASTC_6x5_UNORM_BLOCK,
        VK_FORMAT_ASTC_6x5_SRGB_BLOCK,
        VK_FORMAT_E5B9G9R9_UFLOAT_PACK32,
    };
    std::unordered_map<VkFormat, VkFormatProperties> format_properties;
    for (const auto format : formats) {
        format_properties.emplace(format, physical.GetFormatProperties(format));
    }
    return format_properties;
}

std::vector<std::string> GetSupportedExtensions(vk::PhysicalDevice physical) {
    const std::vector extensions = physical.EnumerateDeviceExtensionProperties();
    std::vector<std::string> supported_extensions(std::size(extensions));
    for (const auto& extension : extensions) {
        supported_extensions.emplace_back(extension.extensionName);
    }
    return supported_extensions;
}

} // Anonymous namespace

Device::Device(VkInstance instance_, vk::PhysicalDevice physical_, VkSurfaceKHR surface,
               const vk::InstanceDispatch& dld_)
    : instance{instance_}, dld{dld_}, physical{physical_}, properties{physical.GetProperties()},
      supported_extensions{GetSupportedExtensions(physical)},
      format_properties(GetFormatProperties(physical)) {
    CheckSuitability(surface != nullptr);
    SetupFamilies(surface);
    SetupFeatures();
    SetupProperties();

    const auto queue_cis = GetDeviceQueueCreateInfos();
    const std::vector extensions = LoadExtensions(surface != nullptr);

    VkPhysicalDeviceFeatures2 features2{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = nullptr,
        .features{
            .robustBufferAccess = true,
            .fullDrawIndexUint32 = false,
            .imageCubeArray = true,
            .independentBlend = true,
            .geometryShader = true,
            .tessellationShader = true,
            .sampleRateShading = true,
            .dualSrcBlend = true,
            .logicOp = false,
            .multiDrawIndirect = false,
            .drawIndirectFirstInstance = false,
            .depthClamp = true,
            .depthBiasClamp = true,
            .fillModeNonSolid = true,
            .depthBounds = is_depth_bounds_supported,
            .wideLines = true,
            .largePoints = true,
            .alphaToOne = false,
            .multiViewport = true,
            .samplerAnisotropy = true,
            .textureCompressionETC2 = false,
            .textureCompressionASTC_LDR = is_optimal_astc_supported,
            .textureCompressionBC = false,
            .occlusionQueryPrecise = true,
            .pipelineStatisticsQuery = false,
            .vertexPipelineStoresAndAtomics = true,
            .fragmentStoresAndAtomics = true,
            .shaderTessellationAndGeometryPointSize = false,
            .shaderImageGatherExtended = true,
            .shaderStorageImageExtendedFormats = false,
            .shaderStorageImageMultisample = is_shader_storage_image_multisample,
            .shaderStorageImageReadWithoutFormat = is_formatless_image_load_supported,
            .shaderStorageImageWriteWithoutFormat = true,
            .shaderUniformBufferArrayDynamicIndexing = false,
            .shaderSampledImageArrayDynamicIndexing = false,
            .shaderStorageBufferArrayDynamicIndexing = false,
            .shaderStorageImageArrayDynamicIndexing = false,
            .shaderClipDistance = true,
            .shaderCullDistance = true,
            .shaderFloat64 = is_shader_float64_supported,
            .shaderInt64 = is_shader_int64_supported,
            .shaderInt16 = is_shader_int16_supported,
            .shaderResourceResidency = false,
            .shaderResourceMinLod = false,
            .sparseBinding = false,
            .sparseResidencyBuffer = false,
            .sparseResidencyImage2D = false,
            .sparseResidencyImage3D = false,
            .sparseResidency2Samples = false,
            .sparseResidency4Samples = false,
            .sparseResidency8Samples = false,
            .sparseResidency16Samples = false,
            .sparseResidencyAliased = false,
            .variableMultisampleRate = false,
            .inheritedQueries = false,
        },
    };
    const void* first_next = &features2;
    void** next = &features2.pNext;

    VkPhysicalDeviceTimelineSemaphoreFeaturesKHR timeline_semaphore{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES_KHR,
        .pNext = nullptr,
        .timelineSemaphore = true,
    };
    SetNext(next, timeline_semaphore);

    VkPhysicalDevice16BitStorageFeaturesKHR bit16_storage{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES_KHR,
        .pNext = nullptr,
        .storageBuffer16BitAccess = true,
        .uniformAndStorageBuffer16BitAccess = true,
        .storagePushConstant16 = false,
        .storageInputOutput16 = false,
    };
    SetNext(next, bit16_storage);

    VkPhysicalDevice8BitStorageFeaturesKHR bit8_storage{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES_KHR,
        .pNext = nullptr,
        .storageBuffer8BitAccess = false,
        .uniformAndStorageBuffer8BitAccess = true,
        .storagePushConstant8 = false,
    };
    SetNext(next, bit8_storage);

    VkPhysicalDeviceRobustness2FeaturesEXT robustness2{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT,
        .pNext = nullptr,
        .robustBufferAccess2 = true,
        .robustImageAccess2 = true,
        .nullDescriptor = true,
    };
    SetNext(next, robustness2);

    VkPhysicalDeviceHostQueryResetFeaturesEXT host_query_reset{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_QUERY_RESET_FEATURES_EXT,
        .pNext = nullptr,
        .hostQueryReset = true,
    };
    SetNext(next, host_query_reset);

    VkPhysicalDeviceVariablePointerFeaturesKHR variable_pointers{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VARIABLE_POINTERS_FEATURES_KHR,
        .pNext = nullptr,
        .variablePointersStorageBuffer = VK_TRUE,
        .variablePointers = VK_TRUE,
    };
    SetNext(next, variable_pointers);

    VkPhysicalDeviceShaderDemoteToHelperInvocationFeaturesEXT demote{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DEMOTE_TO_HELPER_INVOCATION_FEATURES_EXT,
        .pNext = nullptr,
        .shaderDemoteToHelperInvocation = true,
    };
    SetNext(next, demote);

    VkPhysicalDeviceFloat16Int8FeaturesKHR float16_int8;
    if (is_float16_supported) {
        float16_int8 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FLOAT16_INT8_FEATURES_KHR,
            .pNext = nullptr,
            .shaderFloat16 = true,
            .shaderInt8 = false,
        };
        SetNext(next, float16_int8);
    } else {
        LOG_INFO(Render_Vulkan, "Device doesn't support float16 natively");
    }

    if (!nv_viewport_swizzle) {
        LOG_INFO(Render_Vulkan, "Device doesn't support viewport swizzles");
    }

    if (!nv_viewport_array2) {
        LOG_INFO(Render_Vulkan, "Device doesn't support viewport masks");
    }

    if (!nv_geometry_shader_passthrough) {
        LOG_INFO(Render_Vulkan, "Device doesn't support passthrough geometry shaders");
    }

    VkPhysicalDeviceUniformBufferStandardLayoutFeaturesKHR std430_layout;
    if (khr_uniform_buffer_standard_layout) {
        std430_layout = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_UNIFORM_BUFFER_STANDARD_LAYOUT_FEATURES_KHR,
            .pNext = nullptr,
            .uniformBufferStandardLayout = true,
        };
        SetNext(next, std430_layout);
    } else {
        LOG_INFO(Render_Vulkan, "Device doesn't support packed UBOs");
    }

    VkPhysicalDeviceIndexTypeUint8FeaturesEXT index_type_uint8;
    if (ext_index_type_uint8) {
        index_type_uint8 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INDEX_TYPE_UINT8_FEATURES_EXT,
            .pNext = nullptr,
            .indexTypeUint8 = true,
        };
        SetNext(next, index_type_uint8);
    } else {
        LOG_INFO(Render_Vulkan, "Device doesn't support uint8 indexes");
    }

    VkPhysicalDeviceTransformFeedbackFeaturesEXT transform_feedback;
    if (ext_transform_feedback) {
        transform_feedback = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_FEATURES_EXT,
            .pNext = nullptr,
            .transformFeedback = true,
            .geometryStreams = true,
        };
        SetNext(next, transform_feedback);
    } else {
        LOG_INFO(Render_Vulkan, "Device doesn't support transform feedbacks");
    }

    VkPhysicalDeviceCustomBorderColorFeaturesEXT custom_border;
    if (ext_custom_border_color) {
        custom_border = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CUSTOM_BORDER_COLOR_FEATURES_EXT,
            .pNext = nullptr,
            .customBorderColors = VK_TRUE,
            .customBorderColorWithoutFormat = VK_TRUE,
        };
        SetNext(next, custom_border);
    } else {
        LOG_INFO(Render_Vulkan, "Device doesn't support custom border colors");
    }

    VkPhysicalDeviceExtendedDynamicStateFeaturesEXT dynamic_state;
    if (ext_extended_dynamic_state) {
        dynamic_state = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT,
            .pNext = nullptr,
            .extendedDynamicState = VK_TRUE,
        };
        SetNext(next, dynamic_state);
    } else {
        LOG_INFO(Render_Vulkan, "Device doesn't support extended dynamic state");
    }

    VkPhysicalDeviceLineRasterizationFeaturesEXT line_raster;
    if (ext_line_rasterization) {
        line_raster = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LINE_RASTERIZATION_FEATURES_EXT,
            .pNext = nullptr,
            .rectangularLines = VK_TRUE,
            .bresenhamLines = VK_FALSE,
            .smoothLines = VK_TRUE,
            .stippledRectangularLines = VK_FALSE,
            .stippledBresenhamLines = VK_FALSE,
            .stippledSmoothLines = VK_FALSE,
        };
        SetNext(next, line_raster);
    } else {
        LOG_INFO(Render_Vulkan, "Device doesn't support smooth lines");
    }

    if (!ext_conservative_rasterization) {
        LOG_INFO(Render_Vulkan, "Device doesn't support conservative rasterization");
    }

    VkPhysicalDeviceProvokingVertexFeaturesEXT provoking_vertex;
    if (ext_provoking_vertex) {
        provoking_vertex = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROVOKING_VERTEX_FEATURES_EXT,
            .pNext = nullptr,
            .provokingVertexLast = VK_TRUE,
            .transformFeedbackPreservesProvokingVertex = VK_TRUE,
        };
        SetNext(next, provoking_vertex);
    } else {
        LOG_INFO(Render_Vulkan, "Device doesn't support provoking vertex last");
    }

    VkPhysicalDeviceVertexInputDynamicStateFeaturesEXT vertex_input_dynamic;
    if (ext_vertex_input_dynamic_state) {
        vertex_input_dynamic = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_INPUT_DYNAMIC_STATE_FEATURES_EXT,
            .pNext = nullptr,
            .vertexInputDynamicState = VK_TRUE,
        };
        SetNext(next, vertex_input_dynamic);
    } else {
        LOG_INFO(Render_Vulkan, "Device doesn't support vertex input dynamic state");
    }

    VkPhysicalDeviceShaderAtomicInt64FeaturesKHR atomic_int64;
    if (ext_shader_atomic_int64) {
        atomic_int64 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_INT64_FEATURES_KHR,
            .pNext = nullptr,
            .shaderBufferInt64Atomics = VK_TRUE,
            .shaderSharedInt64Atomics = VK_TRUE,
        };
        SetNext(next, atomic_int64);
    }

    VkPhysicalDeviceWorkgroupMemoryExplicitLayoutFeaturesKHR workgroup_layout;
    if (khr_workgroup_memory_explicit_layout) {
        workgroup_layout = {
            .sType =
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_WORKGROUP_MEMORY_EXPLICIT_LAYOUT_FEATURES_KHR,
            .pNext = nullptr,
            .workgroupMemoryExplicitLayout = VK_TRUE,
            .workgroupMemoryExplicitLayoutScalarBlockLayout = VK_TRUE,
            .workgroupMemoryExplicitLayout8BitAccess = VK_TRUE,
            .workgroupMemoryExplicitLayout16BitAccess = VK_TRUE,
        };
        SetNext(next, workgroup_layout);
    }

    if (!ext_depth_range_unrestricted) {
        LOG_INFO(Render_Vulkan, "Device doesn't support depth range unrestricted");
    }

    VkDeviceDiagnosticsConfigCreateInfoNV diagnostics_nv;
    if (Settings::values.enable_nsight_aftermath && nv_device_diagnostics_config) {
        nsight_aftermath_tracker = std::make_unique<NsightAftermathTracker>();

        diagnostics_nv = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_DIAGNOSTICS_CONFIG_CREATE_INFO_NV,
            .pNext = &features2,
            .flags = VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_SHADER_DEBUG_INFO_BIT_NV |
                     VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_RESOURCE_TRACKING_BIT_NV |
                     VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_AUTOMATIC_CHECKPOINTS_BIT_NV,
        };
        first_next = &diagnostics_nv;
    }
    logical = vk::Device::Create(physical, queue_cis, extensions, first_next, dld);

    CollectPhysicalMemoryInfo();
    CollectTelemetryParameters();
    CollectToolingInfo();

    if (driver_id == VK_DRIVER_ID_NVIDIA_PROPRIETARY_KHR && is_float16_supported) {
        if (std::ranges::find(supported_extensions, VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME) !=
            supported_extensions.end()) {
            LOG_WARNING(Render_Vulkan, "Blacklisting Ampere devices from float16 math");
            is_float16_supported = false;
        }
    }
    if (ext_extended_dynamic_state && driver_id == VK_DRIVER_ID_MESA_RADV) {
        // Mask driver version variant
        const u32 version = (properties.driverVersion << 3) >> 3;
        if (version < VK_MAKE_API_VERSION(0, 21, 2, 0)) {
            LOG_WARNING(Render_Vulkan,
                        "RADV versions older than 21.2 have broken VK_EXT_extended_dynamic_state");
            ext_extended_dynamic_state = false;
        }
    }
    if (ext_vertex_input_dynamic_state && driver_id == VK_DRIVER_ID_INTEL_PROPRIETARY_WINDOWS) {
        LOG_WARNING(Render_Vulkan, "Blacklisting Intel for VK_EXT_vertex_input_dynamic_state");
        ext_vertex_input_dynamic_state = false;
    }
    if (is_float16_supported && driver_id == VK_DRIVER_ID_INTEL_PROPRIETARY_WINDOWS) {
        // Intel's compiler crashes when using fp16 on Astral Chain, disable it for the time being.
        LOG_WARNING(Render_Vulkan, "Blacklisting Intel proprietary from float16 math");
        is_float16_supported = false;
    }

    graphics_queue = logical.GetQueue(graphics_family);
    present_queue = logical.GetQueue(present_family);
}

Device::~Device() = default;

VkFormat Device::GetSupportedFormat(VkFormat wanted_format, VkFormatFeatureFlags wanted_usage,
                                    FormatType format_type) const {
    if (IsFormatSupported(wanted_format, wanted_usage, format_type)) {
        return wanted_format;
    }
    // The wanted format is not supported by hardware, search for alternatives
    const VkFormat* alternatives = GetFormatAlternatives(wanted_format);
    if (alternatives == nullptr) {
        UNREACHABLE_MSG("Format={} with usage={} and type={} has no defined alternatives and host "
                        "hardware does not support it",
                        wanted_format, wanted_usage, format_type);
        return wanted_format;
    }

    std::size_t i = 0;
    for (VkFormat alternative = *alternatives; alternative; alternative = alternatives[++i]) {
        if (!IsFormatSupported(alternative, wanted_usage, format_type)) {
            continue;
        }
        LOG_WARNING(Render_Vulkan,
                    "Emulating format={} with alternative format={} with usage={} and type={}",
                    wanted_format, alternative, wanted_usage, format_type);
        return alternative;
    }

    // No alternatives found, panic
    UNREACHABLE_MSG("Format={} with usage={} and type={} is not supported by the host hardware and "
                    "doesn't support any of the alternatives",
                    wanted_format, wanted_usage, format_type);
    return wanted_format;
}

void Device::ReportLoss() const {
    LOG_CRITICAL(Render_Vulkan, "Device loss occured!");

    // Wait for the log to flush and for Nsight Aftermath to dump the results
    std::this_thread::sleep_for(std::chrono::seconds{15});
}

void Device::SaveShader(std::span<const u32> spirv) const {
    if (nsight_aftermath_tracker) {
        nsight_aftermath_tracker->SaveShader(spirv);
    }
}

bool Device::IsOptimalAstcSupported(const VkPhysicalDeviceFeatures& features) const {
    // Disable for now to avoid converting ASTC twice.
    static constexpr std::array astc_formats = {
        VK_FORMAT_ASTC_4x4_UNORM_BLOCK,   VK_FORMAT_ASTC_4x4_SRGB_BLOCK,
        VK_FORMAT_ASTC_5x4_UNORM_BLOCK,   VK_FORMAT_ASTC_5x4_SRGB_BLOCK,
        VK_FORMAT_ASTC_5x5_UNORM_BLOCK,   VK_FORMAT_ASTC_5x5_SRGB_BLOCK,
        VK_FORMAT_ASTC_6x5_UNORM_BLOCK,   VK_FORMAT_ASTC_6x5_SRGB_BLOCK,
        VK_FORMAT_ASTC_6x6_UNORM_BLOCK,   VK_FORMAT_ASTC_6x6_SRGB_BLOCK,
        VK_FORMAT_ASTC_8x5_UNORM_BLOCK,   VK_FORMAT_ASTC_8x5_SRGB_BLOCK,
        VK_FORMAT_ASTC_8x6_UNORM_BLOCK,   VK_FORMAT_ASTC_8x6_SRGB_BLOCK,
        VK_FORMAT_ASTC_8x8_UNORM_BLOCK,   VK_FORMAT_ASTC_8x8_SRGB_BLOCK,
        VK_FORMAT_ASTC_10x5_UNORM_BLOCK,  VK_FORMAT_ASTC_10x5_SRGB_BLOCK,
        VK_FORMAT_ASTC_10x6_UNORM_BLOCK,  VK_FORMAT_ASTC_10x6_SRGB_BLOCK,
        VK_FORMAT_ASTC_10x8_UNORM_BLOCK,  VK_FORMAT_ASTC_10x8_SRGB_BLOCK,
        VK_FORMAT_ASTC_10x10_UNORM_BLOCK, VK_FORMAT_ASTC_10x10_SRGB_BLOCK,
        VK_FORMAT_ASTC_12x10_UNORM_BLOCK, VK_FORMAT_ASTC_12x10_SRGB_BLOCK,
        VK_FORMAT_ASTC_12x12_UNORM_BLOCK, VK_FORMAT_ASTC_12x12_SRGB_BLOCK,
    };
    if (!features.textureCompressionASTC_LDR) {
        return false;
    }
    const auto format_feature_usage{
        VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_BLIT_SRC_BIT |
        VK_FORMAT_FEATURE_BLIT_DST_BIT | VK_FORMAT_FEATURE_TRANSFER_SRC_BIT |
        VK_FORMAT_FEATURE_TRANSFER_DST_BIT};
    for (const auto format : astc_formats) {
        const auto physical_format_properties{physical.GetFormatProperties(format)};
        if ((physical_format_properties.optimalTilingFeatures & format_feature_usage) == 0) {
            return false;
        }
    }
    return true;
}

bool Device::TestDepthStencilBlits() const {
    static constexpr VkFormatFeatureFlags required_features =
        VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT;
    const auto test_features = [](VkFormatProperties props) {
        return (props.optimalTilingFeatures & required_features) == required_features;
    };
    return test_features(format_properties.at(VK_FORMAT_D32_SFLOAT_S8_UINT)) &&
           test_features(format_properties.at(VK_FORMAT_D24_UNORM_S8_UINT));
}

bool Device::IsFormatSupported(VkFormat wanted_format, VkFormatFeatureFlags wanted_usage,
                               FormatType format_type) const {
    const auto it = format_properties.find(wanted_format);
    if (it == format_properties.end()) {
        UNIMPLEMENTED_MSG("Unimplemented format query={}", wanted_format);
        return true;
    }
    const auto supported_usage = GetFormatFeatures(it->second, format_type);
    return (supported_usage & wanted_usage) == wanted_usage;
}

std::string Device::GetDriverName() const {
    switch (driver_id) {
    case VK_DRIVER_ID_AMD_PROPRIETARY:
        return "AMD";
    case VK_DRIVER_ID_AMD_OPEN_SOURCE:
        return "AMDVLK";
    case VK_DRIVER_ID_MESA_RADV:
        return "RADV";
    case VK_DRIVER_ID_NVIDIA_PROPRIETARY:
        return "NVIDIA";
    case VK_DRIVER_ID_INTEL_PROPRIETARY_WINDOWS:
        return "INTEL";
    case VK_DRIVER_ID_INTEL_OPEN_SOURCE_MESA:
        return "ANV";
    case VK_DRIVER_ID_MESA_LLVMPIPE:
        return "LAVAPIPE";
    default:
        return vendor_name;
    }
}

void Device::CheckSuitability(bool requires_swapchain) const {
    std::bitset<REQUIRED_EXTENSIONS.size()> available_extensions;
    bool has_swapchain = false;
    for (const VkExtensionProperties& property : physical.EnumerateDeviceExtensionProperties()) {
        const std::string_view name{property.extensionName};
        for (size_t i = 0; i < REQUIRED_EXTENSIONS.size(); ++i) {
            if (available_extensions[i]) {
                continue;
            }
            available_extensions[i] = name == REQUIRED_EXTENSIONS[i];
        }
        has_swapchain = has_swapchain || name == VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    }
    for (size_t i = 0; i < REQUIRED_EXTENSIONS.size(); ++i) {
        if (available_extensions[i]) {
            continue;
        }
        LOG_ERROR(Render_Vulkan, "Missing required extension: {}", REQUIRED_EXTENSIONS[i]);
        throw vk::Exception(VK_ERROR_EXTENSION_NOT_PRESENT);
    }
    if (requires_swapchain && !has_swapchain) {
        LOG_ERROR(Render_Vulkan, "Missing required extension: VK_KHR_swapchain");
        throw vk::Exception(VK_ERROR_EXTENSION_NOT_PRESENT);
    }

    struct LimitTuple {
        u32 minimum;
        u32 value;
        const char* name;
    };
    const VkPhysicalDeviceLimits& limits{properties.limits};
    const std::array limits_report{
        LimitTuple{65536, limits.maxUniformBufferRange, "maxUniformBufferRange"},
        LimitTuple{16, limits.maxViewports, "maxViewports"},
        LimitTuple{8, limits.maxColorAttachments, "maxColorAttachments"},
        LimitTuple{8, limits.maxClipDistances, "maxClipDistances"},
    };
    for (const auto& tuple : limits_report) {
        if (tuple.value < tuple.minimum) {
            LOG_ERROR(Render_Vulkan, "{} has to be {} or greater but it is {}", tuple.name,
                      tuple.minimum, tuple.value);
            throw vk::Exception(VK_ERROR_FEATURE_NOT_PRESENT);
        }
    }
    VkPhysicalDeviceShaderDemoteToHelperInvocationFeaturesEXT demote{};
    demote.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DEMOTE_TO_HELPER_INVOCATION_FEATURES_EXT;
    demote.pNext = nullptr;

    VkPhysicalDeviceVariablePointerFeaturesKHR variable_pointers{};
    variable_pointers.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VARIABLE_POINTERS_FEATURES_KHR;
    variable_pointers.pNext = &demote;

    VkPhysicalDeviceRobustness2FeaturesEXT robustness2{};
    robustness2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT;
    robustness2.pNext = &variable_pointers;

    VkPhysicalDeviceFeatures2KHR features2{};
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.pNext = &robustness2;

    physical.GetFeatures2KHR(features2);

    const VkPhysicalDeviceFeatures& features{features2.features};
    const std::array feature_report{
        std::make_pair(features.robustBufferAccess, "robustBufferAccess"),
        std::make_pair(features.vertexPipelineStoresAndAtomics, "vertexPipelineStoresAndAtomics"),
        std::make_pair(features.imageCubeArray, "imageCubeArray"),
        std::make_pair(features.independentBlend, "independentBlend"),
        std::make_pair(features.depthClamp, "depthClamp"),
        std::make_pair(features.samplerAnisotropy, "samplerAnisotropy"),
        std::make_pair(features.largePoints, "largePoints"),
        std::make_pair(features.multiViewport, "multiViewport"),
        std::make_pair(features.depthBiasClamp, "depthBiasClamp"),
        std::make_pair(features.fillModeNonSolid, "fillModeNonSolid"),
        std::make_pair(features.wideLines, "wideLines"),
        std::make_pair(features.geometryShader, "geometryShader"),
        std::make_pair(features.tessellationShader, "tessellationShader"),
        std::make_pair(features.sampleRateShading, "sampleRateShading"),
        std::make_pair(features.dualSrcBlend, "dualSrcBlend"),
        std::make_pair(features.occlusionQueryPrecise, "occlusionQueryPrecise"),
        std::make_pair(features.fragmentStoresAndAtomics, "fragmentStoresAndAtomics"),
        std::make_pair(features.shaderImageGatherExtended, "shaderImageGatherExtended"),
        std::make_pair(features.shaderStorageImageWriteWithoutFormat,
                       "shaderStorageImageWriteWithoutFormat"),
        std::make_pair(features.shaderClipDistance, "shaderClipDistance"),
        std::make_pair(features.shaderCullDistance, "shaderCullDistance"),
        std::make_pair(demote.shaderDemoteToHelperInvocation, "shaderDemoteToHelperInvocation"),
        std::make_pair(variable_pointers.variablePointers, "variablePointers"),
        std::make_pair(variable_pointers.variablePointersStorageBuffer,
                       "variablePointersStorageBuffer"),
        std::make_pair(robustness2.robustBufferAccess2, "robustBufferAccess2"),
        std::make_pair(robustness2.robustImageAccess2, "robustImageAccess2"),
        std::make_pair(robustness2.nullDescriptor, "nullDescriptor"),
    };
    for (const auto& [is_supported, name] : feature_report) {
        if (is_supported) {
            continue;
        }
        LOG_ERROR(Render_Vulkan, "Missing required feature: {}", name);
        throw vk::Exception(VK_ERROR_FEATURE_NOT_PRESENT);
    }
}

std::vector<const char*> Device::LoadExtensions(bool requires_surface) {
    std::vector<const char*> extensions;
    extensions.reserve(8 + REQUIRED_EXTENSIONS.size());
    extensions.insert(extensions.begin(), REQUIRED_EXTENSIONS.begin(), REQUIRED_EXTENSIONS.end());
    if (requires_surface) {
        extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    }

    bool has_khr_shader_float16_int8{};
    bool has_khr_workgroup_memory_explicit_layout{};
    bool has_ext_subgroup_size_control{};
    bool has_ext_transform_feedback{};
    bool has_ext_custom_border_color{};
    bool has_ext_extended_dynamic_state{};
    bool has_ext_shader_atomic_int64{};
    bool has_ext_provoking_vertex{};
    bool has_ext_vertex_input_dynamic_state{};
    bool has_ext_line_rasterization{};
    for (const std::string& extension : supported_extensions) {
        const auto test = [&](std::optional<std::reference_wrapper<bool>> status, const char* name,
                              bool push) {
            if (extension != name) {
                return;
            }
            if (push) {
                extensions.push_back(name);
            }
            if (status) {
                status->get() = true;
            }
        };
        test(nv_viewport_swizzle, VK_NV_VIEWPORT_SWIZZLE_EXTENSION_NAME, true);
        test(nv_viewport_array2, VK_NV_VIEWPORT_ARRAY2_EXTENSION_NAME, true);
        test(nv_geometry_shader_passthrough, VK_NV_GEOMETRY_SHADER_PASSTHROUGH_EXTENSION_NAME,
             true);
        test(khr_uniform_buffer_standard_layout,
             VK_KHR_UNIFORM_BUFFER_STANDARD_LAYOUT_EXTENSION_NAME, true);
        test(khr_spirv_1_4, VK_KHR_SPIRV_1_4_EXTENSION_NAME, true);
        test(khr_push_descriptor, VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME, true);
        test(has_khr_shader_float16_int8, VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME, false);
        test(ext_depth_range_unrestricted, VK_EXT_DEPTH_RANGE_UNRESTRICTED_EXTENSION_NAME, true);
        test(ext_index_type_uint8, VK_EXT_INDEX_TYPE_UINT8_EXTENSION_NAME, true);
        test(ext_sampler_filter_minmax, VK_EXT_SAMPLER_FILTER_MINMAX_EXTENSION_NAME, true);
        test(ext_shader_viewport_index_layer, VK_EXT_SHADER_VIEWPORT_INDEX_LAYER_EXTENSION_NAME,
             true);
        test(ext_tooling_info, VK_EXT_TOOLING_INFO_EXTENSION_NAME, true);
        test(ext_shader_stencil_export, VK_EXT_SHADER_STENCIL_EXPORT_EXTENSION_NAME, true);
        test(ext_conservative_rasterization, VK_EXT_CONSERVATIVE_RASTERIZATION_EXTENSION_NAME,
             true);
        test(has_ext_transform_feedback, VK_EXT_TRANSFORM_FEEDBACK_EXTENSION_NAME, false);
        test(has_ext_custom_border_color, VK_EXT_CUSTOM_BORDER_COLOR_EXTENSION_NAME, false);
        test(has_ext_extended_dynamic_state, VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME, false);
        test(has_ext_subgroup_size_control, VK_EXT_SUBGROUP_SIZE_CONTROL_EXTENSION_NAME, false);
        test(has_ext_provoking_vertex, VK_EXT_PROVOKING_VERTEX_EXTENSION_NAME, false);
        test(has_ext_vertex_input_dynamic_state, VK_EXT_VERTEX_INPUT_DYNAMIC_STATE_EXTENSION_NAME,
             false);
        test(has_ext_shader_atomic_int64, VK_KHR_SHADER_ATOMIC_INT64_EXTENSION_NAME, false);
        test(has_khr_workgroup_memory_explicit_layout,
             VK_KHR_WORKGROUP_MEMORY_EXPLICIT_LAYOUT_EXTENSION_NAME, false);
        test(has_ext_line_rasterization, VK_EXT_LINE_RASTERIZATION_EXTENSION_NAME, false);
        if (Settings::values.enable_nsight_aftermath) {
            test(nv_device_diagnostics_config, VK_NV_DEVICE_DIAGNOSTICS_CONFIG_EXTENSION_NAME,
                 true);
        }
    }
    VkPhysicalDeviceFeatures2KHR features{};
    features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR;

    VkPhysicalDeviceProperties2KHR physical_properties;
    physical_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR;

    if (has_khr_shader_float16_int8) {
        VkPhysicalDeviceFloat16Int8FeaturesKHR float16_int8_features;
        float16_int8_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FLOAT16_INT8_FEATURES_KHR;
        float16_int8_features.pNext = nullptr;
        features.pNext = &float16_int8_features;

        physical.GetFeatures2KHR(features);
        is_float16_supported = float16_int8_features.shaderFloat16;
        extensions.push_back(VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME);
    }
    if (has_ext_subgroup_size_control) {
        VkPhysicalDeviceSubgroupSizeControlFeaturesEXT subgroup_features;
        subgroup_features.sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_FEATURES_EXT;
        subgroup_features.pNext = nullptr;
        features.pNext = &subgroup_features;
        physical.GetFeatures2KHR(features);

        VkPhysicalDeviceSubgroupSizeControlPropertiesEXT subgroup_properties;
        subgroup_properties.sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_PROPERTIES_EXT;
        subgroup_properties.pNext = nullptr;
        physical_properties.pNext = &subgroup_properties;
        physical.GetProperties2KHR(physical_properties);

        is_warp_potentially_bigger = subgroup_properties.maxSubgroupSize > GuestWarpSize;

        if (subgroup_features.subgroupSizeControl &&
            subgroup_properties.minSubgroupSize <= GuestWarpSize &&
            subgroup_properties.maxSubgroupSize >= GuestWarpSize) {
            extensions.push_back(VK_EXT_SUBGROUP_SIZE_CONTROL_EXTENSION_NAME);
            guest_warp_stages = subgroup_properties.requiredSubgroupSizeStages;
            ext_subgroup_size_control = true;
        }
    } else {
        is_warp_potentially_bigger = true;
    }
    if (has_ext_provoking_vertex) {
        VkPhysicalDeviceProvokingVertexFeaturesEXT provoking_vertex;
        provoking_vertex.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROVOKING_VERTEX_FEATURES_EXT;
        provoking_vertex.pNext = nullptr;
        features.pNext = &provoking_vertex;
        physical.GetFeatures2KHR(features);

        if (provoking_vertex.provokingVertexLast &&
            provoking_vertex.transformFeedbackPreservesProvokingVertex) {
            extensions.push_back(VK_EXT_PROVOKING_VERTEX_EXTENSION_NAME);
            ext_provoking_vertex = true;
        }
    }
    if (has_ext_vertex_input_dynamic_state) {
        VkPhysicalDeviceVertexInputDynamicStateFeaturesEXT vertex_input;
        vertex_input.sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_INPUT_DYNAMIC_STATE_FEATURES_EXT;
        vertex_input.pNext = nullptr;
        features.pNext = &vertex_input;
        physical.GetFeatures2KHR(features);

        if (vertex_input.vertexInputDynamicState) {
            extensions.push_back(VK_EXT_VERTEX_INPUT_DYNAMIC_STATE_EXTENSION_NAME);
            ext_vertex_input_dynamic_state = true;
        }
    }
    if (has_ext_shader_atomic_int64) {
        VkPhysicalDeviceShaderAtomicInt64Features atomic_int64;
        atomic_int64.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT;
        atomic_int64.pNext = nullptr;
        features.pNext = &atomic_int64;
        physical.GetFeatures2KHR(features);

        if (atomic_int64.shaderBufferInt64Atomics && atomic_int64.shaderSharedInt64Atomics) {
            extensions.push_back(VK_KHR_SHADER_ATOMIC_INT64_EXTENSION_NAME);
            ext_shader_atomic_int64 = true;
        }
    }
    if (has_ext_transform_feedback) {
        VkPhysicalDeviceTransformFeedbackFeaturesEXT tfb_features;
        tfb_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_FEATURES_EXT;
        tfb_features.pNext = nullptr;
        features.pNext = &tfb_features;
        physical.GetFeatures2KHR(features);

        VkPhysicalDeviceTransformFeedbackPropertiesEXT tfb_properties;
        tfb_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_PROPERTIES_EXT;
        tfb_properties.pNext = nullptr;
        physical_properties.pNext = &tfb_properties;
        physical.GetProperties2KHR(physical_properties);

        if (tfb_features.transformFeedback && tfb_features.geometryStreams &&
            tfb_properties.maxTransformFeedbackStreams >= 4 &&
            tfb_properties.maxTransformFeedbackBuffers && tfb_properties.transformFeedbackQueries &&
            tfb_properties.transformFeedbackDraw) {
            extensions.push_back(VK_EXT_TRANSFORM_FEEDBACK_EXTENSION_NAME);
            ext_transform_feedback = true;
        }
    }
    if (has_ext_custom_border_color) {
        VkPhysicalDeviceCustomBorderColorFeaturesEXT border_features;
        border_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CUSTOM_BORDER_COLOR_FEATURES_EXT;
        border_features.pNext = nullptr;
        features.pNext = &border_features;
        physical.GetFeatures2KHR(features);

        if (border_features.customBorderColors && border_features.customBorderColorWithoutFormat) {
            extensions.push_back(VK_EXT_CUSTOM_BORDER_COLOR_EXTENSION_NAME);
            ext_custom_border_color = true;
        }
    }
    if (has_ext_extended_dynamic_state) {
        VkPhysicalDeviceExtendedDynamicStateFeaturesEXT extended_dynamic_state;
        extended_dynamic_state.sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT;
        extended_dynamic_state.pNext = nullptr;
        features.pNext = &extended_dynamic_state;
        physical.GetFeatures2KHR(features);

        if (extended_dynamic_state.extendedDynamicState) {
            extensions.push_back(VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME);
            ext_extended_dynamic_state = true;
        }
    }
    if (has_ext_line_rasterization) {
        VkPhysicalDeviceLineRasterizationFeaturesEXT line_raster;
        line_raster.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LINE_RASTERIZATION_FEATURES_EXT;
        line_raster.pNext = nullptr;
        features.pNext = &line_raster;
        physical.GetFeatures2KHR(features);
        if (line_raster.rectangularLines && line_raster.smoothLines) {
            extensions.push_back(VK_EXT_LINE_RASTERIZATION_EXTENSION_NAME);
            ext_line_rasterization = true;
        }
    }
    if (has_khr_workgroup_memory_explicit_layout) {
        VkPhysicalDeviceWorkgroupMemoryExplicitLayoutFeaturesKHR layout;
        layout.sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_WORKGROUP_MEMORY_EXPLICIT_LAYOUT_FEATURES_KHR;
        layout.pNext = nullptr;
        features.pNext = &layout;
        physical.GetFeatures2KHR(features);

        if (layout.workgroupMemoryExplicitLayout &&
            layout.workgroupMemoryExplicitLayout8BitAccess &&
            layout.workgroupMemoryExplicitLayout16BitAccess &&
            layout.workgroupMemoryExplicitLayoutScalarBlockLayout) {
            extensions.push_back(VK_KHR_WORKGROUP_MEMORY_EXPLICIT_LAYOUT_EXTENSION_NAME);
            khr_workgroup_memory_explicit_layout = true;
        }
    }
    if (khr_push_descriptor) {
        VkPhysicalDevicePushDescriptorPropertiesKHR push_descriptor;
        push_descriptor.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PUSH_DESCRIPTOR_PROPERTIES_KHR;
        push_descriptor.pNext = nullptr;

        physical_properties.pNext = &push_descriptor;
        physical.GetProperties2KHR(physical_properties);

        max_push_descriptors = push_descriptor.maxPushDescriptors;
    }
    return extensions;
}

void Device::SetupFamilies(VkSurfaceKHR surface) {
    const std::vector queue_family_properties = physical.GetQueueFamilyProperties();
    std::optional<u32> graphics;
    std::optional<u32> present;
    for (u32 index = 0; index < static_cast<u32>(queue_family_properties.size()); ++index) {
        if (graphics && (present || !surface)) {
            break;
        }
        const VkQueueFamilyProperties& queue_family = queue_family_properties[index];
        if (queue_family.queueCount == 0) {
            continue;
        }
        if (queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            graphics = index;
        }
        if (surface && physical.GetSurfaceSupportKHR(index, surface)) {
            present = index;
        }
    }
    if (!graphics) {
        LOG_ERROR(Render_Vulkan, "Device lacks a graphics queue");
        throw vk::Exception(VK_ERROR_FEATURE_NOT_PRESENT);
    }
    if (surface && !present) {
        LOG_ERROR(Render_Vulkan, "Device lacks a present queue");
        throw vk::Exception(VK_ERROR_FEATURE_NOT_PRESENT);
    }
    graphics_family = *graphics;
    present_family = *present;
}

void Device::SetupFeatures() {
    const VkPhysicalDeviceFeatures features{physical.GetFeatures()};
    is_depth_bounds_supported = features.depthBounds;
    is_formatless_image_load_supported = features.shaderStorageImageReadWithoutFormat;
    is_shader_float64_supported = features.shaderFloat64;
    is_shader_int64_supported = features.shaderInt64;
    is_shader_int16_supported = features.shaderInt16;
    is_shader_storage_image_multisample = features.shaderStorageImageMultisample;
    is_blit_depth_stencil_supported = TestDepthStencilBlits();
    is_optimal_astc_supported = IsOptimalAstcSupported(features);
}

void Device::SetupProperties() {
    float_controls.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FLOAT_CONTROLS_PROPERTIES_KHR;

    VkPhysicalDeviceProperties2KHR properties2{};
    properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR;
    properties2.pNext = &float_controls;

    physical.GetProperties2KHR(properties2);
}

void Device::CollectTelemetryParameters() {
    VkPhysicalDeviceDriverPropertiesKHR driver{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES_KHR,
        .pNext = nullptr,
        .driverID = {},
        .driverName = {},
        .driverInfo = {},
        .conformanceVersion = {},
    };

    VkPhysicalDeviceProperties2KHR device_properties{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR,
        .pNext = &driver,
        .properties = {},
    };
    physical.GetProperties2KHR(device_properties);

    driver_id = driver.driverID;
    vendor_name = driver.driverName;
}

void Device::CollectPhysicalMemoryInfo() {
    const auto mem_properties = physical.GetMemoryProperties();
    const size_t num_properties = mem_properties.memoryHeapCount;
    device_access_memory = 0;
    for (size_t element = 0; element < num_properties; ++element) {
        if ((mem_properties.memoryHeaps[element].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) != 0) {
            device_access_memory += mem_properties.memoryHeaps[element].size;
        }
    }
}

void Device::CollectToolingInfo() {
    if (!ext_tooling_info) {
        return;
    }
    const auto vkGetPhysicalDeviceToolPropertiesEXT =
        reinterpret_cast<PFN_vkGetPhysicalDeviceToolPropertiesEXT>(
            dld.vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceToolPropertiesEXT"));
    if (!vkGetPhysicalDeviceToolPropertiesEXT) {
        return;
    }
    u32 tool_count = 0;
    if (vkGetPhysicalDeviceToolPropertiesEXT(physical, &tool_count, nullptr) != VK_SUCCESS) {
        return;
    }
    std::vector<VkPhysicalDeviceToolPropertiesEXT> tools(tool_count);
    if (vkGetPhysicalDeviceToolPropertiesEXT(physical, &tool_count, tools.data()) != VK_SUCCESS) {
        return;
    }
    for (const VkPhysicalDeviceToolPropertiesEXT& tool : tools) {
        const std::string_view name = tool.name;
        LOG_INFO(Render_Vulkan, "{}", name);
        has_renderdoc = has_renderdoc || name == "RenderDoc";
        has_nsight_graphics = has_nsight_graphics || name == "NVIDIA Nsight Graphics";
    }
}

std::vector<VkDeviceQueueCreateInfo> Device::GetDeviceQueueCreateInfos() const {
    static constexpr float QUEUE_PRIORITY = 1.0f;

    std::unordered_set<u32> unique_queue_families{graphics_family, present_family};
    std::vector<VkDeviceQueueCreateInfo> queue_cis;
    queue_cis.reserve(unique_queue_families.size());

    for (const u32 queue_family : unique_queue_families) {
        auto& ci = queue_cis.emplace_back(VkDeviceQueueCreateInfo{
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .queueFamilyIndex = queue_family,
            .queueCount = 1,
            .pQueuePriorities = nullptr,
        });
        ci.pQueuePriorities = &QUEUE_PRIORITY;
    }

    return queue_cis;
}

} // namespace Vulkan
