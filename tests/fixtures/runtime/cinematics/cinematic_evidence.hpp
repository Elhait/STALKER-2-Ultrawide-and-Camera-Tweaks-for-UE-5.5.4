#pragma once

#include "../provenance.hpp"

namespace runtime_evidence::cinematics
{
    inline constexpr Provenance kProvenance{
        "cinematic-native-and-gameplay-horplus", "GLOBAL_HORPLUS_RUNTIME_PASS.md",
        "UNKNOWN", "UNKNOWN", "UNKNOWN"};

    struct CinematicCase
    {
        float authoredEnterFov;
        float gameplayBaselineNativeFov;
        float aspect;
        float nativeAspect;
        float expectedNativeHorPlus;
        float expectedGameplayHorPlus;
    };

    // Recorded 32:9 evidence: authored 90 -> 126.87 in NativeHorPlus and
    // gameplay baseline 112.623 -> approximately 143.132 in GameplayHorPlus.
    inline constexpr CinematicCase kRecorded32x9{
        90.0f, 112.623f, 3.55556f, 1.77778f, 126.87f, 143.132f};

    // Recorded forced-21:9 evidence from the combined runtime session:
    // authored 90 -> 106.688 in NativeHorPlus and -> 126.988 when the
    // retained GameplayHorPlus baseline is 112.344.
    inline constexpr CinematicCase kRecorded21x9{
        90.0f, 112.344f, 2.38889f, 1.77778f, 106.688f, 126.988f};
}
