// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <atomic>
#include <string>

#include <boost/intrusive/rbtree.hpp>

#include "common/assert.h"
#include "common/common_funcs.h"
#include "common/common_types.h"
#include "core/hle/kernel/k_class_token.h"

namespace Kernel {

class KernelCore;
class KProcess;

#define KERNEL_AUTOOBJECT_TRAITS(CLASS, BASE_CLASS)                                                \
    YUZU_NON_COPYABLE(CLASS);                                                                      \
    YUZU_NON_MOVEABLE(CLASS);                                                                      \
                                                                                                   \
private:                                                                                           \
    friend class ::Kernel::KClassTokenGenerator;                                                   \
    static constexpr inline auto ObjectType = ::Kernel::KClassTokenGenerator::ObjectType::CLASS;   \
    static constexpr inline const char* const TypeName = #CLASS;                                   \
    static constexpr inline ClassTokenType ClassToken() {                                          \
        return ::Kernel::ClassToken<CLASS>;                                                        \
    }                                                                                              \
                                                                                                   \
public:                                                                                            \
    using BaseClass = BASE_CLASS;                                                                  \
    static constexpr TypeObj GetStaticTypeObj() {                                                  \
        constexpr ClassTokenType Token = ClassToken();                                             \
        return TypeObj(TypeName, Token);                                                           \
    }                                                                                              \
    static constexpr const char* GetStaticTypeName() {                                             \
        return TypeName;                                                                           \
    }                                                                                              \
    virtual TypeObj GetTypeObj() const {                                                           \
        return GetStaticTypeObj();                                                                 \
    }                                                                                              \
    virtual const char* GetTypeName() const {                                                      \
        return GetStaticTypeName();                                                                \
    }                                                                                              \
                                                                                                   \
private:                                                                                           \
    constexpr bool operator!=(const TypeObj& rhs)

class KAutoObject {
protected:
    class TypeObj {
    public:
        constexpr explicit TypeObj(const char* n, ClassTokenType tok)
            : m_name(n), m_class_token(tok) {}

        constexpr const char* GetName() const {
            return m_name;
        }
        constexpr ClassTokenType GetClassToken() const {
            return m_class_token;
        }

        constexpr bool operator==(const TypeObj& rhs) const {
            return this->GetClassToken() == rhs.GetClassToken();
        }

        constexpr bool operator!=(const TypeObj& rhs) const {
            return this->GetClassToken() != rhs.GetClassToken();
        }

        constexpr bool IsDerivedFrom(const TypeObj& rhs) const {
            return (this->GetClassToken() | rhs.GetClassToken()) == this->GetClassToken();
        }

    private:
        const char* m_name;
        ClassTokenType m_class_token;
    };

private:
    KERNEL_AUTOOBJECT_TRAITS(KAutoObject, KAutoObject);

public:
    explicit KAutoObject(KernelCore& kernel_) : kernel(kernel_) {
        RegisterWithKernel();
    }
    virtual ~KAutoObject() {
        UnregisterWithKernel();
    }

    static KAutoObject* Create(KAutoObject* ptr);

    // Destroy is responsible for destroying the auto object's resources when ref_count hits zero.
    virtual void Destroy() {
        UNIMPLEMENTED();
    }

    // Finalize is responsible for cleaning up resource, but does not destroy the object.
    virtual void Finalize() {}

    virtual KProcess* GetOwner() const {
        return nullptr;
    }

    u32 GetReferenceCount() const {
        return m_ref_count.load();
    }

    bool IsDerivedFrom(const TypeObj& rhs) const {
        return this->GetTypeObj().IsDerivedFrom(rhs);
    }

    bool IsDerivedFrom(const KAutoObject& rhs) const {
        return this->IsDerivedFrom(rhs.GetTypeObj());
    }

    template <typename Derived>
    Derived DynamicCast() {
        static_assert(std::is_pointer_v<Derived>);
        using DerivedType = std::remove_pointer_t<Derived>;

        if (this->IsDerivedFrom(DerivedType::GetStaticTypeObj())) {
            return static_cast<Derived>(this);
        } else {
            return nullptr;
        }
    }

    template <typename Derived>
    const Derived DynamicCast() const {
        static_assert(std::is_pointer_v<Derived>);
        using DerivedType = std::remove_pointer_t<Derived>;

        if (this->IsDerivedFrom(DerivedType::GetStaticTypeObj())) {
            return static_cast<Derived>(this);
        } else {
            return nullptr;
        }
    }

    bool Open() {
        // Atomically increment the reference count, only if it's positive.
        u32 cur_ref_count = m_ref_count.load(std::memory_order_acquire);
        do {
            if (cur_ref_count == 0) {
                return false;
            }
            ASSERT(cur_ref_count < cur_ref_count + 1);
        } while (!m_ref_count.compare_exchange_weak(cur_ref_count, cur_ref_count + 1,
                                                    std::memory_order_relaxed));

        return true;
    }

    void Close() {
        // Atomically decrement the reference count, not allowing it to become negative.
        u32 cur_ref_count = m_ref_count.load(std::memory_order_acquire);
        do {
            ASSERT(cur_ref_count > 0);
        } while (!m_ref_count.compare_exchange_weak(cur_ref_count, cur_ref_count - 1,
                                                    std::memory_order_relaxed));

        // If ref count hits zero, destroy the object.
        if (cur_ref_count - 1 == 0) {
            this->Destroy();
        }
    }

private:
    void RegisterWithKernel();
    void UnregisterWithKernel();

protected:
    KernelCore& kernel;
    std::string name;

private:
    std::atomic<u32> m_ref_count{};
};

class KAutoObjectWithListContainer;

class KAutoObjectWithList : public KAutoObject, public boost::intrusive::set_base_hook<> {
public:
    explicit KAutoObjectWithList(KernelCore& kernel_) : KAutoObject(kernel_) {}

    static int Compare(const KAutoObjectWithList& lhs, const KAutoObjectWithList& rhs) {
        const u64 lid = lhs.GetId();
        const u64 rid = rhs.GetId();

        if (lid < rid) {
            return -1;
        } else if (lid > rid) {
            return 1;
        } else {
            return 0;
        }
    }

    friend bool operator<(const KAutoObjectWithList& left, const KAutoObjectWithList& right) {
        return &left < &right;
    }

public:
    virtual u64 GetId() const {
        return reinterpret_cast<u64>(this);
    }

    virtual const std::string& GetName() const {
        return name;
    }

private:
    friend class KAutoObjectWithListContainer;
};

template <typename T>
class KScopedAutoObject {
    YUZU_NON_COPYABLE(KScopedAutoObject);

public:
    constexpr KScopedAutoObject() = default;

    constexpr KScopedAutoObject(T* o) : m_obj(o) {
        if (m_obj != nullptr) {
            m_obj->Open();
        }
    }

    ~KScopedAutoObject() {
        if (m_obj != nullptr) {
            m_obj->Close();
        }
        m_obj = nullptr;
    }

    template <typename U>
    requires(std::derived_from<T, U> ||
             std::derived_from<U, T>) constexpr KScopedAutoObject(KScopedAutoObject<U>&& rhs) {
        if constexpr (std::derived_from<U, T>) {
            // Upcast.
            m_obj = rhs.m_obj;
            rhs.m_obj = nullptr;
        } else {
            // Downcast.
            T* derived = nullptr;
            if (rhs.m_obj != nullptr) {
                derived = rhs.m_obj->template DynamicCast<T*>();
                if (derived == nullptr) {
                    rhs.m_obj->Close();
                }
            }

            m_obj = derived;
            rhs.m_obj = nullptr;
        }
    }

    constexpr KScopedAutoObject<T>& operator=(KScopedAutoObject<T>&& rhs) {
        rhs.Swap(*this);
        return *this;
    }

    constexpr T* operator->() {
        return m_obj;
    }
    constexpr T& operator*() {
        return *m_obj;
    }

    constexpr void Reset(T* o) {
        KScopedAutoObject(o).Swap(*this);
    }

    constexpr T* GetPointerUnsafe() {
        return m_obj;
    }

    constexpr T* GetPointerUnsafe() const {
        return m_obj;
    }

    constexpr T* ReleasePointerUnsafe() {
        T* ret = m_obj;
        m_obj = nullptr;
        return ret;
    }

    constexpr bool IsNull() const {
        return m_obj == nullptr;
    }
    constexpr bool IsNotNull() const {
        return m_obj != nullptr;
    }

private:
    template <typename U>
    friend class KScopedAutoObject;

private:
    T* m_obj{};

private:
    constexpr void Swap(KScopedAutoObject& rhs) noexcept {
        std::swap(m_obj, rhs.m_obj);
    }
};

} // namespace Kernel
