#include "cinematic_selection.hpp"

namespace cinematics
{
    CinematicSelectionSnapshot CaptureCinematicSelection(
        config::CinematicAspectPolicy aspectPolicy,
        config::CinematicFovMode fovMode,
        std::uint64_t generation) noexcept
    {
        return { aspectPolicy, fovMode, generation, true };
    }

    bool TryCaptureCinematicSelection(
        bool activeSelectionValid,
        config::CinematicAspectPolicy aspectPolicy,
        config::CinematicFovMode fovMode,
        std::uint64_t generation,
        CinematicSelectionSnapshot& snapshot) noexcept
    {
        if (activeSelectionValid) return false;
        snapshot = CaptureCinematicSelection(aspectPolicy, fovMode, generation);
        return true;
    }
}
