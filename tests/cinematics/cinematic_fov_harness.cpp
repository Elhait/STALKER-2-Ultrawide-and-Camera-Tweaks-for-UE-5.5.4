#include "../../src/cinematics/cinematic_fov.hpp"

#include <cmath>
#include <iostream>
#include <limits>

namespace
{
    bool Check(bool condition, const char* name)
    {
        if (!condition) std::cerr << name << ": FAIL\n";
        return condition;
    }

    camera::GameplayBaseline Baseline(float native, float aspect, bool transformed)
    {
        camera::GameplayBaseline baseline{};
        baseline.valid = true;
        baseline.nativeFov = { native, camera::FovSpace::Native,
            camera::FovProvenance::NativeRegisterInput, true };
        baseline.aspect = aspect;
        baseline.source = { 0x1000, true };
        baseline.observationSequence = 7;
        if (transformed) {
            baseline.horPlusFov = { 143.132f, camera::FovSpace::Transformed,
                camera::FovProvenance::ModTransformResult, true };
        }
        return baseline;
    }
}

int main()
{
    bool pass = true;
    const float authored = 90.0f;
    const auto transformedBaseline = Baseline(112.623f, 3.55556f, true);
    const auto passThroughBaseline = Baseline(104.0f, 2.4f, false);

    const auto nativeSelection = cinematics::SelectCinematicBaseline(
        false, authored, transformedBaseline);
    pass &= Check(nativeSelection.targetBaselineFov == authored &&
        !nativeSelection.usedGameplayBaseline &&
        nativeSelection.source == cinematics::CinematicBaselineSource::AuthoredEnter,
        "native_ignores_baseline");

    const auto invalidSelection = cinematics::SelectCinematicBaseline(
        true, authored, {});
    pass &= Check(invalidSelection.targetBaselineFov == authored &&
        !invalidSelection.usedGameplayBaseline,
        "invalid_baseline_fallback");

    const auto gameplaySelection = cinematics::SelectCinematicBaseline(
        true, authored, transformedBaseline);
    pass &= Check(gameplaySelection.targetBaselineFov == 112.623f &&
        gameplaySelection.usedGameplayBaseline &&
        gameplaySelection.source == cinematics::CinematicBaselineSource::GameplayBaselineNative,
        "transformed_baseline_native_target");

    const auto passThroughSelection = cinematics::SelectCinematicBaseline(
        true, authored, passThroughBaseline);
    pass &= Check(passThroughSelection.targetBaselineFov == 104.0f &&
        passThroughSelection.usedGameplayBaseline,
        "passthrough_baseline_native_target");

    auto malformed = transformedBaseline;
    malformed.nativeFov.value = std::numeric_limits<float>::quiet_NaN();
    const auto malformedSelection = cinematics::SelectCinematicBaseline(
        true, authored, malformed);
    pass &= Check(malformedSelection.targetBaselineFov == authored &&
        !malformedSelection.usedGameplayBaseline,
        "malformed_native_fallback");

    const auto aspectRecalculationSelection = cinematics::SelectCinematicBaseline(
        true, authored, Baseline(96.0f, 1.77778f, false));
    pass &= Check(aspectRecalculationSelection.targetBaselineFov == 96.0f &&
        aspectRecalculationSelection.usedGameplayBaseline,
        "aspect_recalculation_native_target");

    float forced21x9 = authored;
    float forced16x9 = authored;
    float forced32x9 = authored;
    pass &= Check(cinematics::TryTransformCinematicFov(authored, authored,
        gameplaySelection.targetBaselineFov, 21.0f / 9.0f, 16.0f / 9.0f, forced21x9),
        "forced_21x9_transform");
    pass &= Check(cinematics::TryTransformCinematicFov(authored, authored,
        gameplaySelection.targetBaselineFov, 16.0f / 9.0f, 16.0f / 9.0f, forced16x9),
        "forced_16x9_transform");
    pass &= Check(cinematics::TryTransformCinematicFov(authored, authored,
        gameplaySelection.targetBaselineFov, 32.0f / 9.0f, 16.0f / 9.0f, forced32x9),
        "forced_32x9_transform");
    pass &= Check(std::isfinite(forced21x9) && std::isfinite(forced16x9) &&
        std::isfinite(forced32x9) && forced21x9 != forced16x9 &&
        forced32x9 != forced21x9,
        "cinematic_aspect_independence");
    pass &= Check(transformedBaseline.horPlusFov.value != forced21x9,
        "stored_gameplay_horplus_not_reused");

    float disabledOutput = authored;
    pass &= Check(!cinematics::TryTransformEnterFov(authored, 32.0f / 9.0f,
        false, 16.0f / 9.0f, disabledOutput) && disabledOutput == authored,
        "native_aspect_disabled_behavior");

    std::cout << "Cinematic FOV cutover harness: " << (pass ? "PASS" : "FAIL") << "\n";
    return pass ? 0 : 1;
}
