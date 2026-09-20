#include "presentation_state.hpp"

namespace camera
{
    const char* CoordinatorStateName(CoordinatorState state)
    {
        switch (state) {
        case CoordinatorState::Gameplay: return "Gameplay";
        case CoordinatorState::CinematicActive: return "CinematicActive";
        case CoordinatorState::CinematicExiting: return "CinematicExiting";
        }
        return "Unknown";
    }
}
