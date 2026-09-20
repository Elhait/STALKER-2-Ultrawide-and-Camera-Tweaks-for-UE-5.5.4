#include "../../src/camera/gameplay_baseline.hpp"

#include <cmath>
#include <iostream>

namespace
{
    struct LegacyRetainedSubset
    {
        float nativeFov{std::numeric_limits<float>::quiet_NaN()};
        float aspect{std::numeric_limits<float>::quiet_NaN()};
        std::uintptr_t source{};
        bool aspectValid{};
    };

    bool Check(bool condition, const char* name)
    {
        if (!condition) std::cerr << name << ": FAIL\n";
        return condition;
    }

    camera::CameraFovObservation NativeHorPlus(
        float nativeFov, float horPlusFov, float aspect,
        std::uintptr_t source, std::uint64_t sequence)
    {
        camera::CameraFovObservation observation{};
        observation.boundary = camera::FovObservationBoundary::CameraWriter;
        observation.inputFov = { nativeFov, camera::FovSpace::Native,
            camera::FovProvenance::NativeRegisterInput, true };
        observation.resultFov = { horPlusFov, camera::FovSpace::Transformed,
            camera::FovProvenance::ModTransformResult, true };
        observation.aspect = { aspect, camera::FovProvenance::ResolvedAspect, true };
        observation.writerSource = { source, true };
        observation.publicationSequence = sequence;
        return observation;
    }

    camera::CameraFovObservation NativePassThrough(
        float nativeFov, float aspect, std::uintptr_t source, std::uint64_t sequence)
    {
        auto observation = NativeHorPlus(nativeFov, nativeFov, aspect, source, sequence);
        observation.resultFov = { nativeFov, camera::FovSpace::Native,
            camera::FovProvenance::PassThroughResult, true };
        return observation;
    }
}

int main()
{
    using namespace camera;
    bool pass = true;
    GameplayBaselineStore store;
    LegacyRetainedSubset legacy{};

    const auto startup = store.Read();
    pass &= Check(!startup.valid && !startup.nativeFov.valid &&
        std::isnan(startup.nativeFov.value), "startup_invalid");

    const auto first = store.Project(NativeHorPlus(112.623f, 143.132f, 3.55556f,
        0x1000, 7), true);
    legacy.nativeFov = 112.623f;
    legacy.aspect = 3.55556f;
    legacy.source = 0x1000;
    legacy.aspectValid = true;
    pass &= Check(first.disposition == GameplayBaselineProjectionDisposition::Updated,
        "valid_update");
    pass &= Check(first.baseline.valid && first.baseline.nativeFov.valid &&
        first.baseline.nativeFov.value == 112.623f && first.baseline.horPlusFov.valid &&
        first.baseline.horPlusFov.value == 143.132f && first.baseline.aspect == 3.55556f &&
        first.baseline.source.value == 0x1000 && first.baseline.observationSequence == 7,
        "coherent_first_pair");
    pass &= Check(first.baseline.nativeFov.value == legacy.nativeFov &&
        first.baseline.aspect == legacy.aspect && first.baseline.source.value == legacy.source,
        "legacy_subset_equivalence");

    const auto second = store.Project(NativeHorPlus(101.25f, 129.0f, 2.4f, 0x2000, 8), true);
    pass &= Check(second.disposition == GameplayBaselineProjectionDisposition::Updated &&
        second.baseline.nativeFov.value == 101.25f && second.baseline.horPlusFov.valid &&
        second.baseline.horPlusFov.value == 129.0f &&
        second.baseline.aspect == 2.4f && second.baseline.source.value == 0x2000 &&
        second.baseline.observationSequence == 8, "consecutive_update");

    const auto ads = store.Project(NativeHorPlus(73.5f, 101.0f, 2.4f, 0x2000, 9), true);
    legacy.nativeFov = 73.5f;
    legacy.aspect = 2.4f;
    legacy.source = 0x2000;
    pass &= Check(ads.disposition == GameplayBaselineProjectionDisposition::Updated &&
        ads.baseline.nativeFov.valid && ads.baseline.nativeFov.value == 73.5f &&
        ads.baseline.horPlusFov.valid && ads.baseline.horPlusFov.value == 101.0f,
        "transient_gameplay_matches_legacy_retention");

    const auto passThrough = store.Project(NativePassThrough(80.0f, 2.4f, 0x2000, 10), true);
    legacy.nativeFov = 80.0f;
    legacy.aspect = 2.4f;
    legacy.source = 0x2000;
    pass &= Check(passThrough.disposition == GameplayBaselineProjectionDisposition::Updated &&
        passThrough.baseline.valid && passThrough.baseline.nativeFov.valid &&
        passThrough.baseline.nativeFov.value == 80.0f &&
        !passThrough.baseline.horPlusFov.valid,
        "valid_passthrough_updates_native_only");
    pass &= Check(passThrough.baseline.nativeFov.value == legacy.nativeFov &&
        passThrough.baseline.aspect == legacy.aspect &&
        passThrough.baseline.source.value == legacy.source,
        "passthrough_legacy_subset_equivalence");

    auto invalid = NativePassThrough(75.0f, 2.4f, 0x2000, 11);
    invalid.inputFov.valid = false;
    const auto retainedInvalid = store.Project(invalid, true);
    pass &= Check(retainedInvalid.disposition == GameplayBaselineProjectionDisposition::Retained &&
        retainedInvalid.baseline.nativeFov.value == 80.0f,
        "invalid_retains");
    pass &= Check(retainedInvalid.baseline.nativeFov.value == legacy.nativeFov &&
        retainedInvalid.baseline.aspect == legacy.aspect &&
        retainedInvalid.baseline.source.value == legacy.source,
        "invalid_preserves_legacy_subset");

    const auto cinematic = store.Project(NativeHorPlus(90.0f, 126.87f, 3.55556f,
        0x3000, 11), false);
    pass &= Check(cinematic.disposition == GameplayBaselineProjectionDisposition::Retained &&
        cinematic.baseline.nativeFov.value == 80.0f, "cinematic_retains");

    CameraFovObservation cachedObservation{};
    cachedObservation.boundary = FovObservationBoundary::CameraWriter;
    cachedObservation.inputFov = { 126.87f, FovSpace::Transformed,
        FovProvenance::CachedCinematicEnter, true };
    cachedObservation.resultFov = { 126.87f, FovSpace::Transformed,
        FovProvenance::PassThroughResult, true };
    cachedObservation.aspect = { 3.55556f, FovProvenance::ResolvedAspect, true };
    cachedObservation.writerSource = { 0x3000, true };
    cachedObservation.publicationSequence = 12;
    const auto cachedResult = store.Project(cachedObservation, false);
    pass &= Check(cachedResult.disposition == GameplayBaselineProjectionDisposition::Retained &&
        cachedResult.baseline.nativeFov.value == 80.0f, "cached_transformed_retains");

    const auto noEligibility = store.Project(NativeHorPlus(84.0f, 118.0f, 2.4f,
        0x4000, 13), false);
    pass &= Check(noEligibility.disposition == GameplayBaselineProjectionDisposition::Retained &&
        noEligibility.baseline.nativeFov.value == 80.0f, "semantic_eligibility_is_explicit");

    GameplayBaselineStore passThroughStartupStore;
    const auto passThroughStartup = passThroughStartupStore.Project(
        NativePassThrough(88.0f, 2.4f, 0x4100, 14), true);
    pass &= Check(passThroughStartup.disposition == GameplayBaselineProjectionDisposition::Updated &&
        passThroughStartup.baseline.valid && passThroughStartup.baseline.nativeFov.value == 88.0f &&
        !passThroughStartup.baseline.horPlusFov.valid, "passthrough_only_startup");
    GameplayBaselineStore aspectRecalculationStore;
    const auto aspectRecalculation = aspectRecalculationStore.Project(
        NativePassThrough(96.0f, 1.77778f, 0x4200, 14), true);
    pass &= Check(aspectRecalculation.disposition == GameplayBaselineProjectionDisposition::Updated &&
        aspectRecalculation.baseline.valid && aspectRecalculation.baseline.nativeFov.value == 96.0f &&
        !aspectRecalculation.baseline.horPlusFov.valid,
        "aspect_recalculation_native_update");
    const auto transformedAfterPassThrough = passThroughStartupStore.Project(
        NativeHorPlus(91.0f, 130.0f, 2.4f, 0x4100, 15), true);
    pass &= Check(transformedAfterPassThrough.disposition ==
        GameplayBaselineProjectionDisposition::Updated &&
        transformedAfterPassThrough.baseline.nativeFov.value == 91.0f &&
        transformedAfterPassThrough.baseline.horPlusFov.valid &&
        transformedAfterPassThrough.baseline.horPlusFov.value == 130.0f,
        "transformed_after_passthrough");

    auto malformed = NativeHorPlus(84.0f, 118.0f, 2.4f, 0x4000, 14);
    malformed.writerSource.valid = false;
    const auto malformedResult = store.Project(malformed, true);
    pass &= Check(malformedResult.disposition == GameplayBaselineProjectionDisposition::Retained &&
        malformedResult.baseline.nativeFov.value == 80.0f, "malformed_retains");

    const auto invalidated = store.Invalidate();
    legacy = {};
    legacy.aspectValid = false;
    pass &= Check(invalidated.disposition == GameplayBaselineProjectionDisposition::Invalidated &&
        !invalidated.baseline.valid && !invalidated.baseline.nativeFov.valid &&
        std::isnan(invalidated.baseline.nativeFov.value),
        "explicit_invalidate");
    pass &= Check(!legacy.aspectValid && std::isnan(legacy.nativeFov),
        "legacy_transition_invalidation_model");

    const auto afterInvalidate = store.Project(NativeHorPlus(90.0f, 126.87f, 3.55556f,
        0x5000, 15), true);
    pass &= Check(afterInvalidate.disposition == GameplayBaselineProjectionDisposition::Updated &&
        afterInvalidate.baseline.observationSequence == 15, "update_after_invalidate");

    std::cout << "Gameplay baseline harness: " << (pass ? "PASS" : "FAIL") << "\n";
    return pass ? 0 : 1;
}
