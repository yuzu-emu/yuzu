// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/spl/spl.h"

namespace Service::SPL {

SPL::SPL(std::shared_ptr<Module> interface_module)
    : Module::Interface(std::move(interface_module), "spl:") {
    static const FunctionInfo functions[] = {
        {0, nullptr, "GetConfig"},
        {1, nullptr, "ModularExponentiate"},
        {2, nullptr, "GenerateAesKek"},
        {3, nullptr, "LoadAesKey"},
        {4, nullptr, "GenerateAesKey"},
        {5, nullptr, "SetConfig"},
        {7, &SPL::GetRandomBytes, "GetRandomBytes"},
        {9, nullptr, "ImportLotusKey"},
        {10, nullptr, "DecryptLotusMessage"},
        {11, nullptr, "IsDevelopment"},
        {12, nullptr, "GenerateSpecificAesKey"},
        {13, nullptr, "DecryptDeviceUniqueData"},
        {14, nullptr, "DecryptAesKey"},
        {15, nullptr, "CryptAesCtr"},
        {16, nullptr, "ComputeCmac"},
        {17, nullptr, "ImportEsKey"},
        {18, nullptr, "UnwrapTitleKey"},
        {19, nullptr, "LoadTitleKey"},
        {20, nullptr, "PrepareEsCommonKey"},
        {21, nullptr, "AllocateAesKeyslot"},
        {22, nullptr, "DeallocateAesKeySlot"},
        {23, nullptr, "GetAesKeyslotAvailableEvent"},
        {24, nullptr, "SetBootReason"},
        {25, nullptr, "GetBootReason"},
        {26, nullptr, "DecryptAndStoreSslClientCertKey"},
        {27, nullptr, "ModularExponentiateWithSslClientCertKey"},
        {28, nullptr, "DecryptAndStoreDrmDeviceCertKey"},
        {29, nullptr, "ModularExponentiateWithDrmDeviceCertKey"},
        {30, nullptr, "ReencryptDeviceUniqueData "},
        {31, nullptr, "PrepareEsArchiveKey"}, // This is also GetPackage2Hash?
        {32, nullptr, "LoadPreparedAesKey"},
    };
    RegisterHandlers(functions);
}

SPL::~SPL() = default;

} // namespace Service::SPL
