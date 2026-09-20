#pragma once

#include "../provenance.hpp"

namespace runtime_evidence::gameplay
{
    inline constexpr Provenance kProvenance{
        "gameplay-horplus-32x9", "GLOBAL_HORPLUS_RUNTIME_PASS.md",
        "UNKNOWN", "UNKNOWN", "UNKNOWN"};

    struct HorPlusCase
    {
        float nativeFov;
        float aspect;
        float expectedHorPlus;
    };

    // Recorded gameplay evidence from the non-90 FOV 32:9 run.
    inline constexpr HorPlusCase kRecordedGameplay{112.623f, 3.55556f, 143.132f};

    // The fixture intentionally sweeps arbitrary aspects, not a 21:9/32:9 whitelist.
    inline constexpr HorPlusCase kAspectSweep[] = {
        {90.0f, 2.0f, 98.213f},
        {110.0f, 2.4f, 123.625f},
        {112.623f, 3.55556f, 143.132f},
    };
}
