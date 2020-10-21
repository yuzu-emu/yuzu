// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <new>
#include <utility>
#include "common/assert.h"
#include "common/bit_field.h"
#include "common/common_types.h"

// All the constants in this file come from http://switchbrew.org/index.php?title=Error_codes

/**
 * Identifies the module which caused the error. Error codes can be propagated through a call
 * chain, meaning that this doesn't always correspond to the module where the API call made is
 * contained.
 */
enum class ErrorModule : u32 {
    Common = 0,
    Kernel = 1,
    FS = 2,
    OS = 3, // used for Memory, Thread, Mutex, Nvidia
    HTCS = 4,
    NCM = 5,
    DD = 6,
    LR = 8,
    Loader = 9,
    CMIF = 10,
    HIPC = 11,
    PM = 15,
    NS = 16,
    HTC = 18,
    NCMContent = 20,
    SM = 21,
    RO = 22,
    SDMMC = 24,
    OVLN = 25,
    SPL = 26,
    ETHC = 100,
    I2C = 101,
    GPIO = 102,
    UART = 103,
    Settings = 105,
    WLAN = 107,
    XCD = 108,
    NIFM = 110,
    Hwopus = 111,
    Bluetooth = 113,
    VI = 114,
    NFP = 115,
    Time = 116,
    FGM = 117,
    OE = 118,
    PCIe = 120,
    Friends = 121,
    BCAT = 122,
    SSL = 123,
    Account = 124,
    News = 125,
    Mii = 126,
    NFC = 127,
    AM = 128,
    PlayReport = 129,
    AHID = 130,
    Qlaunch = 132,
    PCV = 133,
    OMM = 134,
    BPC = 135,
    PSM = 136,
    NIM = 137,
    PSC = 138,
    TC = 139,
    USB = 140,
    NSD = 141,
    PCTL = 142,
    BTM = 143,
    ETicket = 145,
    NGC = 146,
    ERPT = 147,
    APM = 148,
    Profiler = 150,
    ErrorUpload = 151,
    Audio = 153,
    NPNS = 154,
    NPNSHTTPSTREAM = 155,
    ARP = 157,
    SWKBD = 158,
    BOOT = 159,
    NFCMifare = 161,
    UserlandAssert = 162,
    Fatal = 163,
    NIMShop = 164,
    SPSM = 165,
    BGTC = 167,
    UserlandCrash = 168,
    SREPO = 180,
    Dauth = 181,
    HID = 202,
    LDN = 203,
    Irsensor = 205,
    Capture = 206,
    Manu = 208,
    ATK = 209,
    GRC = 212,
    Migration = 216,
    MigrationLdcServ = 217,
    GeneralWebApplet = 800,
    WifiWebAuthApplet = 809,
    WhitelistedApplet = 810,
    ShopN = 811,
};

/// Encapsulates a Horizon OS error code, allowing it to be separated into its constituent fields.
union ResultCode {
    u32 raw;

    BitField<0, 9, ErrorModule> error_module;
    BitField<9, 13, u32> description;

    constexpr explicit ResultCode(u32 raw) : raw(raw) {}

    constexpr ResultCode(ErrorModule module_, u32 description_)
        : raw(error_module.FormatValue(module_) | description.FormatValue(description_)) {}

    constexpr bool IsSuccess() const {
        return raw == 0;
    }

    constexpr bool IsError() const {
        return raw != 0;
    }
};

constexpr bool operator==(const ResultCode& a, const ResultCode& b) {
    return a.raw == b.raw;
}

constexpr bool operator!=(const ResultCode& a, const ResultCode& b) {
    return a.raw != b.raw;
}

// Convenience functions for creating some common kinds of errors:

/// The default success `ResultCode`.
constexpr ResultCode RESULT_SUCCESS(0);

/**
 * Placeholder result code used for unknown error codes.
 *
 * @note This should only be used when a particular error code
 *       is not known yet.
 */
constexpr ResultCode RESULT_UNKNOWN(UINT32_MAX);

/**
 * This is an optional value type. It holds a `ResultCode` and, if that code is a success code,
 * also holds a result of type `T`. If the code is an error code then trying to access the inner
 * value fails, thus ensuring that the ResultCode of functions is always checked properly before
 * their return value is used.  It is similar in concept to the `std::optional` type
 * (http://en.cppreference.com/w/cpp/experimental/optional) originally proposed for inclusion in
 * C++14, or the `Result` type in Rust (http://doc.rust-lang.org/std/result/index.html).
 *
 * An example of how it could be used:
 * \code
 * ResultVal<int> Frobnicate(float strength) {
 *     if (strength < 0.f || strength > 1.0f) {
 *         // Can't frobnicate too weakly or too strongly
 *         return ResultCode(ErrorDescription::OutOfRange, ErrorModule::Common,
 *             ErrorSummary::InvalidArgument, ErrorLevel::Permanent);
 *     } else {
 *         // Frobnicated! Give caller a cookie
 *         return MakeResult<int>(42);
 *     }
 * }
 * \endcode
 *
 * \code
 * ResultVal<int> frob_result = Frobnicate(0.75f);
 * if (frob_result) {
 *     // Frobbed ok
 *     printf("My cookie is %d\n", *frob_result);
 * } else {
 *     printf("Guess I overdid it. :( Error code: %ux\n", frob_result.code().hex);
 * }
 * \endcode
 */
template <typename T>
class ResultVal {
public:
    /// Constructs an empty `ResultVal` with the given error code. The code must not be a success
    /// code.
    ResultVal(ResultCode error_code = RESULT_UNKNOWN) : result_code(error_code) {
        ASSERT(error_code.IsError());
    }

    /**
     * Similar to the non-member function `MakeResult`, with the exception that you can manually
     * specify the success code. `success_code` must not be an error code.
     */
    template <typename... Args>
    static ResultVal WithCode(ResultCode success_code, Args&&... args) {
        ResultVal<T> result;
        result.emplace(success_code, std::forward<Args>(args)...);
        return result;
    }

    ResultVal(const ResultVal& o) : result_code(o.result_code) {
        if (!o.empty()) {
            new (&object) T(o.object);
        }
    }

    ResultVal(ResultVal&& o) noexcept : result_code(o.result_code) {
        if (!o.empty()) {
            new (&object) T(std::move(o.object));
        }
    }

    ~ResultVal() {
        if (!empty()) {
            object.~T();
        }
    }

    ResultVal& operator=(const ResultVal& o) {
        if (this == &o) {
            return *this;
        }
        if (!empty()) {
            if (!o.empty()) {
                object = o.object;
            } else {
                object.~T();
            }
        } else {
            if (!o.empty()) {
                new (&object) T(o.object);
            }
        }
        result_code = o.result_code;

        return *this;
    }

    /**
     * Replaces the current result with a new constructed result value in-place. The code must not
     * be an error code.
     */
    template <typename... Args>
    void emplace(ResultCode success_code, Args&&... args) {
        ASSERT(success_code.IsSuccess());
        if (!empty()) {
            object.~T();
        }
        new (&object) T(std::forward<Args>(args)...);
        result_code = success_code;
    }

    /// Returns true if the `ResultVal` contains an error code and no value.
    bool empty() const {
        return result_code.IsError();
    }

    /// Returns true if the `ResultVal` contains a return value.
    bool Succeeded() const {
        return result_code.IsSuccess();
    }
    /// Returns true if the `ResultVal` contains an error code and no value.
    bool Failed() const {
        return empty();
    }

    ResultCode Code() const {
        return result_code;
    }

    const T& operator*() const {
        return object;
    }
    T& operator*() {
        return object;
    }
    const T* operator->() const {
        return &object;
    }
    T* operator->() {
        return &object;
    }

    /// Returns the value contained in this `ResultVal`, or the supplied default if it is missing.
    template <typename U>
    T ValueOr(U&& value) const {
        return !empty() ? object : std::move(value);
    }

    /// Asserts that the result succeeded and returns a reference to it.
    T& Unwrap() & {
        ASSERT_MSG(Succeeded(), "Tried to Unwrap empty ResultVal");
        return **this;
    }

    T&& Unwrap() && {
        ASSERT_MSG(Succeeded(), "Tried to Unwrap empty ResultVal");
        return std::move(**this);
    }

private:
    // A union is used to allocate the storage for the value, while allowing us to construct and
    // destruct it at will.
    union {
        T object;
    };
    ResultCode result_code;
};

/**
 * This function is a helper used to construct `ResultVal`s. It receives the arguments to construct
 * `T` with and creates a success `ResultVal` contained the constructed value.
 */
template <typename T, typename... Args>
ResultVal<T> MakeResult(Args&&... args) {
    return ResultVal<T>::WithCode(RESULT_SUCCESS, std::forward<Args>(args)...);
}

/**
 * Deducible overload of MakeResult, allowing the template parameter to be ommited if you're just
 * copy or move constructing.
 */
template <typename Arg>
ResultVal<std::remove_reference_t<Arg>> MakeResult(Arg&& arg) {
    return ResultVal<std::remove_reference_t<Arg>>::WithCode(RESULT_SUCCESS,
                                                             std::forward<Arg>(arg));
}

/**
 * Check for the success of `source` (which must evaluate to a ResultVal). If it succeeds, unwraps
 * the contained value and assigns it to `target`, which can be either an l-value expression or a
 * variable declaration. If it fails the return code is returned from the current function. Thus it
 * can be used to cascade errors out, achieving something akin to exception handling.
 */
#define CASCADE_RESULT(target, source)                                                             \
    auto CONCAT2(check_result_L, __LINE__) = source;                                               \
    if (CONCAT2(check_result_L, __LINE__).Failed()) {                                              \
        return CONCAT2(check_result_L, __LINE__).Code();                                           \
    }                                                                                              \
    target = std::move(*CONCAT2(check_result_L, __LINE__))

/**
 * Analogous to CASCADE_RESULT, but for a bare ResultCode. The code will be propagated if
 * non-success, or discarded otherwise.
 */
#define CASCADE_CODE(source)                                                                       \
    do {                                                                                           \
        auto CONCAT2(check_result_L, __LINE__) = source;                                           \
        if (CONCAT2(check_result_L, __LINE__).IsError()) {                                         \
            return CONCAT2(check_result_L, __LINE__);                                              \
        }                                                                                          \
    } while (false)
