#pragma once

#include <cstdint>

#include "../config/feature_config.hpp"
#include "../camera/presentation_state.hpp"

namespace gameplay
{
    enum class ReplayState : std::uint32_t
    {
        WaitingForAutomaticUpdate,
        AppliedConstrainPass,
        Complete
    };

    struct CinematicExitTransition
    {
        camera::CoordinatorState nextState;
        bool armGameplayHandoff;
    };

    const char* ReplayStateName(ReplayState state);
    CinematicExitTransition ResolveCinematicExitTransition(bool gameplayAvailable);
    CinematicExitTransition ResolveCinematicExitTransition(
        bool gameplayAvailable, config::GameplayMode gameplayMode);

    struct GameplayModeTransitionPlan
    {
        bool changed{};
        bool resetAspectRecalculation{};
        bool invalidateHorPlusState{};
        bool deferPhysicalTransition{};
    };

    GameplayModeTransitionPlan ResolveGameplayModeTransition(
        config::GameplayMode oldMode, config::GameplayMode newMode,
        camera::CoordinatorState coordinator);
}
