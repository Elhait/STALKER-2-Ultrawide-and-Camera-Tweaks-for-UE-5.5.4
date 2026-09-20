#pragma once

#include <cmath>
#include <limits>

namespace diagnostics::matchgameplay
{
    enum class SampleSpace
    {
        Native,
        CachedTransformedEnter,
        Unavailable,
    };

    struct PredictionInput
    {
        bool numericGuardMatched{};
        bool baselinePairValid{};
        float gameplayNative{std::numeric_limits<float>::quiet_NaN()};
        float gameplayHorPlus{std::numeric_limits<float>::quiet_NaN()};
        float gameplayAspect{std::numeric_limits<float>::quiet_NaN()};
        float enterObservation{std::numeric_limits<float>::quiet_NaN()};
        float sampleNative{std::numeric_limits<float>::quiet_NaN()};
        float cinematicAspect{std::numeric_limits<float>::quiet_NaN()};
    };

    struct PredictionResult
    {
        bool candidateAvailable{};
        SampleSpace sampleSpace{SampleSpace::Unavailable};
        float nativeMatched{std::numeric_limits<float>::quiet_NaN()};
        float horPlusMatched{std::numeric_limits<float>::quiet_NaN()};
    };

    inline PredictionResult Evaluate(const PredictionInput& input, float nativeAspect)
    {
        PredictionResult result{};
        if (input.numericGuardMatched) {
            result.sampleSpace = SampleSpace::CachedTransformedEnter;
            return result;
        }

        result.sampleSpace = SampleSpace::Native;
        bool available = input.baselinePairValid &&
            std::isfinite(input.gameplayNative) && std::isfinite(input.gameplayHorPlus) &&
            std::isfinite(input.gameplayAspect) && std::isfinite(input.enterObservation) &&
            std::isfinite(input.sampleNative) && std::isfinite(input.cinematicAspect) &&
            input.gameplayNative > 1.0f && input.gameplayNative < 179.0f &&
            input.enterObservation > 1.0f && input.enterObservation < 179.0f &&
            input.sampleNative > 1.0f && input.sampleNative < 179.0f;
        if (!available) return result;

        constexpr float radiansPerDegree = 0.01745329251994329577f;
        const float referenceProjection = std::tan(input.enterObservation * 0.5f * radiansPerDegree);
        const float sampleProjection = std::tan(input.sampleNative * 0.5f * radiansPerDegree);
        const float nativeBaselineProjection = std::tan(input.gameplayNative * 0.5f * radiansPerDegree);
        if (!std::isfinite(referenceProjection) || std::fabs(referenceProjection) <= 1.0e-6f) return result;

        const float matchedProjection = sampleProjection *
            (nativeBaselineProjection / referenceProjection);
        result.nativeMatched = 2.0f * std::atan(matchedProjection) / radiansPerDegree;
        if (!std::isfinite(result.nativeMatched) || result.nativeMatched <= 1.0f ||
            result.nativeMatched >= 179.0f) {
            result.nativeMatched = std::numeric_limits<float>::quiet_NaN();
            return result;
        }

        result.horPlusMatched = 2.0f * std::atan(
            std::tan(result.nativeMatched * 0.5f * radiansPerDegree) *
            (input.cinematicAspect / nativeAspect)) / radiansPerDegree;
        result.candidateAvailable = std::isfinite(result.horPlusMatched) &&
            result.horPlusMatched > 1.0f && result.horPlusMatched < 179.0f;
        if (!result.candidateAvailable) {
            result.nativeMatched = std::numeric_limits<float>::quiet_NaN();
            result.horPlusMatched = std::numeric_limits<float>::quiet_NaN();
        }
        return result;
    }
}
