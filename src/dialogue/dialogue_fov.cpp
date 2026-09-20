#include "dialogue_fov.hpp"

#include <algorithm>
#include <cmath>

namespace dialogue
{
    float TransformProjectionSample(float incoming, float baseline, float target)
    {
        constexpr float nativeDialogueTarget = 70.0f;
        constexpr float projectionIntervalTolerance = 0.25f;
        constexpr float radiansPerDegree = 0.01745329251994329577f;
        const float intervalLow = baseline < nativeDialogueTarget ? baseline : nativeDialogueTarget;
        const float intervalHigh = baseline > nativeDialogueTarget ? baseline : nativeDialogueTarget;
        if (incoming < intervalLow - projectionIntervalTolerance ||
            incoming > intervalHigh + projectionIntervalTolerance) return incoming;
        const float baselineProjection = std::tan(baseline * 0.5f * radiansPerDegree);
        const float nativeTargetProjection = std::tan(nativeDialogueTarget * 0.5f * radiansPerDegree);
        const float incomingProjection = std::tan(incoming * 0.5f * radiansPerDegree);
        const float targetProjection = std::tan(target * 0.5f * radiansPerDegree);
        const float nativeSpan = nativeTargetProjection - baselineProjection;
        if (!std::isfinite(nativeSpan) || std::fabs(nativeSpan) <= 1.0e-6f) return incoming;
        const float progress = std::clamp(
            (incomingProjection - baselineProjection) / nativeSpan, 0.0f, 1.0f);
        const float outputProjection = baselineProjection +
            progress * (targetProjection - baselineProjection);
        const float output = 2.0f * std::atan(outputProjection) / radiansPerDegree;
        return std::isfinite(output) ? output : incoming;
    }

    float TransformExitSample(float incoming, float baseline, float target, float exitStart)
    {
        constexpr float radiansPerDegree = 0.01745329251994329577f;
        const float startProjection = std::tan(exitStart * 0.5f * radiansPerDegree);
        const float baselineProjection = std::tan(baseline * 0.5f * radiansPerDegree);
        const float incomingProjection = std::tan(incoming * 0.5f * radiansPerDegree);
        const float targetProjection = std::tan(target * 0.5f * radiansPerDegree);
        const float nativeSpan = baselineProjection - startProjection;
        if (!std::isfinite(nativeSpan) || std::fabs(nativeSpan) <= 1.0e-6f) return incoming;
        const float progress = std::clamp(
            (incomingProjection - startProjection) / nativeSpan, 0.0f, 1.0f);
        const float outputProjection = targetProjection +
            progress * (baselineProjection - targetProjection);
        const float output = 2.0f * std::atan(outputProjection) / radiansPerDegree;
        return std::isfinite(output) ? output : incoming;
    }

    float AdaptiveTarget(float baseline)
    {
        constexpr float nativeDialogueTarget = 70.0f;
        constexpr float nativeReferenceGameplay = 90.0f;
        constexpr float radiansPerDegree = 0.01745329251994329577f;
        const float nativeZoom =
            std::tan(nativeReferenceGameplay * 0.5f * radiansPerDegree) /
            std::tan(nativeDialogueTarget * 0.5f * radiansPerDegree);
        const float projection = std::tan(baseline * 0.5f * radiansPerDegree) / nativeZoom;
        const float output = 2.0f * std::atan(projection) / radiansPerDegree;
        return std::isfinite(output) ? output : baseline;
    }

    float ReducedTarget(float baseline)
    {
        constexpr float nativeDialogueTarget = 70.0f;
        constexpr float nativeReferenceGameplay = 90.0f;
        constexpr float radiansPerDegree = 0.01745329251994329577f;
        const float nativeZoom =
            std::tan(nativeReferenceGameplay * 0.5f * radiansPerDegree) /
            std::tan(nativeDialogueTarget * 0.5f * radiansPerDegree);
        const float projection = std::tan(baseline * 0.5f * radiansPerDegree) /
            std::sqrt(nativeZoom);
        const float output = 2.0f * std::atan(projection) / radiansPerDegree;
        return std::isfinite(output) ? output : baseline;
    }
}
