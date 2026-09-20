#include "cinematic_fov.hpp"
#include "../camera/horplus.hpp"

#include <cmath>

namespace cinematics
{
    CinematicBaselineSelection SelectCinematicBaseline(bool gameplayHorPlusRequested,
        float authoredEnterFov, const camera::GameplayBaseline& gameplayBaseline)
    {
        if (gameplayHorPlusRequested && camera::IsUsableGameplayBaseline(gameplayBaseline)) {
            return { gameplayBaseline.nativeFov.value,
                CinematicBaselineSource::GameplayBaselineNative, true };
        }
        return { authoredEnterFov, CinematicBaselineSource::AuthoredEnter, false };
    }

    bool TryTransformCinematicFov(float cinematicFov, float cinematicReferenceFov,
        float targetBaselineFov, float aspect, float nativeAspect,
        float& transformedFov)
    {
        transformedFov = cinematicFov;
        if (!std::isfinite(cinematicFov) || !std::isfinite(cinematicReferenceFov) ||
            !std::isfinite(targetBaselineFov) || !std::isfinite(aspect) ||
            !std::isfinite(nativeAspect) || cinematicFov <= 1.0f || cinematicFov >= 179.0f ||
            cinematicReferenceFov <= 1.0f || cinematicReferenceFov >= 179.0f ||
            targetBaselineFov <= 1.0f || targetBaselineFov >= 179.0f || aspect <= 1.0f ||
            nativeAspect <= 1.0f)
            return false;

        constexpr float pi = 3.14159265358979323846f;
        const float radiansPerDegree = pi / 180.0f;
        const float referenceProjection =
            std::tan(cinematicReferenceFov * 0.5f * radiansPerDegree);
        if (!std::isfinite(referenceProjection) || std::fabs(referenceProjection) <= 1.0e-6f)
            return false;

        const float matchedProjection = std::tan(cinematicFov * 0.5f * radiansPerDegree) *
            (std::tan(targetBaselineFov * 0.5f * radiansPerDegree) / referenceProjection);
        if (!std::isfinite(matchedProjection)) return false;

        const float matchedNative = 2.0f * std::atan(matchedProjection) / radiansPerDegree;
        if (!std::isfinite(matchedNative) || matchedNative <= 1.0f || matchedNative >= 179.0f)
            return false;

        transformedFov = camera::HorPlus(matchedNative, aspect, nativeAspect);
        return std::isfinite(transformedFov) && transformedFov > 1.0f && transformedFov < 179.0f;
    }

    bool TryTransformEnterFov(float authoredFov, float aspect, bool enabled,
        float nativeAspect, float& transformedFov)
    {
        transformedFov = authoredFov;
        if (!enabled)
            return false;
        return TryTransformCinematicFov(authoredFov, authoredFov, authoredFov,
            aspect, nativeAspect, transformedFov);
    }

    bool TryTransformMatchGameplay(float cinematicFov, float gameplayBaselineFov,
        float cinematicReferenceFov, float aspect, float nativeAspect,
        float& transformedFov)
    {
        return TryTransformCinematicFov(cinematicFov, cinematicReferenceFov,
            gameplayBaselineFov, aspect, nativeAspect, transformedFov);
    }
}
