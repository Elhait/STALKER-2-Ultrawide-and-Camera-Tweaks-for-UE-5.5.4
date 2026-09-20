#include "../../src/gameplay/horplus_gameplay.hpp"
#include "../../src/camera/horplus.hpp"
#include "../../src/cinematics/cinematic_fov.hpp"

#include <cmath>
#include <iostream>

namespace
{
    bool Check(bool condition, const char* name)
    {
        if (!condition) std::cerr << name << ": FAIL\n";
        return condition;
    }
}

int main()
{
    constexpr float native = 16.0f / 9.0f;
    constexpr float aspect21 = 21.0f / 9.0f;
    constexpr float aspect2560x1080 = 2560.0f / 1080.0f;
    constexpr float aspect3440x1440 = 3440.0f / 1440.0f;
    constexpr float aspect3840x1600 = 3840.0f / 1600.0f;
    constexpr float aspect3840x1200 = 3840.0f / 1200.0f;
    constexpr float aspect32 = 32.0f / 9.0f;
    float transformed = 0.0f;

    const auto eligibleResult = gameplay::EvaluateHorPlus(90.0f, aspect21, 0x4, native);
    const auto identityResult = gameplay::EvaluateHorPlus(90.0f, native, 0x4, native);
    const auto invalidFlagsResult = gameplay::EvaluateHorPlus(90.0f, aspect21, 0x6, native);
    const auto invalidFovResult = gameplay::EvaluateHorPlus(0.0f, aspect21, 0x4, native);
    const auto invalidAspectResult = gameplay::EvaluateHorPlus(
        90.0f, std::numeric_limits<float>::quiet_NaN(), 0x4, native);

    const bool twentyOneOff = gameplay::TryTransformHorPlus(90.0f, aspect21, 0x4, native, transformed);
    const float twentyOneValue = transformed;
    const bool twentyOneOn = gameplay::TryTransformHorPlus(90.0f, aspect21, 0x5, native, transformed);
    const float twentyOneConstrainedValue = transformed;
    const bool actualTwentyOneOff = gameplay::TryTransformHorPlus(
        90.0f, aspect3440x1440, 0x4, native, transformed);
    const float actualTwentyOneValue = transformed;
    const bool actualTwentyOneOn = gameplay::TryTransformHorPlus(
        90.0f, aspect3440x1440, 0x5, native, transformed);
    const float actualTwentyOneConstrainedValue = transformed;
    const bool customTwentyOne = gameplay::TryTransformHorPlus(
        90.0f, aspect2560x1080, 0x4, native, transformed) &&
        gameplay::TryTransformHorPlus(90.0f, aspect3840x1600, 0x5, native, transformed);
    const bool customSuperUltrawide = gameplay::TryTransformHorPlus(
        90.0f, aspect3840x1200, 0x4, native, transformed);
    const bool thirtyTwo = gameplay::TryTransformHorPlus(90.0f, aspect32, 0x4, native, transformed);
    const bool nativeIdentity = !gameplay::TryTransformHorPlus(90.0f, native, 0x4, native, transformed) &&
        transformed == 90.0f;
    const bool invalidFlags = !gameplay::TryTransformHorPlus(90.0f, aspect21, 0x6, native, transformed);
    float matchGameplay = 0.0f;
    const bool matchGameplayValid = cinematics::TryTransformMatchGameplay(
        90.0f, 112.623f, 90.0f, aspect32, native, matchGameplay);
    const bool matchGameplayIdentity = matchGameplayValid && std::fabs(matchGameplay - 143.132f) <= 0.01f;
    float unifiedNative = 0.0f;
    const bool unifiedNativeValid = cinematics::TryTransformCinematicFov(
        90.0f, 90.0f, 90.0f, aspect32, native, unifiedNative);
    const bool unifiedNativeIdentity = unifiedNativeValid &&
        std::fabs(unifiedNative - camera::HorPlus(90.0f, aspect32, native)) <= 0.001f;
    float unifiedGameplay = 0.0f;
    const bool unifiedGameplayValid = cinematics::TryTransformCinematicFov(
        90.0f, 90.0f, 112.623f, aspect32, native, unifiedGameplay);
    const bool unifiedMatchesWrapper = unifiedGameplayValid && matchGameplayValid &&
        std::fabs(unifiedGameplay - matchGameplay) <= 0.001f;
    const bool matchGameplayInvalid = !cinematics::TryTransformMatchGameplay(
        90.0f, std::numeric_limits<float>::quiet_NaN(), 90.0f, aspect32, native, matchGameplay);
    const bool sameMultiplier = twentyOneOff && twentyOneOn &&
        std::fabs(twentyOneValue - twentyOneConstrainedValue) <= 0.001f;
    const bool actualAspectAccepted = actualTwentyOneOff && actualTwentyOneOn &&
        std::fabs(actualTwentyOneValue - actualTwentyOneConstrainedValue) <= 0.001f;
    const bool result = Check(twentyOneOff, "21x9_off") && Check(twentyOneOn, "21x9_on") &&
        Check(thirtyTwo, "32x9") && Check(nativeIdentity, "16x9_identity") &&
        Check(invalidFlags, "invalid_flags") && Check(sameMultiplier, "flags_policy") &&
        Check(actualAspectAccepted, "3440x1440_aspect") &&
        Check(customTwentyOne, "custom_21x9_family") &&
        Check(customSuperUltrawide, "custom_super_ultrawide") &&
        Check(matchGameplayIdentity, "matchgameplay_identity") &&
        Check(unifiedNativeIdentity, "unified_native_identity") &&
        Check(unifiedMatchesWrapper, "unified_gameplay_matches_wrapper") &&
        Check(matchGameplayInvalid, "matchgameplay_invalid_context") &&
        Check(eligibleResult.eligible && eligibleResult.applied &&
            eligibleResult.outputFov > eligibleResult.inputFov, "result_eligible_applied") &&
        Check(!identityResult.eligible && !identityResult.applied &&
            identityResult.outputFov == identityResult.inputFov, "result_identity_bypass") &&
        Check(!invalidFlagsResult.eligible && !invalidFlagsResult.applied &&
            invalidFlagsResult.outputFov == invalidFlagsResult.inputFov, "result_invalid_flags_bypass") &&
        Check(!invalidFovResult.eligible && !invalidFovResult.applied &&
            invalidFovResult.outputFov == invalidFovResult.inputFov, "result_invalid_fov_bypass") &&
        Check(!invalidAspectResult.eligible && !invalidAspectResult.applied &&
            invalidAspectResult.outputFov == invalidAspectResult.inputFov, "result_invalid_aspect_bypass");
    std::cout << "HorPlus gameplay harness: " << (result ? "PASS" : "FAIL") << "\n";
    return result ? 0 : 1;
}
