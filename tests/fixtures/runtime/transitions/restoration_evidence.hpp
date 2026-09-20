#pragma once

#include "../provenance.hpp"

#include <cstdint>

namespace runtime_evidence::transitions
{
    inline constexpr Provenance kProvenance{
        "gameplay-aspect-restoration-roundtrip", "GLOBAL_HORPLUS_RUNTIME_PASS.md",
        "UNKNOWN", "UNKNOWN", "UNKNOWN"};

    struct RestorationCase
    {
        float retainedAspect;
        std::uintptr_t source;
        float nativeAspect;
    };

    // Recorded live-switch contract: ultrawide target is retained, invalidated
    // for AspectRecalculation, then republished before the return to HorPlus.
    inline constexpr RestorationCase kRecordedRoundTrip{3.55556f, 0x1000, 1.77778f};
}
