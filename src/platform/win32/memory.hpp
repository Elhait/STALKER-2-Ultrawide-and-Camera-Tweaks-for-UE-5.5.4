#pragma once

#include <cstddef>
#include <cstdint>

namespace platform::win32
{
    bool IsWritable(std::uintptr_t address, std::size_t size);
    bool ReadMemory(std::uintptr_t address, void* destination, std::size_t size);
}
