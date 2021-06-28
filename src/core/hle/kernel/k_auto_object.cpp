// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/kernel/k_auto_object.h"
#include "core/hle/kernel/kernel.h"

namespace Kernel {

KAutoObject* KAutoObject::Create(KAutoObject* obj) {
    obj->m_ref_count = 1;
    return obj;
}

void KAutoObject::RegisterWithKernel() {
    kernel.RegisterKernelObject(this);
}

void KAutoObject::UnregisterWithKernel() {
    kernel.UnregisterKernelObject(this);
}

} // namespace Kernel
