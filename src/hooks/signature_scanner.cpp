#include "signature_scanner.hpp"

#include <cstdlib>
#include <cstring>
#include <windows.h>

namespace
{
    // FindAll is called only with internally authored signatures and a valid
    // loaded module. This parser intentionally is not a general-purpose
    // untrusted pattern parser.
    std::vector<int> PatternBytes(const char* pattern)
    {
        std::vector<int> bytes;
        auto* current = const_cast<char*>(pattern);
        auto* end = current + std::strlen(current);
        while (current < end) {
            while (current < end && *current == ' ') ++current;
            if (current >= end) break;
            if (*current == '?') {
                ++current;
                if (current < end && *current == '?') ++current;
                bytes.push_back(-1);
            } else {
                auto* tokenEnd = current;
                const auto value = std::strtoul(current, &tokenEnd, 16);
                if (tokenEnd == current) break;
                current = tokenEnd;
                bytes.push_back(value);
            }
        }
        return bytes;
    }

    template <typename Callback>
    void ForEachExecutableSection(void* module, Callback&& callback)
    {
        const auto* base = reinterpret_cast<const std::uint8_t*>(module);
        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
        auto* section = IMAGE_FIRST_SECTION(nt);
        for (WORD index = 0; index < nt->FileHeader.NumberOfSections; ++index, ++section) {
            if ((section->Characteristics & IMAGE_SCN_MEM_EXECUTE) == 0) continue;
            auto* sectionStart = const_cast<std::uint8_t*>(base) + section->VirtualAddress;
            const auto sectionSize = static_cast<std::size_t>(section->Misc.VirtualSize);
            if (sectionSize != 0) callback(sectionStart, sectionSize);
        }
    }

    std::vector<std::uint8_t*> PatternScanAll(void* module, const char* signature)
    {
        const auto patternBytes = PatternBytes(signature);
        std::vector<std::uint8_t*> results;
        ForEachExecutableSection(module, [&](std::uint8_t* scanBytes, std::size_t scanSize) {
            if (patternBytes.size() > scanSize) return;
            for (std::size_t offset = 0; offset <= scanSize - patternBytes.size(); ++offset) {
                    bool found = true;
                    for (std::size_t index = 0; index < patternBytes.size(); ++index) {
                        if (patternBytes[index] != -1 && scanBytes[offset + index] != patternBytes[index]) {
                            found = false;
                            break;
                        }
                    }
                    if (found) results.push_back(scanBytes + offset);
                }
        });
        return results;
    }
}

namespace hooks
{
    bool FindSectionSpan(void* module, const char* sectionName, ExecutableSpan& span)
    {
        span = {};
        if (!module || !sectionName) return false;
        const auto* base = reinterpret_cast<const std::uint8_t*>(module);
        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
        const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE) return false;
        auto* section = IMAGE_FIRST_SECTION(nt);
        for (WORD index = 0; index < nt->FileHeader.NumberOfSections; ++index, ++section) {
            char name[IMAGE_SIZEOF_SHORT_NAME + 1]{};
            std::memcpy(name, section->Name, IMAGE_SIZEOF_SHORT_NAME);
            if (std::strcmp(name, sectionName) != 0) continue;
            if (section->Misc.VirtualSize == 0) return false;
            span.begin = const_cast<std::uint8_t*>(base) + section->VirtualAddress;
            span.size = static_cast<std::size_t>(section->Misc.VirtualSize);
            return true;
        }
        return false;
    }

    std::vector<std::uint8_t*> FindAll(void* module, const char* signature)
    {
        return PatternScanAll(module, signature);
    }

#ifdef SIGNATURE_SCANNER_TEST
    std::vector<int> ParseSignatureForTest(const char* signature)
    {
        return PatternBytes(signature);
    }
#endif
}
