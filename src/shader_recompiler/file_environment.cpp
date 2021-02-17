#include <cstdio>

#include "exception.h"
#include "file_environment.h"

namespace Shader {

FileEnvironment::FileEnvironment(const char* path) {
    std::FILE* const file{std::fopen(path, "rb")};
    if (!file) {
        throw RuntimeError("Failed to open file='{}'", path);
    }
    std::fseek(file, 0, SEEK_END);
    const long size{std::ftell(file)};
    std::rewind(file);
    if (size % 8 != 0) {
        std::fclose(file);
        throw RuntimeError("File size={} is not aligned to 8", size);
    }
    // TODO: Use a unique_ptr to avoid zero-initializing this
    const size_t num_inst{static_cast<size_t>(size) / 8};
    data.resize(num_inst);
    if (std::fread(data.data(), 8, num_inst, file) != num_inst) {
        std::fclose(file);
        throw RuntimeError("Failed to read instructions={} from file='{}'", num_inst, path);
    }
    std::fclose(file);
}

FileEnvironment::~FileEnvironment() = default;

u64 FileEnvironment::ReadInstruction(u32 offset) {
    if (offset % 8 != 0) {
        throw InvalidArgument("offset={} is not aligned to 8", offset);
    }
    if (offset / 8 >= static_cast<u32>(data.size())) {
        throw InvalidArgument("offset={} is out of bounds", offset);
    }
    return data[offset / 8];
}

std::array<u32, 3> FileEnvironment::WorkgroupSize() {
    return {1, 1, 1};
}

} // namespace Shader
