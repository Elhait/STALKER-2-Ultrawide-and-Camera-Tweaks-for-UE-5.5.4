#include "memory.hpp"

#include <cstring>
#include <windows.h>

namespace platform::win32
{
    namespace
    {
        bool IsReadableProtection(DWORD protection)
        {
            switch (protection & 0xFF) {
            case PAGE_READONLY:
            case PAGE_READWRITE:
            case PAGE_WRITECOPY:
            case PAGE_EXECUTE_READ:
            case PAGE_EXECUTE_READWRITE:
            case PAGE_EXECUTE_WRITECOPY:
                return true;
            default:
                return false;
            }
        }
    }

    bool IsWritable(std::uintptr_t address, std::size_t size)
    {
        MEMORY_BASIC_INFORMATION info{};
        if (!address || !VirtualQuery(reinterpret_cast<const void*>(address), &info, sizeof(info)) ||
            info.State != MEM_COMMIT || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return false;
        const auto regionEnd = reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize;
        const DWORD writable = PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
        return address <= regionEnd && size <= regionEnd - address && (info.Protect & writable);
    }

    bool ReadMemory(std::uintptr_t address, void* destination, std::size_t size)
    {
        MEMORY_BASIC_INFORMATION info{};
        if (!address || !destination || !VirtualQuery(reinterpret_cast<const void*>(address), &info, sizeof(info)) ||
            info.State != MEM_COMMIT || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)) ||
            !IsReadableProtection(info.Protect)) return false;
        const auto regionEnd = reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize;
        if (address > regionEnd || size > regionEnd - address) return false;
        std::memcpy(destination, reinterpret_cast<const void*>(address), size);
        return true;
    }
}
