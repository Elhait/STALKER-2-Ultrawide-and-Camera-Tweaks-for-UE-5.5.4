#include "../../src/gameplay/gameplay_state.hpp"

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
    using config::GameplayMode;
    using camera::CoordinatorState;
    bool pass = true;

    const auto startup = gameplay::ResolveGameplayModeTransition(
        GameplayMode::HorPlus, GameplayMode::HorPlus, CoordinatorState::Gameplay);
    pass &= Check(!startup.changed, "startup_same_mode_is_noop");
    pass &= Check(!startup.resetAspectRecalculation && !startup.invalidateHorPlusState,
        "startup_has_no_previous_mode_cleanup");

    const auto aspectToHorPlus = gameplay::ResolveGameplayModeTransition(
        GameplayMode::AspectRecalculation, GameplayMode::HorPlus,
        CoordinatorState::Gameplay);
    pass &= Check(aspectToHorPlus.changed, "aspect_to_horplus_changes");
    pass &= Check(aspectToHorPlus.resetAspectRecalculation,
        "aspect_to_horplus_resets_replay_contract");
    pass &= Check(aspectToHorPlus.invalidateHorPlusState,
        "aspect_to_horplus_invalidates_transition_state");
    pass &= Check(!aspectToHorPlus.deferPhysicalTransition,
        "gameplay_boundary_applies_immediately");

    const auto aspectToHorPlusCinematic = gameplay::ResolveGameplayModeTransition(
        GameplayMode::AspectRecalculation, GameplayMode::HorPlus,
        CoordinatorState::CinematicActive);
    pass &= Check(aspectToHorPlusCinematic.deferPhysicalTransition,
        "cinematic_transition_is_deferred");

    const auto horPlusToAspect = gameplay::ResolveGameplayModeTransition(
        GameplayMode::HorPlus, GameplayMode::AspectRecalculation,
        CoordinatorState::Gameplay);
    pass &= Check(horPlusToAspect.changed && horPlusToAspect.resetAspectRecalculation,
        "horplus_to_aspect_resets_replay_contract");
    pass &= Check(horPlusToAspect.invalidateHorPlusState,
        "horplus_to_aspect_invalidates_horplus_state");

    const auto noOp = gameplay::ResolveGameplayModeTransition(
        GameplayMode::AspectRecalculation, GameplayMode::AspectRecalculation,
        CoordinatorState::CinematicExiting);
    pass &= Check(!noOp.changed && !noOp.deferPhysicalTransition,
        "aspect_same_mode_is_noop");

    std::cout << "gameplay_mode_transition=" << (pass ? "PASS" : "FAIL") << "\n";
    return pass ? 0 : 1;
}
