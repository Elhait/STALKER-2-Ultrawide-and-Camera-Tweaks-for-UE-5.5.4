#pragma once

#include "../camera/gameplay_baseline.hpp"

namespace cinematics
{
    enum class CinematicBaselineSource
    {
        AuthoredEnter,
        GameplayBaselineNative,
    };

    struct CinematicBaselineSelection
    {
        float targetBaselineFov{};
        CinematicBaselineSource source{CinematicBaselineSource::AuthoredEnter};
        bool usedGameplayBaseline{};
    };

    CinematicBaselineSelection SelectCinematicBaseline(bool gameplayHorPlusRequested,
        float authoredEnterFov, const camera::GameplayBaseline& gameplayBaseline);

    bool TryTransformCinematicFov(float cinematicFov, float cinematicReferenceFov,
        float targetBaselineFov, float aspect, float nativeAspect,
        float& transformedFov);

    bool TryTransformMatchGameplay(float cinematicFov, float gameplayBaselineFov,
        float cinematicReferenceFov, float aspect, float nativeAspect,
        float& transformedFov);

    bool TryTransformEnterFov(float authoredFov, float aspect, bool enabled,
        float nativeAspect, float& transformedFov);
}
