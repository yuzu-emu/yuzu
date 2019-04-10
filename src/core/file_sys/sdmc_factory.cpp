// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>
#include "core/file_sys/registered_cache.h"
#include "core/file_sys/sdmc_factory.h"
#include "core/file_sys/xts_archive.h"

namespace FileSys {

SDMCFactory::SDMCFactory(VirtualDir dir_)
    : dir(std::move(dir_)), contents(std::make_unique<RegisteredCache>(
                                GetOrCreateDirectoryRelative(dir, "/Nintendo/Contents/registered"),
                                [](const VirtualFile& file, const NcaID& id) {
                                    return NAX{file, id}.GetDecrypted();
                                })),
      placeholder(std::make_unique<PlaceholderCache>(
          GetOrCreateDirectoryRelative(dir, "/Nintendo/Contents/placehld"))) {}

SDMCFactory::~SDMCFactory() = default;

ResultVal<VirtualDir> SDMCFactory::Open() {
    return MakeResult<VirtualDir>(dir);
}

VirtualDir SDMCFactory::GetSDMCContentDirectory() const {
    return GetOrCreateDirectoryRelative(dir, "/Nintendo/Contents");
}

RegisteredCache* SDMCFactory::GetSDMCContents() const {
    return contents.get();
}

PlaceholderCache* SDMCFactory::GetSDMCPlaceholder() const {
    return placeholder.get();
}

} // namespace FileSys
