// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <queue>
#include <string>
#include "common/common_types.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/wait_object.h"
#include "core/hle/result.h"

namespace Kernel {

class Semaphore final : public WaitObject {
public:
    /**
     * Creates a semaphore.
     * @param guest_addr Address of the object tracking the semaphore in guest memory. If specified,
     * this semaphore will update the guest object when its state changes.
     * @param mutex_addr Optional address of a guest mutex associated with this semaphore, used by
     * the OS for implementing events.
     * @param name Optional name of semaphore.
     * @return The created semaphore.
     */
    static ResultVal<SharedPtr<Semaphore>> Create(VAddr guest_addr, VAddr mutex_addr = 0,
                                                  std::string name = "Unknown");

    std::string GetTypeName() const override {
        return "Semaphore";
    }
    std::string GetName() const override {
        return name;
    }

    static const HandleType HANDLE_TYPE = HandleType::Semaphore;
    HandleType GetHandleType() const override {
        return HANDLE_TYPE;
    }

    s32 max_count;       ///< Maximum number of simultaneous holders the semaphore can have
    s32 available_count; ///< Number of free slots left in the semaphore
    std::string name;    ///< Name of semaphore (optional)
    VAddr guest_addr;    ///< Address of the guest semaphore value
    VAddr mutex_addr; ///< (optional) Address of guest mutex value associated with this semaphore,
                      ///< used for implementing events

    bool ShouldWait(Thread* thread) const override;
    void Acquire(Thread* thread) override;

    /**
     * Releases a certain number of slots from a semaphore.
     * @param release_count The number of slots to release
     * @return The number of free slots the semaphore had before this call
     */
    ResultVal<s32> Release(s32 release_count);

private:
    Semaphore();
    ~Semaphore() override;

    /// Updates the state of the object tracking this semaphore in guest memory
    void UpdateGuestState();
};

} // namespace Kernel
