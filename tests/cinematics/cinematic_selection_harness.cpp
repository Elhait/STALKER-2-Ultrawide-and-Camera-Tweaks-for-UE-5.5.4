#include "../../src/cinematics/cinematic_selection.hpp"

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
    using config::CinematicAspectPolicy;
    using config::CinematicFovMode;
    bool pass = true;

    const auto active = cinematics::CaptureCinematicSelection(
        CinematicAspectPolicy::Auto, CinematicFovMode::NativeHorPlus, 1);
    const auto next = cinematics::CaptureCinematicSelection(
        CinematicAspectPolicy::Forced21x9, CinematicFovMode::GameplayHorPlus, 2);

    pass &= Check(active.valid, "active_snapshot_valid");
    pass &= Check(active.aspectPolicy == CinematicAspectPolicy::Auto &&
        active.fovMode == CinematicFovMode::NativeHorPlus && active.generation == 1,
        "active_snapshot_is_coherent");
    pass &= Check(next.aspectPolicy == CinematicAspectPolicy::Forced21x9 &&
        next.fovMode == CinematicFovMode::GameplayHorPlus && next.generation == 2,
        "next_snapshot_captures_latest_selection");
    pass &= Check(active.aspectPolicy == CinematicAspectPolicy::Auto &&
        active.fovMode == CinematicFovMode::NativeHorPlus,
        "active_snapshot_remains_immutable");

    cinematics::CinematicSelectionSnapshot accepted{};
    pass &= Check(cinematics::TryCaptureCinematicSelection(
        false, CinematicAspectPolicy::Auto, CinematicFovMode::NativeHorPlus, 3, accepted),
        "first_enter_is_accepted");
    pass &= Check(!cinematics::TryCaptureCinematicSelection(
        true, CinematicAspectPolicy::Forced32x9, CinematicFovMode::GameplayHorPlus, 4, accepted) &&
        accepted.generation == 3 && accepted.aspectPolicy == CinematicAspectPolicy::Auto,
        "duplicate_enter_does_not_overwrite_active_selection");

    std::cout << "cinematic_selection=" << (pass ? "PASS" : "FAIL") << "\n";
    return pass ? 0 : 1;
}
