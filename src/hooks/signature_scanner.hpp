#pragma once

#include <cstdint>
#include <vector>

namespace hooks
{
    struct ExecutableSpan
    {
        std::uint8_t* begin{};
        std::size_t size{};

        bool Contains(const std::uint8_t* address, std::size_t length) const noexcept
        {
            if (!begin || address < begin || length > size) return false;
            return static_cast<std::size_t>(address - begin) <= size - length;
        }
    };

    bool FindSectionSpan(void* module, const char* sectionName, ExecutableSpan& span);

    // Preconditions: module is a valid loaded PE image owned by the current
    // process and signature is an internally authored pattern using the
    // repository's trusted byte/wildcard syntax. Arbitrary external or
    // malformed pattern input is not a supported API surface.
    std::vector<std::uint8_t*> FindAll(void* module, const char* signature);

#ifdef SIGNATURE_SCANNER_TEST
    std::vector<int> ParseSignatureForTest(const char* signature);
#endif
}
