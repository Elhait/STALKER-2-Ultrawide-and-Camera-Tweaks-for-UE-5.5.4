#include "../../src/hooks/signature_scanner.hpp"
#include "../../src/hooks/signatures/signature_definitions.hpp"

#include <windows.h>

#include <cstddef>
#include <iostream>

namespace
{
    bool CheckPattern(const char* name, const char* pattern, std::size_t expectedSize)
    {
        const auto parsed = hooks::ParseSignatureForTest(pattern);
        if (parsed.size() != expectedSize) {
            std::cerr << name << ": unexpected token count " << parsed.size() << "\n";
            return false;
        }
        return true;
    }

    bool CheckEnterWildcards()
    {
        const auto parsed = hooks::ParseSignatureForTest(hooks::signatures::CinematicEnter);
        constexpr std::size_t wildcardRanges[][2] = {{4, 7}, {9, 12}, {17, 20}, {27, 30}};
        for (std::size_t index = 0; index < parsed.size(); ++index) {
            bool expectedWildcard = false;
            for (const auto& range : wildcardRanges)
                expectedWildcard = expectedWildcard || (index >= range[0] && index <= range[1]);
            if ((parsed[index] == -1) != expectedWildcard) {
                std::cerr << "CinematicEnter: wildcard position mismatch at token " << index << "\n";
                return false;
            }
        }
        return true;
    }

    bool CheckMixedTokens()
    {
        const auto singleWildcard = hooks::ParseSignatureForTest("AA ?? BB");
        if (singleWildcard.size() != 3 || singleWildcard[0] != 0xAA ||
            singleWildcard[1] != -1 || singleWildcard[2] != 0xBB) {
            std::cerr << "AA ?? BB: token or wildcard mismatch\n";
            return false;
        }

        const auto doubleWildcard = hooks::ParseSignatureForTest("AA ?? ?? BB");
        if (doubleWildcard.size() != 4 || doubleWildcard[0] != 0xAA ||
            doubleWildcard[1] != -1 || doubleWildcard[2] != -1 ||
            doubleWildcard[3] != 0xBB) {
            std::cerr << "AA ?? ?? BB: token or wildcard mismatch\n";
            return false;
        }
        return true;
    }

    bool CheckMalformedInputTerminates()
    {
        const auto incompleteWildcard = hooks::ParseSignatureForTest("AA ?");
        if (incompleteWildcard.size() != 2 || incompleteWildcard[1] != -1) {
            std::cerr << "AA ?: malformed wildcard handling mismatch\n";
            return false;
        }

        const auto invalidToken = hooks::ParseSignatureForTest("AA ZZ");
        if (invalidToken.size() != 1 || invalidToken[0] != 0xAA) {
            std::cerr << "AA ZZ: malformed token handling mismatch\n";
            return false;
        }
        return true;
    }
}

int main()
{
    if (!CheckPattern("DialogueBoundary", hooks::signatures::DialogueBoundary, 23) ||
        !CheckPattern("CinematicAspectSetter", hooks::signatures::CinematicAspectSetter, 38) ||
        !CheckPattern("CinematicEnter", hooks::signatures::CinematicEnter, 31) ||
        !CheckPattern("CinematicExit", hooks::signatures::CinematicExit, 28) ||
        !CheckPattern("CinematicExitIndexed", hooks::signatures::CinematicExitIndexed, 65) ||
        !CheckEnterWildcards() ||
        !CheckMixedTokens() ||
        !CheckMalformedInputTerminates())
        return 1;

    for (const auto* signature : {
        hooks::signatures::DialogueBoundary,
        hooks::signatures::CinematicAspectSetter,
        hooks::signatures::CinematicEnter,
        hooks::signatures::CinematicExit,
        hooks::signatures::CinematicExitIndexed,
    }) {
        try {
            const auto matches = hooks::FindAll(GetModuleHandleW(nullptr), signature);
            if (!matches.empty()) {
                std::cerr << "unexpected match in harness image\n";
                return 1;
            }
        } catch (...) {
            std::cerr << "FindAll threw while scanning a production signature\n";
            return 1;
        }
    }

    std::cout << "Signature scanner harness: PASS\n";
    return 0;
}
