#include "../../src/platform/win32/memory.hpp"

#include <Windows.h>

#include <cstdint>
#include <cstdio>
#include <limits>

namespace
{
    class PageAllocation
    {
    public:
        PageAllocation()
        {
            SYSTEM_INFO info{};
            GetSystemInfo(&info);
            size_ = info.dwPageSize;
            address_ = VirtualAlloc(nullptr, size_, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        }

        ~PageAllocation()
        {
            if (address_) VirtualFree(address_, 0, MEM_RELEASE);
        }

        PageAllocation(const PageAllocation&) = delete;
        PageAllocation& operator=(const PageAllocation&) = delete;

        void* Address() const { return address_; }
        SIZE_T Size() const { return size_; }

        bool Protect(DWORD protection) const
        {
            DWORD previous{};
            return address_ && VirtualProtect(address_, size_, protection, &previous) != FALSE;
        }

    private:
        void* address_{};
        SIZE_T size_{};
    };

    bool TestReadablePage()
    {
        PageAllocation page;
        if (!page.Address()) return false;
        constexpr std::uint32_t expected = 0x13579BDF;
        *static_cast<std::uint32_t*>(page.Address()) = expected;
        std::uint32_t observed{};
        return platform::win32::ReadMemory(reinterpret_cast<std::uintptr_t>(page.Address()),
            &observed, sizeof(observed)) && observed == expected;
    }

    bool TestExecuteOnlyPageIsRefused()
    {
        PageAllocation page;
        std::uint8_t observed{};
        return page.Address() && page.Protect(PAGE_EXECUTE) &&
            !platform::win32::ReadMemory(reinterpret_cast<std::uintptr_t>(page.Address()),
                &observed, sizeof(observed));
    }

    bool TestNoAccessPageIsRefused()
    {
        PageAllocation page;
        std::uint8_t observed{};
        return page.Address() && page.Protect(PAGE_NOACCESS) &&
            !platform::win32::ReadMemory(reinterpret_cast<std::uintptr_t>(page.Address()),
                &observed, sizeof(observed));
    }

    bool TestGuardPageIsRefused()
    {
        PageAllocation page;
        std::uint8_t observed{};
        return page.Address() && page.Protect(PAGE_READWRITE | PAGE_GUARD) &&
            !platform::win32::ReadMemory(reinterpret_cast<std::uintptr_t>(page.Address()),
                &observed, sizeof(observed));
    }

    bool TestWritableProtectionContracts()
    {
        PageAllocation page;
        if (!page.Address()) return false;
        const auto address = reinterpret_cast<std::uintptr_t>(page.Address());
        if (!page.Protect(PAGE_READWRITE) || !platform::win32::IsWritable(address, 4)) return false;
        if (!platform::win32::IsWritable(address, page.Size()) ||
            platform::win32::IsWritable(address + page.Size() - 1, 2)) return false;
        if (!page.Protect(PAGE_EXECUTE_READWRITE) || !platform::win32::IsWritable(address, 4)) return false;
        if (!page.Protect(PAGE_READONLY) || platform::win32::IsWritable(address, 4)) return false;
        if (!page.Protect(PAGE_EXECUTE_READ) || platform::win32::IsWritable(address, 4)) return false;
        if (!page.Protect(PAGE_NOACCESS) || platform::win32::IsWritable(address, 4)) return false;
        return !platform::win32::IsWritable(0, 4) &&
            !platform::win32::IsWritable((std::numeric_limits<std::uintptr_t>::max)(), 4);
    }
}

int main()
{
    const bool readable = TestReadablePage();
    const bool executeOnly = TestExecuteOnlyPageIsRefused();
    const bool noAccess = TestNoAccessPageIsRefused();
    const bool guard = TestGuardPageIsRefused();
    const bool writable = TestWritableProtectionContracts();
    std::printf("readable=%s execute_only=%s noaccess=%s guard=%s writable=%s\n",
        readable ? "PASS" : "FAIL", executeOnly ? "PASS" : "FAIL",
        noAccess ? "PASS" : "FAIL", guard ? "PASS" : "FAIL",
        writable ? "PASS" : "FAIL");
    return readable && executeOnly && noAccess && guard && writable ? 0 : 1;
}
