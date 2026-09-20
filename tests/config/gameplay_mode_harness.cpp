#include "../../src/config/feature_config.hpp"

#include <iostream>
#include <string>

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
    config::GameplayMode mode{};
    const bool aspect = config::ParseGameplayMode("AspectRecalculation", mode) &&
        mode == config::GameplayMode::AspectRecalculation;
    const bool horPlus = config::ParseGameplayMode("hor+", mode) &&
        mode == config::GameplayMode::HorPlus;
    mode = config::GameplayMode::AspectRecalculation;
    const bool unknown = !config::ParseGameplayMode("unsupported", mode) &&
        mode == config::GameplayMode::AspectRecalculation;
    const bool names = std::string(config::GameplayModeName(config::GameplayMode::AspectRecalculation)) ==
            "AspectRecalculation" &&
        std::string(config::GameplayModeName(config::GameplayMode::HorPlus)) == "HorPlus";
    const bool cycle = config::NextGameplayMode(config::GameplayMode::AspectRecalculation) == config::GameplayMode::HorPlus &&
        config::NextGameplayMode(config::GameplayMode::HorPlus) == config::GameplayMode::AspectRecalculation;
    const bool result = Check(aspect, "aspect_mode") && Check(horPlus, "horplus_mode") &&
        Check(unknown, "unknown_mode") && Check(names, "mode_names") && Check(cycle, "mode_cycle");
    std::cout << "Gameplay mode harness: " << (result ? "PASS" : "FAIL") << "\n";
    return result ? 0 : 1;
}
