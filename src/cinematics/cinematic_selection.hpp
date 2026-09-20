#pragma once

#include "../config/feature_config.hpp"

#include <cstdint>

namespace cinematics
{
    struct CinematicSelectionSnapshot
    {
        config::CinematicAspectPolicy aspectPolicy{};
        config::CinematicFovMode fovMode{};
        std::uint64_t generation{};
        bool valid{};
    };

    CinematicSelectionSnapshot CaptureCinematicSelection(
        config::CinematicAspectPolicy aspectPolicy,
        config::CinematicFovMode fovMode,
        std::uint64_t generation) noexcept;

    bool TryCaptureCinematicSelection(
        bool activeSelectionValid,
        config::CinematicAspectPolicy aspectPolicy,
        config::CinematicFovMode fovMode,
        std::uint64_t generation,
        CinematicSelectionSnapshot& snapshot) noexcept;
}
