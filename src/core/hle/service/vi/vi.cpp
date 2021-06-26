// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <array>
#include <cstring>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

#include "common/alignment.h"
#include "common/assert.h"
#include "common/common_funcs.h"
#include "common/logging/log.h"
#include "common/math_util.h"
#include "common/settings.h"
#include "common/swap.h"
#include "core/core_timing.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/k_readable_event.h"
#include "core/hle/kernel/k_thread.h"
#include "core/hle/kernel/k_writable_event.h"
#include "core/hle/service/nvdrv/nvdata.h"
#include "core/hle/service/nvdrv/nvdrv.h"
#include "core/hle/service/nvflinger/buffer_queue.h"
#include "core/hle/service/nvflinger/nvflinger.h"
#include "core/hle/service/service.h"
#include "core/hle/service/vi/vi.h"
#include "core/hle/service/vi/vi_m.h"
#include "core/hle/service/vi/vi_s.h"
#include "core/hle/service/vi/vi_u.h"

namespace Service::VI {

constexpr ResultCode ERR_OPERATION_FAILED{ErrorModule::VI, 1};
constexpr ResultCode ERR_PERMISSION_DENIED{ErrorModule::VI, 5};
constexpr ResultCode ERR_UNSUPPORTED{ErrorModule::VI, 6};
constexpr ResultCode ERR_NOT_FOUND{ErrorModule::VI, 7};

struct DisplayInfo {
    /// The name of this particular display.
    char display_name[0x40]{"Default"};

    /// Whether or not the display has a limited number of layers.
    u8 has_limited_layers{1};
    INSERT_PADDING_BYTES(7);

    /// Indicates the total amount of layers supported by the display.
    /// @note This is only valid if has_limited_layers is set.
    u64 max_layers{1};

    /// Maximum width in pixels.
    u64 width{1920};

    /// Maximum height in pixels.
    u64 height{1080};
};
static_assert(sizeof(DisplayInfo) == 0x60, "DisplayInfo has wrong size");

class Parcel {
public:
    // This default size was chosen arbitrarily.
    static constexpr std::size_t DefaultBufferSize = 0x40;
    Parcel() : buffer(DefaultBufferSize) {}
    explicit Parcel(std::vector<u8> data) : buffer(std::move(data)) {}
    virtual ~Parcel() = default;

    template <typename T>
    T Read() {
        static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable.");
        ASSERT(read_index + sizeof(T) <= buffer.size());

        T val;
        std::memcpy(&val, buffer.data() + read_index, sizeof(T));
        read_index += sizeof(T);
        read_index = Common::AlignUp(read_index, 4);
        return val;
    }

    template <typename T>
    T ReadUnaligned() {
        static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable.");
        ASSERT(read_index + sizeof(T) <= buffer.size());

        T val;
        std::memcpy(&val, buffer.data() + read_index, sizeof(T));
        read_index += sizeof(T);
        return val;
    }

    std::vector<u8> ReadBlock(std::size_t length) {
        ASSERT(read_index + length <= buffer.size());
        const u8* const begin = buffer.data() + read_index;
        const u8* const end = begin + length;
        std::vector<u8> data(begin, end);
        read_index += length;
        read_index = Common::AlignUp(read_index, 4);
        return data;
    }

    std::u16string ReadInterfaceToken() {
        [[maybe_unused]] const u32 unknown = Read<u32_le>();
        const u32 length = Read<u32_le>();

        std::u16string token{};

        for (u32 ch = 0; ch < length + 1; ++ch) {
            token.push_back(ReadUnaligned<u16_le>());
        }

        read_index = Common::AlignUp(read_index, 4);

        return token;
    }

    template <typename T>
    void Write(const T& val) {
        static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable.");

        if (buffer.size() < write_index + sizeof(T)) {
            buffer.resize(buffer.size() + sizeof(T) + DefaultBufferSize);
        }

        std::memcpy(buffer.data() + write_index, &val, sizeof(T));
        write_index += sizeof(T);
        write_index = Common::AlignUp(write_index, 4);
    }

    template <typename T>
    void WriteObject(const T& val) {
        static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable.");

        const u32_le size = static_cast<u32>(sizeof(val));
        Write(size);
        // TODO(Subv): Support file descriptors.
        Write<u32_le>(0); // Fd count.
        Write(val);
    }

    void Deserialize() {
        ASSERT(buffer.size() > sizeof(Header));

        Header header{};
        std::memcpy(&header, buffer.data(), sizeof(Header));

        read_index = header.data_offset;
        DeserializeData();
    }

    std::vector<u8> Serialize() {
        ASSERT(read_index == 0);
        write_index = sizeof(Header);

        SerializeData();

        Header header{};
        header.data_size = static_cast<u32_le>(write_index - sizeof(Header));
        header.data_offset = sizeof(Header);
        header.objects_size = 4;
        header.objects_offset = static_cast<u32>(sizeof(Header) + header.data_size);
        std::memcpy(buffer.data(), &header, sizeof(Header));

        return buffer;
    }

protected:
    virtual void SerializeData() {}

    virtual void DeserializeData() {}

private:
    struct Header {
        u32_le data_size;
        u32_le data_offset;
        u32_le objects_size;
        u32_le objects_offset;
    };
    static_assert(sizeof(Header) == 16, "ParcelHeader has wrong size");

    std::vector<u8> buffer;
    std::size_t read_index = 0;
    std::size_t write_index = 0;
};

class NativeWindow : public Parcel {
public:
    explicit NativeWindow(u32 id) {
        data.id = id;
    }
    ~NativeWindow() override = default;

protected:
    void SerializeData() override {
        Write(data);
    }

private:
    struct Data {
        u32_le magic = 2;
        u32_le process_id = 1;
        u32_le id;
        INSERT_PADDING_WORDS(3);
        std::array<u8, 8> dispdrv = {'d', 'i', 's', 'p', 'd', 'r', 'v', '\0'};
        INSERT_PADDING_WORDS(2);
    };
    static_assert(sizeof(Data) == 0x28, "ParcelData has wrong size");

    Data data{};
};

class IGBPConnectRequestParcel : public Parcel {
public:
    explicit IGBPConnectRequestParcel(std::vector<u8> buffer_) : Parcel(std::move(buffer_)) {
        Deserialize();
    }

    void DeserializeData() override {
        [[maybe_unused]] const std::u16string token = ReadInterfaceToken();
        data = Read<Data>();
    }

    struct Data {
        u32_le unk;
        u32_le api;
        u32_le producer_controlled_by_app;
    };

    Data data;
};

class IGBPConnectResponseParcel : public Parcel {
public:
    explicit IGBPConnectResponseParcel(u32 width, u32 height) {
        data.width = width;
        data.height = height;
    }
    ~IGBPConnectResponseParcel() override = default;

protected:
    void SerializeData() override {
        Write(data);
    }

private:
    struct Data {
        u32_le width;
        u32_le height;
        u32_le transform_hint;
        u32_le num_pending_buffers;
        u32_le status;
    };
    static_assert(sizeof(Data) == 20, "ParcelData has wrong size");

    Data data{};
};

/// Represents a parcel containing one int '0' as its data
/// Used by DetachBuffer and Disconnect
class IGBPEmptyResponseParcel : public Parcel {
protected:
    void SerializeData() override {
        Write(data);
    }

private:
    struct Data {
        u32_le unk_0{};
    };

    Data data{};
};

class IGBPSetPreallocatedBufferRequestParcel : public Parcel {
public:
    explicit IGBPSetPreallocatedBufferRequestParcel(std::vector<u8> buffer_)
        : Parcel(std::move(buffer_)) {
        Deserialize();
    }

    void DeserializeData() override {
        [[maybe_unused]] const std::u16string token = ReadInterfaceToken();
        data = Read<Data>();
        if (data.contains_object != 0) {
            buffer_container = Read<BufferContainer>();
        }
    }

    struct Data {
        u32_le slot;
        u32_le contains_object;
    };

    struct BufferContainer {
        u32_le graphic_buffer_length;
        INSERT_PADDING_WORDS(1);
        NVFlinger::IGBPBuffer buffer{};
    };

    Data data{};
    BufferContainer buffer_container{};
};

class IGBPSetPreallocatedBufferResponseParcel : public Parcel {
protected:
    void SerializeData() override {
        // TODO(Subv): Find out what this means
        Write<u32>(0);
    }
};

class IGBPCancelBufferRequestParcel : public Parcel {
public:
    explicit IGBPCancelBufferRequestParcel(std::vector<u8> buffer_) : Parcel(std::move(buffer_)) {
        Deserialize();
    }

    void DeserializeData() override {
        [[maybe_unused]] const std::u16string token = ReadInterfaceToken();
        data = Read<Data>();
    }

    struct Data {
        u32_le slot;
        Service::Nvidia::MultiFence multi_fence;
    };

    Data data;
};

class IGBPCancelBufferResponseParcel : public Parcel {
protected:
    void SerializeData() override {
        Write<u32>(0); // Success
    }
};

class IGBPDequeueBufferRequestParcel : public Parcel {
public:
    explicit IGBPDequeueBufferRequestParcel(std::vector<u8> buffer_) : Parcel(std::move(buffer_)) {
        Deserialize();
    }

    void DeserializeData() override {
        [[maybe_unused]] const std::u16string token = ReadInterfaceToken();
        data = Read<Data>();
    }

    struct Data {
        u32_le pixel_format;
        u32_le width;
        u32_le height;
        u32_le get_frame_timestamps;
        u32_le usage;
    };

    Data data;
};

class IGBPDequeueBufferResponseParcel : public Parcel {
public:
    explicit IGBPDequeueBufferResponseParcel(u32 slot_, Nvidia::MultiFence& multi_fence_)
        : slot(slot_), multi_fence(multi_fence_) {}

protected:
    void SerializeData() override {
        Write(slot);
        Write<u32_le>(1);
        WriteObject(multi_fence);
        Write<u32_le>(0);
    }

    u32_le slot;
    Service::Nvidia::MultiFence multi_fence;
};

class IGBPRequestBufferRequestParcel : public Parcel {
public:
    explicit IGBPRequestBufferRequestParcel(std::vector<u8> buffer_) : Parcel(std::move(buffer_)) {
        Deserialize();
    }

    void DeserializeData() override {
        [[maybe_unused]] const std::u16string token = ReadInterfaceToken();
        slot = Read<u32_le>();
    }

    u32_le slot;
};

class IGBPRequestBufferResponseParcel : public Parcel {
public:
    explicit IGBPRequestBufferResponseParcel(NVFlinger::IGBPBuffer buffer_) : buffer(buffer_) {}
    ~IGBPRequestBufferResponseParcel() override = default;

protected:
    void SerializeData() override {
        // TODO(Subv): Figure out what this value means, writing non-zero here will make libnx
        // try to read an IGBPBuffer object from the parcel.
        Write<u32_le>(1);
        WriteObject(buffer);
        Write<u32_le>(0);
    }

    NVFlinger::IGBPBuffer buffer;
};

class IGBPQueueBufferRequestParcel : public Parcel {
public:
    explicit IGBPQueueBufferRequestParcel(std::vector<u8> buffer_) : Parcel(std::move(buffer_)) {
        Deserialize();
    }

    void DeserializeData() override {
        [[maybe_unused]] const std::u16string token = ReadInterfaceToken();
        data = Read<Data>();
    }

    struct Data {
        u32_le slot;
        INSERT_PADDING_WORDS(3);
        u32_le timestamp;
        s32_le is_auto_timestamp;
        s32_le crop_top;
        s32_le crop_left;
        s32_le crop_right;
        s32_le crop_bottom;
        s32_le scaling_mode;
        NVFlinger::BufferQueue::BufferTransformFlags transform;
        u32_le sticky_transform;
        INSERT_PADDING_WORDS(1);
        u32_le swap_interval;
        Service::Nvidia::MultiFence multi_fence;

        Common::Rectangle<int> GetCropRect() const {
            return {crop_left, crop_top, crop_right, crop_bottom};
        }
    };
    static_assert(sizeof(Data) == 96, "ParcelData has wrong size");

    Data data;
};

class IGBPQueueBufferResponseParcel : public Parcel {
public:
    explicit IGBPQueueBufferResponseParcel(u32 width, u32 height) {
        data.width = width;
        data.height = height;
    }
    ~IGBPQueueBufferResponseParcel() override = default;

protected:
    void SerializeData() override {
        Write(data);
    }

private:
    struct Data {
        u32_le width;
        u32_le height;
        u32_le transform_hint;
        u32_le num_pending_buffers;
        u32_le status;
    };
    static_assert(sizeof(Data) == 20, "ParcelData has wrong size");

    Data data{};
};

class IGBPQueryRequestParcel : public Parcel {
public:
    explicit IGBPQueryRequestParcel(std::vector<u8> buffer_) : Parcel(std::move(buffer_)) {
        Deserialize();
    }

    void DeserializeData() override {
        [[maybe_unused]] const std::u16string token = ReadInterfaceToken();
        type = Read<u32_le>();
    }

    u32 type;
};

class IGBPQueryResponseParcel : public Parcel {
public:
    explicit IGBPQueryResponseParcel(u32 value_) : value{value_} {}
    ~IGBPQueryResponseParcel() override = default;

protected:
    void SerializeData() override {
        Write(value);
    }

private:
    u32_le value;
};

class IHOSBinderDriver final : public ServiceFramework<IHOSBinderDriver> {
public:
    explicit IHOSBinderDriver(Core::System& system_, NVFlinger::NVFlinger& nv_flinger_)
        : ServiceFramework{system_, "IHOSBinderDriver"}, nv_flinger(nv_flinger_) {
        static const FunctionInfo functions[] = {
            {0, &IHOSBinderDriver::TransactParcel, "TransactParcel"},
            {1, &IHOSBinderDriver::AdjustRefcount, "AdjustRefcount"},
            {2, &IHOSBinderDriver::GetNativeHandle, "GetNativeHandle"},
            {3, &IHOSBinderDriver::TransactParcel, "TransactParcelAuto"},
        };
        RegisterHandlers(functions);
    }

private:
    enum class TransactionId {
        RequestBuffer = 1,
        SetBufferCount = 2,
        DequeueBuffer = 3,
        DetachBuffer = 4,
        DetachNextBuffer = 5,
        AttachBuffer = 6,
        QueueBuffer = 7,
        CancelBuffer = 8,
        Query = 9,
        Connect = 10,
        Disconnect = 11,

        AllocateBuffers = 13,
        SetPreallocatedBuffer = 14
    };

    void TransactParcel(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const u32 id = rp.Pop<u32>();
        const auto transaction = static_cast<TransactionId>(rp.Pop<u32>());
        const u32 flags = rp.Pop<u32>();

        LOG_DEBUG(Service_VI, "called. id=0x{:08X} transaction={:X}, flags=0x{:08X}", id,
                  transaction, flags);

        auto& buffer_queue = *nv_flinger.FindBufferQueue(id);

        switch (transaction) {
        case TransactionId::Connect: {
            IGBPConnectRequestParcel request{ctx.ReadBuffer()};
            IGBPConnectResponseParcel response{
                static_cast<u32>(static_cast<u32>(DisplayResolution::UndockedWidth) *
                                 Settings::values.resolution_factor.GetValue()),
                static_cast<u32>(static_cast<u32>(DisplayResolution::UndockedHeight) *
                                 Settings::values.resolution_factor.GetValue())};

            buffer_queue.Connect();

            ctx.WriteBuffer(response.Serialize());
            break;
        }
        case TransactionId::SetPreallocatedBuffer: {
            IGBPSetPreallocatedBufferRequestParcel request{ctx.ReadBuffer()};

            buffer_queue.SetPreallocatedBuffer(request.data.slot, request.buffer_container.buffer);

            IGBPSetPreallocatedBufferResponseParcel response{};
            ctx.WriteBuffer(response.Serialize());
            break;
        }
        case TransactionId::DequeueBuffer: {
            IGBPDequeueBufferRequestParcel request{ctx.ReadBuffer()};
            const u32 width{request.data.width};
            const u32 height{request.data.height};

            do {
                if (auto result = buffer_queue.DequeueBuffer(width, height); result) {
                    // Buffer is available
                    IGBPDequeueBufferResponseParcel response{result->first, *result->second};
                    ctx.WriteBuffer(response.Serialize());
                    break;
                }
            } while (buffer_queue.IsConnected());

            break;
        }
        case TransactionId::RequestBuffer: {
            IGBPRequestBufferRequestParcel request{ctx.ReadBuffer()};

            auto& buffer = buffer_queue.RequestBuffer(request.slot);
            IGBPRequestBufferResponseParcel response{buffer};
            ctx.WriteBuffer(response.Serialize());

            break;
        }
        case TransactionId::QueueBuffer: {
            IGBPQueueBufferRequestParcel request{ctx.ReadBuffer()};

            buffer_queue.QueueBuffer(request.data.slot, request.data.transform,
                                     request.data.GetCropRect(), request.data.swap_interval,
                                     request.data.multi_fence);

            IGBPQueueBufferResponseParcel response{1280, 720};
            ctx.WriteBuffer(response.Serialize());
            break;
        }
        case TransactionId::Query: {
            IGBPQueryRequestParcel request{ctx.ReadBuffer()};

            const u32 value =
                buffer_queue.Query(static_cast<NVFlinger::BufferQueue::QueryType>(request.type));

            IGBPQueryResponseParcel response{value};
            ctx.WriteBuffer(response.Serialize());
            break;
        }
        case TransactionId::CancelBuffer: {
            IGBPCancelBufferRequestParcel request{ctx.ReadBuffer()};

            buffer_queue.CancelBuffer(request.data.slot, request.data.multi_fence);

            IGBPCancelBufferResponseParcel response{};
            ctx.WriteBuffer(response.Serialize());
            break;
        }
        case TransactionId::Disconnect: {
            LOG_WARNING(Service_VI, "(STUBBED) called, transaction=Disconnect");
            const auto buffer = ctx.ReadBuffer();

            buffer_queue.Disconnect();

            IGBPEmptyResponseParcel response{};
            ctx.WriteBuffer(response.Serialize());
            break;
        }
        case TransactionId::DetachBuffer: {
            const auto buffer = ctx.ReadBuffer();

            IGBPEmptyResponseParcel response{};
            ctx.WriteBuffer(response.Serialize());
            break;
        }
        case TransactionId::SetBufferCount: {
            LOG_WARNING(Service_VI, "(STUBBED) called, transaction=SetBufferCount");
            [[maybe_unused]] const auto buffer = ctx.ReadBuffer();

            IGBPEmptyResponseParcel response{};
            ctx.WriteBuffer(response.Serialize());
            break;
        }
        default:
            ASSERT_MSG(false, "Unimplemented");
        }

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultSuccess);
    }

    void AdjustRefcount(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const u32 id = rp.Pop<u32>();
        const s32 addval = rp.PopRaw<s32>();
        const u32 type = rp.Pop<u32>();

        LOG_WARNING(Service_VI, "(STUBBED) called id={}, addval={:08X}, type={:08X}", id, addval,
                    type);

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultSuccess);
    }

    void GetNativeHandle(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const u32 id = rp.Pop<u32>();
        const u32 unknown = rp.Pop<u32>();

        LOG_WARNING(Service_VI, "(STUBBED) called id={}, unknown={:08X}", id, unknown);

        // TODO(Subv): Find out what this actually is.
        IPC::ResponseBuilder rb{ctx, 2, 1};
        rb.Push(ResultSuccess);
        rb.PushCopyObjects(nv_flinger.FindBufferQueue(id)->GetBufferWaitEvent());
    }

    NVFlinger::NVFlinger& nv_flinger;
};

class ISystemDisplayService final : public ServiceFramework<ISystemDisplayService> {
public:
    explicit ISystemDisplayService(Core::System& system_)
        : ServiceFramework{system_, "ISystemDisplayService"} {
        static const FunctionInfo functions[] = {
            {1200, nullptr, "GetZOrderCountMin"},
            {1202, nullptr, "GetZOrderCountMax"},
            {1203, nullptr, "GetDisplayLogicalResolution"},
            {1204, nullptr, "SetDisplayMagnification"},
            {2201, nullptr, "SetLayerPosition"},
            {2203, nullptr, "SetLayerSize"},
            {2204, nullptr, "GetLayerZ"},
            {2205, &ISystemDisplayService::SetLayerZ, "SetLayerZ"},
            {2207, &ISystemDisplayService::SetLayerVisibility, "SetLayerVisibility"},
            {2209, nullptr, "SetLayerAlpha"},
            {2210, nullptr, "SetLayerPositionAndSize"},
            {2312, nullptr, "CreateStrayLayer"},
            {2400, nullptr, "OpenIndirectLayer"},
            {2401, nullptr, "CloseIndirectLayer"},
            {2402, nullptr, "FlipIndirectLayer"},
            {3000, nullptr, "ListDisplayModes"},
            {3001, nullptr, "ListDisplayRgbRanges"},
            {3002, nullptr, "ListDisplayContentTypes"},
            {3200, &ISystemDisplayService::GetDisplayMode, "GetDisplayMode"},
            {3201, nullptr, "SetDisplayMode"},
            {3202, nullptr, "GetDisplayUnderscan"},
            {3203, nullptr, "SetDisplayUnderscan"},
            {3204, nullptr, "GetDisplayContentType"},
            {3205, nullptr, "SetDisplayContentType"},
            {3206, nullptr, "GetDisplayRgbRange"},
            {3207, nullptr, "SetDisplayRgbRange"},
            {3208, nullptr, "GetDisplayCmuMode"},
            {3209, nullptr, "SetDisplayCmuMode"},
            {3210, nullptr, "GetDisplayContrastRatio"},
            {3211, nullptr, "SetDisplayContrastRatio"},
            {3214, nullptr, "GetDisplayGamma"},
            {3215, nullptr, "SetDisplayGamma"},
            {3216, nullptr, "GetDisplayCmuLuma"},
            {3217, nullptr, "SetDisplayCmuLuma"},
            {3218, nullptr, "SetDisplayCrcMode"},
            {6013, nullptr, "GetLayerPresentationSubmissionTimestamps"},
            {8225, nullptr, "GetSharedBufferMemoryHandleId"},
            {8250, nullptr, "OpenSharedLayer"},
            {8251, nullptr, "CloseSharedLayer"},
            {8252, nullptr, "ConnectSharedLayer"},
            {8253, nullptr, "DisconnectSharedLayer"},
            {8254, nullptr, "AcquireSharedFrameBuffer"},
            {8255, nullptr, "PresentSharedFrameBuffer"},
            {8256, nullptr, "GetSharedFrameBufferAcquirableEvent"},
            {8257, nullptr, "FillSharedFrameBufferColor"},
            {8258, nullptr, "CancelSharedFrameBuffer"},
            {9000, nullptr, "GetDp2hdmiController"},
        };
        RegisterHandlers(functions);
    }

private:
    void SetLayerZ(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const u64 layer_id = rp.Pop<u64>();
        const u64 z_value = rp.Pop<u64>();

        LOG_WARNING(Service_VI, "(STUBBED) called. layer_id=0x{:016X}, z_value=0x{:016X}", layer_id,
                    z_value);

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultSuccess);
    }

    // This function currently does nothing but return a success error code in
    // the vi library itself, so do the same thing, but log out the passed in values.
    void SetLayerVisibility(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const u64 layer_id = rp.Pop<u64>();
        const bool visibility = rp.Pop<bool>();

        LOG_DEBUG(Service_VI, "called, layer_id=0x{:08X}, visibility={}", layer_id, visibility);

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultSuccess);
    }

    void GetDisplayMode(Kernel::HLERequestContext& ctx) {
        LOG_WARNING(Service_VI, "(STUBBED) called");

        IPC::ResponseBuilder rb{ctx, 6};
        rb.Push(ResultSuccess);

        if (Settings::values.use_docked_mode.GetValue()) {
            rb.Push(static_cast<u32>(Service::VI::DisplayResolution::DockedWidth) *
                    static_cast<u32>(Settings::values.resolution_factor.GetValue()));
            rb.Push(static_cast<u32>(Service::VI::DisplayResolution::DockedHeight) *
                    static_cast<u32>(Settings::values.resolution_factor.GetValue()));
        } else {
            rb.Push(static_cast<u32>(Service::VI::DisplayResolution::UndockedWidth) *
                    static_cast<u32>(Settings::values.resolution_factor.GetValue()));
            rb.Push(static_cast<u32>(Service::VI::DisplayResolution::UndockedHeight) *
                    static_cast<u32>(Settings::values.resolution_factor.GetValue()));
        }

        rb.PushRaw<float>(60.0f); // This wouldn't seem to be correct for 30 fps games.
        rb.Push<u32>(0);
    }
};

class IManagerDisplayService final : public ServiceFramework<IManagerDisplayService> {
public:
    explicit IManagerDisplayService(Core::System& system_, NVFlinger::NVFlinger& nv_flinger_)
        : ServiceFramework{system_, "IManagerDisplayService"}, nv_flinger{nv_flinger_} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {200, nullptr, "AllocateProcessHeapBlock"},
            {201, nullptr, "FreeProcessHeapBlock"},
            {1020, &IManagerDisplayService::CloseDisplay, "CloseDisplay"},
            {1102, nullptr, "GetDisplayResolution"},
            {2010, &IManagerDisplayService::CreateManagedLayer, "CreateManagedLayer"},
            {2011, nullptr, "DestroyManagedLayer"},
            {2012, nullptr, "CreateStrayLayer"},
            {2050, nullptr, "CreateIndirectLayer"},
            {2051, nullptr, "DestroyIndirectLayer"},
            {2052, nullptr, "CreateIndirectProducerEndPoint"},
            {2053, nullptr, "DestroyIndirectProducerEndPoint"},
            {2054, nullptr, "CreateIndirectConsumerEndPoint"},
            {2055, nullptr, "DestroyIndirectConsumerEndPoint"},
            {2300, nullptr, "AcquireLayerTexturePresentingEvent"},
            {2301, nullptr, "ReleaseLayerTexturePresentingEvent"},
            {2302, nullptr, "GetDisplayHotplugEvent"},
            {2303, nullptr, "GetDisplayModeChangedEvent"},
            {2402, nullptr, "GetDisplayHotplugState"},
            {2501, nullptr, "GetCompositorErrorInfo"},
            {2601, nullptr, "GetDisplayErrorEvent"},
            {2701, nullptr, "GetDisplayFatalErrorEvent"},
            {4201, nullptr, "SetDisplayAlpha"},
            {4203, nullptr, "SetDisplayLayerStack"},
            {4205, nullptr, "SetDisplayPowerState"},
            {4206, nullptr, "SetDefaultDisplay"},
            {4207, nullptr, "ResetDisplayPanel"},
            {4208, nullptr, "SetDisplayFatalErrorEnabled"},
            {4209, nullptr, "IsDisplayPanelOn"},
            {4300, nullptr, "GetInternalPanelId"},
            {6000, &IManagerDisplayService::AddToLayerStack, "AddToLayerStack"},
            {6001, nullptr, "RemoveFromLayerStack"},
            {6002, &IManagerDisplayService::SetLayerVisibility, "SetLayerVisibility"},
            {6003, nullptr, "SetLayerConfig"},
            {6004, nullptr, "AttachLayerPresentationTracer"},
            {6005, nullptr, "DetachLayerPresentationTracer"},
            {6006, nullptr, "StartLayerPresentationRecording"},
            {6007, nullptr, "StopLayerPresentationRecording"},
            {6008, nullptr, "StartLayerPresentationFenceWait"},
            {6009, nullptr, "StopLayerPresentationFenceWait"},
            {6010, nullptr, "GetLayerPresentationAllFencesExpiredEvent"},
            {6011, nullptr, "EnableLayerAutoClearTransitionBuffer"},
            {6012, nullptr, "DisableLayerAutoClearTransitionBuffer"},
            {7000, nullptr, "SetContentVisibility"},
            {8000, nullptr, "SetConductorLayer"},
            {8001, nullptr, "SetTimestampTracking"},
            {8100, nullptr, "SetIndirectProducerFlipOffset"},
            {8200, nullptr, "CreateSharedBufferStaticStorage"},
            {8201, nullptr, "CreateSharedBufferTransferMemory"},
            {8202, nullptr, "DestroySharedBuffer"},
            {8203, nullptr, "BindSharedLowLevelLayerToManagedLayer"},
            {8204, nullptr, "BindSharedLowLevelLayerToIndirectLayer"},
            {8207, nullptr, "UnbindSharedLowLevelLayer"},
            {8208, nullptr, "ConnectSharedLowLevelLayerToSharedBuffer"},
            {8209, nullptr, "DisconnectSharedLowLevelLayerFromSharedBuffer"},
            {8210, nullptr, "CreateSharedLayer"},
            {8211, nullptr, "DestroySharedLayer"},
            {8216, nullptr, "AttachSharedLayerToLowLevelLayer"},
            {8217, nullptr, "ForceDetachSharedLayerFromLowLevelLayer"},
            {8218, nullptr, "StartDetachSharedLayerFromLowLevelLayer"},
            {8219, nullptr, "FinishDetachSharedLayerFromLowLevelLayer"},
            {8220, nullptr, "GetSharedLayerDetachReadyEvent"},
            {8221, nullptr, "GetSharedLowLevelLayerSynchronizedEvent"},
            {8222, nullptr, "CheckSharedLowLevelLayerSynchronized"},
            {8223, nullptr, "RegisterSharedBufferImporterAruid"},
            {8224, nullptr, "UnregisterSharedBufferImporterAruid"},
            {8227, nullptr, "CreateSharedBufferProcessHeap"},
            {8228, nullptr, "GetSharedLayerLayerStacks"},
            {8229, nullptr, "SetSharedLayerLayerStacks"},
            {8291, nullptr, "PresentDetachedSharedFrameBufferToLowLevelLayer"},
            {8292, nullptr, "FillDetachedSharedFrameBufferColor"},
            {8293, nullptr, "GetDetachedSharedFrameBufferImage"},
            {8294, nullptr, "SetDetachedSharedFrameBufferImage"},
            {8295, nullptr, "CopyDetachedSharedFrameBufferImage"},
            {8296, nullptr, "SetDetachedSharedFrameBufferSubImage"},
            {8297, nullptr, "GetSharedFrameBufferContentParameter"},
            {8298, nullptr, "ExpandStartupLogoOnSharedFrameBuffer"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }

private:
    void CloseDisplay(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const u64 display = rp.Pop<u64>();

        LOG_WARNING(Service_VI, "(STUBBED) called. display=0x{:016X}", display);

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultSuccess);
    }

    void CreateManagedLayer(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const u32 unknown = rp.Pop<u32>();
        rp.Skip(1, false);
        const u64 display = rp.Pop<u64>();
        const u64 aruid = rp.Pop<u64>();

        LOG_WARNING(Service_VI,
                    "(STUBBED) called. unknown=0x{:08X}, display=0x{:016X}, aruid=0x{:016X}",
                    unknown, display, aruid);

        const auto layer_id = nv_flinger.CreateLayer(display);
        if (!layer_id) {
            LOG_ERROR(Service_VI, "Layer not found! display=0x{:016X}", display);
            IPC::ResponseBuilder rb{ctx, 2};
            rb.Push(ERR_NOT_FOUND);
            return;
        }

        IPC::ResponseBuilder rb{ctx, 4};
        rb.Push(ResultSuccess);
        rb.Push(*layer_id);
    }

    void AddToLayerStack(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const u32 stack = rp.Pop<u32>();
        const u64 layer_id = rp.Pop<u64>();

        LOG_WARNING(Service_VI, "(STUBBED) called. stack=0x{:08X}, layer_id=0x{:016X}", stack,
                    layer_id);

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultSuccess);
    }

    void SetLayerVisibility(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const u64 layer_id = rp.Pop<u64>();
        const bool visibility = rp.Pop<bool>();

        LOG_WARNING(Service_VI, "(STUBBED) called, layer_id=0x{:X}, visibility={}", layer_id,
                    visibility);

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultSuccess);
    }

    NVFlinger::NVFlinger& nv_flinger;
};

class IApplicationDisplayService final : public ServiceFramework<IApplicationDisplayService> {
public:
    explicit IApplicationDisplayService(Core::System& system_, NVFlinger::NVFlinger& nv_flinger_);

private:
    enum class ConvertedScaleMode : u64 {
        Freeze = 0,
        ScaleToWindow = 1,
        ScaleAndCrop = 2,
        None = 3,
        PreserveAspectRatio = 4,
    };

    enum class NintendoScaleMode : u32 {
        None = 0,
        Freeze = 1,
        ScaleToWindow = 2,
        ScaleAndCrop = 3,
        PreserveAspectRatio = 4,
    };

    void GetRelayService(Kernel::HLERequestContext& ctx) {
        LOG_WARNING(Service_VI, "(STUBBED) called");

        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(ResultSuccess);
        rb.PushIpcInterface<IHOSBinderDriver>(system, nv_flinger);
    }

    void GetSystemDisplayService(Kernel::HLERequestContext& ctx) {
        LOG_WARNING(Service_VI, "(STUBBED) called");

        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(ResultSuccess);
        rb.PushIpcInterface<ISystemDisplayService>(system);
    }

    void GetManagerDisplayService(Kernel::HLERequestContext& ctx) {
        LOG_WARNING(Service_VI, "(STUBBED) called");

        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(ResultSuccess);
        rb.PushIpcInterface<IManagerDisplayService>(system, nv_flinger);
    }

    void GetIndirectDisplayTransactionService(Kernel::HLERequestContext& ctx) {
        LOG_WARNING(Service_VI, "(STUBBED) called");

        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(ResultSuccess);
        rb.PushIpcInterface<IHOSBinderDriver>(system, nv_flinger);
    }

    void OpenDisplay(Kernel::HLERequestContext& ctx) {
        LOG_WARNING(Service_VI, "(STUBBED) called");

        IPC::RequestParser rp{ctx};
        const auto name_buf = rp.PopRaw<std::array<char, 0x40>>();

        OpenDisplayImpl(ctx, std::string_view{name_buf.data(), name_buf.size()});
    }

    void OpenDefaultDisplay(Kernel::HLERequestContext& ctx) {
        LOG_DEBUG(Service_VI, "called");

        OpenDisplayImpl(ctx, "Default");
    }

    void OpenDisplayImpl(Kernel::HLERequestContext& ctx, std::string_view name) {
        const auto trim_pos = name.find('\0');

        if (trim_pos != std::string_view::npos) {
            name.remove_suffix(name.size() - trim_pos);
        }

        ASSERT_MSG(name == "Default", "Non-default displays aren't supported yet");

        const auto display_id = nv_flinger.OpenDisplay(name);
        if (!display_id) {
            LOG_ERROR(Service_VI, "Display not found! display_name={}", name);
            IPC::ResponseBuilder rb{ctx, 2};
            rb.Push(ERR_NOT_FOUND);
            return;
        }

        IPC::ResponseBuilder rb{ctx, 4};
        rb.Push(ResultSuccess);
        rb.Push<u64>(*display_id);
    }

    void CloseDisplay(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const u64 display_id = rp.Pop<u64>();

        LOG_WARNING(Service_VI, "(STUBBED) called. display_id=0x{:016X}", display_id);

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultSuccess);
    }

    // This literally does nothing internally in the actual service itself,
    // and just returns a successful result code regardless of the input.
    void SetDisplayEnabled(Kernel::HLERequestContext& ctx) {
        LOG_DEBUG(Service_VI, "called.");

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultSuccess);
    }

    void GetDisplayResolution(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const u64 display_id = rp.Pop<u64>();

        LOG_DEBUG(Service_VI, "called. display_id=0x{:016X}", display_id);

        IPC::ResponseBuilder rb{ctx, 6};
        rb.Push(ResultSuccess);

        // This only returns the fixed values of 1280x720 and makes no distinguishing
        // between docked and undocked dimensions. We take the liberty of applying
        // the resolution scaling factor here.
        rb.Push(static_cast<u64>(DisplayResolution::UndockedWidth) *
                static_cast<u32>(Settings::values.resolution_factor.GetValue()));
        rb.Push(static_cast<u64>(DisplayResolution::UndockedHeight) *
                static_cast<u32>(Settings::values.resolution_factor.GetValue()));
    }

    void SetLayerScalingMode(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto scaling_mode = rp.PopEnum<NintendoScaleMode>();
        const u64 unknown = rp.Pop<u64>();

        LOG_DEBUG(Service_VI, "called. scaling_mode=0x{:08X}, unknown=0x{:016X}", scaling_mode,
                  unknown);

        IPC::ResponseBuilder rb{ctx, 2};

        if (scaling_mode > NintendoScaleMode::PreserveAspectRatio) {
            LOG_ERROR(Service_VI, "Invalid scaling mode provided.");
            rb.Push(ERR_OPERATION_FAILED);
            return;
        }

        if (scaling_mode != NintendoScaleMode::ScaleToWindow &&
            scaling_mode != NintendoScaleMode::PreserveAspectRatio) {
            LOG_ERROR(Service_VI, "Unsupported scaling mode supplied.");
            rb.Push(ERR_UNSUPPORTED);
            return;
        }

        rb.Push(ResultSuccess);
    }

    void ListDisplays(Kernel::HLERequestContext& ctx) {
        LOG_WARNING(Service_VI, "(STUBBED) called");

        DisplayInfo display_info;
        display_info.width *= static_cast<u64>(Settings::values.resolution_factor.GetValue());
        display_info.height *= static_cast<u64>(Settings::values.resolution_factor.GetValue());
        ctx.WriteBuffer(&display_info, sizeof(DisplayInfo));
        IPC::ResponseBuilder rb{ctx, 4};
        rb.Push(ResultSuccess);
        rb.Push<u64>(1);
    }

    void OpenLayer(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto name_buf = rp.PopRaw<std::array<u8, 0x40>>();
        const auto end = std::find(name_buf.begin(), name_buf.end(), '\0');

        const std::string display_name(name_buf.begin(), end);

        const u64 layer_id = rp.Pop<u64>();
        const u64 aruid = rp.Pop<u64>();

        LOG_DEBUG(Service_VI, "called. layer_id=0x{:016X}, aruid=0x{:016X}", layer_id, aruid);

        const auto display_id = nv_flinger.OpenDisplay(display_name);
        if (!display_id) {
            LOG_ERROR(Service_VI, "Layer not found! layer_id={}", layer_id);
            IPC::ResponseBuilder rb{ctx, 2};
            rb.Push(ERR_NOT_FOUND);
            return;
        }

        const auto buffer_queue_id = nv_flinger.FindBufferQueueId(*display_id, layer_id);
        if (!buffer_queue_id) {
            LOG_ERROR(Service_VI, "Buffer queue id not found! display_id={}", *display_id);
            IPC::ResponseBuilder rb{ctx, 2};
            rb.Push(ERR_NOT_FOUND);
            return;
        }

        NativeWindow native_window{*buffer_queue_id};
        const auto buffer_size = ctx.WriteBuffer(native_window.Serialize());

        IPC::ResponseBuilder rb{ctx, 4};
        rb.Push(ResultSuccess);
        rb.Push<u64>(buffer_size);
    }

    void CloseLayer(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto layer_id{rp.Pop<u64>()};

        LOG_DEBUG(Service_VI, "called. layer_id=0x{:016X}", layer_id);

        nv_flinger.CloseLayer(layer_id);

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultSuccess);
    }

    void CreateStrayLayer(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const u32 flags = rp.Pop<u32>();
        rp.Pop<u32>(); // padding
        const u64 display_id = rp.Pop<u64>();

        LOG_DEBUG(Service_VI, "called. flags=0x{:08X}, display_id=0x{:016X}", flags, display_id);

        // TODO(Subv): What's the difference between a Stray and a Managed layer?

        const auto layer_id = nv_flinger.CreateLayer(display_id);
        if (!layer_id) {
            LOG_ERROR(Service_VI, "Layer not found! layer_id={}", *layer_id);
            IPC::ResponseBuilder rb{ctx, 2};
            rb.Push(ERR_NOT_FOUND);
            return;
        }

        const auto buffer_queue_id = nv_flinger.FindBufferQueueId(display_id, *layer_id);
        if (!buffer_queue_id) {
            LOG_ERROR(Service_VI, "Buffer queue id not found! display_id={}", display_id);
            IPC::ResponseBuilder rb{ctx, 2};
            rb.Push(ERR_NOT_FOUND);
            return;
        }

        NativeWindow native_window{*buffer_queue_id};
        const auto buffer_size = ctx.WriteBuffer(native_window.Serialize());

        IPC::ResponseBuilder rb{ctx, 6};
        rb.Push(ResultSuccess);
        rb.Push(*layer_id);
        rb.Push<u64>(buffer_size);
    }

    void DestroyStrayLayer(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const u64 layer_id = rp.Pop<u64>();

        LOG_WARNING(Service_VI, "(STUBBED) called. layer_id=0x{:016X}", layer_id);

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultSuccess);
    }

    void GetDisplayVsyncEvent(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const u64 display_id = rp.Pop<u64>();

        LOG_WARNING(Service_VI, "(STUBBED) called. display_id=0x{:016X}", display_id);

        const auto vsync_event = nv_flinger.FindVsyncEvent(display_id);
        if (!vsync_event) {
            LOG_ERROR(Service_VI, "Vsync event was not found for display_id={}", display_id);
            IPC::ResponseBuilder rb{ctx, 2};
            rb.Push(ERR_NOT_FOUND);
            return;
        }

        IPC::ResponseBuilder rb{ctx, 2, 1};
        rb.Push(ResultSuccess);
        rb.PushCopyObjects(vsync_event);
    }

    void ConvertScalingMode(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto mode = rp.PopEnum<NintendoScaleMode>();
        LOG_DEBUG(Service_VI, "called mode={}", mode);

        const auto converted_mode = ConvertScalingModeImpl(mode);

        if (converted_mode.Succeeded()) {
            IPC::ResponseBuilder rb{ctx, 4};
            rb.Push(ResultSuccess);
            rb.PushEnum(*converted_mode);
        } else {
            IPC::ResponseBuilder rb{ctx, 2};
            rb.Push(converted_mode.Code());
        }
    }

    void GetIndirectLayerImageMap(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto width = rp.Pop<s64>();
        const auto height = rp.Pop<s64>();
        const auto indirect_layer_consumer_handle = rp.Pop<u64>();
        const auto applet_resource_user_id = rp.Pop<u64>();

        LOG_WARNING(Service_VI,
                    "(STUBBED) called, width={}, height={}, indirect_layer_consumer_handle={}, "
                    "applet_resource_user_id={}",
                    width, height, indirect_layer_consumer_handle, applet_resource_user_id);

        std::vector<u8> out_buffer(0x46);
        ctx.WriteBuffer(out_buffer);

        // TODO: Figure out what these are

        constexpr s64 unknown_result_1 = 0;
        constexpr s64 unknown_result_2 = 0;

        IPC::ResponseBuilder rb{ctx, 6};
        rb.Push(unknown_result_1);
        rb.Push(unknown_result_2);
        rb.Push(ResultSuccess);
    }

    void GetIndirectLayerImageRequiredMemoryInfo(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto width = rp.Pop<u64>();
        const auto height = rp.Pop<u64>();
        LOG_DEBUG(Service_VI, "called width={}, height={}", width, height);

        constexpr u64 base_size = 0x20000;
        constexpr u64 alignment = 0x1000;
        const auto texture_size = width * height * 4;
        const auto out_size = (texture_size + base_size - 1) / base_size * base_size;

        IPC::ResponseBuilder rb{ctx, 6};
        rb.Push(ResultSuccess);
        rb.Push(out_size);
        rb.Push(alignment);
    }

    static ResultVal<ConvertedScaleMode> ConvertScalingModeImpl(NintendoScaleMode mode) {
        switch (mode) {
        case NintendoScaleMode::None:
            return MakeResult(ConvertedScaleMode::None);
        case NintendoScaleMode::Freeze:
            return MakeResult(ConvertedScaleMode::Freeze);
        case NintendoScaleMode::ScaleToWindow:
            return MakeResult(ConvertedScaleMode::ScaleToWindow);
        case NintendoScaleMode::ScaleAndCrop:
            return MakeResult(ConvertedScaleMode::ScaleAndCrop);
        case NintendoScaleMode::PreserveAspectRatio:
            return MakeResult(ConvertedScaleMode::PreserveAspectRatio);
        default:
            LOG_ERROR(Service_VI, "Invalid scaling mode specified, mode={}", mode);
            return ERR_OPERATION_FAILED;
        }
    }

    NVFlinger::NVFlinger& nv_flinger;
};

IApplicationDisplayService::IApplicationDisplayService(Core::System& system_,
                                                       NVFlinger::NVFlinger& nv_flinger_)
    : ServiceFramework{system_, "IApplicationDisplayService"}, nv_flinger{nv_flinger_} {
    static const FunctionInfo functions[] = {
        {100, &IApplicationDisplayService::GetRelayService, "GetRelayService"},
        {101, &IApplicationDisplayService::GetSystemDisplayService, "GetSystemDisplayService"},
        {102, &IApplicationDisplayService::GetManagerDisplayService, "GetManagerDisplayService"},
        {103, &IApplicationDisplayService::GetIndirectDisplayTransactionService,
         "GetIndirectDisplayTransactionService"},
        {1000, &IApplicationDisplayService::ListDisplays, "ListDisplays"},
        {1010, &IApplicationDisplayService::OpenDisplay, "OpenDisplay"},
        {1011, &IApplicationDisplayService::OpenDefaultDisplay, "OpenDefaultDisplay"},
        {1020, &IApplicationDisplayService::CloseDisplay, "CloseDisplay"},
        {1101, &IApplicationDisplayService::SetDisplayEnabled, "SetDisplayEnabled"},
        {1102, &IApplicationDisplayService::GetDisplayResolution, "GetDisplayResolution"},
        {2020, &IApplicationDisplayService::OpenLayer, "OpenLayer"},
        {2021, &IApplicationDisplayService::CloseLayer, "CloseLayer"},
        {2030, &IApplicationDisplayService::CreateStrayLayer, "CreateStrayLayer"},
        {2031, &IApplicationDisplayService::DestroyStrayLayer, "DestroyStrayLayer"},
        {2101, &IApplicationDisplayService::SetLayerScalingMode, "SetLayerScalingMode"},
        {2102, &IApplicationDisplayService::ConvertScalingMode, "ConvertScalingMode"},
        {2450, &IApplicationDisplayService::GetIndirectLayerImageMap, "GetIndirectLayerImageMap"},
        {2451, nullptr, "GetIndirectLayerImageCropMap"},
        {2460, &IApplicationDisplayService::GetIndirectLayerImageRequiredMemoryInfo,
         "GetIndirectLayerImageRequiredMemoryInfo"},
        {5202, &IApplicationDisplayService::GetDisplayVsyncEvent, "GetDisplayVsyncEvent"},
        {5203, nullptr, "GetDisplayVsyncEventForDebug"},
    };
    RegisterHandlers(functions);
}

static bool IsValidServiceAccess(Permission permission, Policy policy) {
    if (permission == Permission::User) {
        return policy == Policy::User;
    }

    if (permission == Permission::System || permission == Permission::Manager) {
        return policy == Policy::User || policy == Policy::Compositor;
    }

    return false;
}

void detail::GetDisplayServiceImpl(Kernel::HLERequestContext& ctx, Core::System& system,
                                   NVFlinger::NVFlinger& nv_flinger, Permission permission) {
    IPC::RequestParser rp{ctx};
    const auto policy = rp.PopEnum<Policy>();

    if (!IsValidServiceAccess(permission, policy)) {
        LOG_ERROR(Service_VI, "Permission denied for policy {}", policy);
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ERR_PERMISSION_DENIED);
        return;
    }

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<IApplicationDisplayService>(system, nv_flinger);
}

void InstallInterfaces(SM::ServiceManager& service_manager, Core::System& system,
                       NVFlinger::NVFlinger& nv_flinger) {
    std::make_shared<VI_M>(system, nv_flinger)->InstallAsService(service_manager);
    std::make_shared<VI_S>(system, nv_flinger)->InstallAsService(service_manager);
    std::make_shared<VI_U>(system, nv_flinger)->InstallAsService(service_manager);
}

} // namespace Service::VI
