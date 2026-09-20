#include "gameplay_state.hpp"

namespace gameplay
{
    const char* ReplayStateName(ReplayState state)
    {
        switch (state) {
        case ReplayState::WaitingForAutomaticUpdate: return "WaitingForAutomaticUpdate";
        case ReplayState::AppliedConstrainPass: return "AppliedConstrainPass";
        case ReplayState::Complete: return "Complete";
        }
        return "Unknown";
    }

    CinematicExitTransition ResolveCinematicExitTransition(bool gameplayAvailable)
    {
        if (gameplayAvailable)
            return { camera::CoordinatorState::CinematicExiting, true };
        return { camera::CoordinatorState::Gameplay, false };
    }

    CinematicExitTransition ResolveCinematicExitTransition(
        bool gameplayAvailable, config::GameplayMode gameplayMode)
    {
        if (gameplayMode == config::GameplayMode::HorPlus)
            return { camera::CoordinatorState::Gameplay, false };
        return ResolveCinematicExitTransition(gameplayAvailable);
    }

    GameplayModeTransitionPlan ResolveGameplayModeTransition(
        config::GameplayMode oldMode, config::GameplayMode newMode,
        camera::CoordinatorState coordinator)
    {
        if (oldMode == newMode) return {};
        return {
            true,
            true,
            true,
            coordinator != camera::CoordinatorState::Gameplay
        };
    }
}
