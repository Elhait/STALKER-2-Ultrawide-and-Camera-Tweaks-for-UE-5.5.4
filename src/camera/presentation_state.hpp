#pragma once

#include <cstdint>

namespace camera
{
    enum class CoordinatorState : std::uint32_t
    {
        Gameplay,
        CinematicActive,
        CinematicExiting
    };

    const char* CoordinatorStateName(CoordinatorState state);
}
