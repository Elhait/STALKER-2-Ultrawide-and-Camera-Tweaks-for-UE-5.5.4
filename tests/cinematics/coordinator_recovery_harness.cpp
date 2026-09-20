#include "../../src/gameplay/gameplay_state.hpp"

#include <iostream>

namespace
{
    bool Check(bool condition, const char* name)
    {
        if (!condition) std::cerr << name << ": FAIL\n";
        return condition;
    }

    bool CheckGameplayAvailable()
    {
        const auto transition = gameplay::ResolveCinematicExitTransition(true);
        return Check(transition.nextState == camera::CoordinatorState::CinematicExiting,
                "available_state") &&
            Check(transition.armGameplayHandoff, "available_handoff");
    }

    bool CheckGameplayDisabled()
    {
        const auto transition = gameplay::ResolveCinematicExitTransition(false);
        return Check(transition.nextState == camera::CoordinatorState::Gameplay,
                "disabled_state") &&
            Check(!transition.armGameplayHandoff, "disabled_handoff");
    }

    bool CheckGameplayInitializationFailure()
    {
        const auto transition = gameplay::ResolveCinematicExitTransition(false);
        return Check(transition.nextState == camera::CoordinatorState::Gameplay,
                "failed_state") &&
            Check(!transition.armGameplayHandoff, "failed_handoff");
    }

    bool CheckHorPlusMode()
    {
        const auto transition = gameplay::ResolveCinematicExitTransition(true,
            config::GameplayMode::HorPlus);
        return Check(transition.nextState == camera::CoordinatorState::Gameplay,
                "horplus_state") &&
            Check(!transition.armGameplayHandoff, "horplus_handoff");
    }
}

int main()
{
    const bool available = CheckGameplayAvailable();
    const bool disabled = CheckGameplayDisabled();
    const bool failed = CheckGameplayInitializationFailure();
    const bool horPlus = CheckHorPlusMode();
    std::cout << "available=" << (available ? "PASS" : "FAIL")
        << " disabled=" << (disabled ? "PASS" : "FAIL")
        << " failed=" << (failed ? "PASS" : "FAIL")
        << " horplus=" << (horPlus ? "PASS" : "FAIL") << "\n";
    return available && disabled && failed && horPlus ? 0 : 1;
}
