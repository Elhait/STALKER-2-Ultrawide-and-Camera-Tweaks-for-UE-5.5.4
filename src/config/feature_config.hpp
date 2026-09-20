#pragma once

#include <Windows.h>

#include <cstdint>
#include <string>

namespace config
{
    enum class GameplayMode : std::uint32_t
    {
        AspectRecalculation,
        HorPlus,
    };

    enum class CinematicAspectPolicy : std::uint32_t
    {
        Auto,
        Native,
        Forced16x9,
        Forced21x9,
        Forced32x9,
    };

    enum class CinematicFovMode : std::uint32_t
    {
        NativeHorPlus,
        GameplayHorPlus,
    };

    enum class DialogueZoomPolicy : std::uint32_t
    {
        Native,
        Adaptive,
        Reduced,
        Disabled,
    };

    struct FeatureConfig
    {
        bool gameplayEnabled{true};
        GameplayMode gameplayMode{GameplayMode::HorPlus};
        CinematicAspectPolicy cinematicAspectPolicy{CinematicAspectPolicy::Auto};
        bool cinematicAspectPolicyExplicit{};
        CinematicFovMode cinematicFovMode{CinematicFovMode::GameplayHorPlus};
        DialogueZoomPolicy dialogueZoomPolicy{DialogueZoomPolicy::Adaptive};
        bool diagnosticsEnabled{};
        bool hotkeysEnabled{false};
        int gameplayCycleKey{VK_F9};
        int cinematicCycleKey{VK_F10};
        int cinematicFovCycleKey{VK_F11};
        int dialogueCycleKey{VK_F12};
    };

    std::string Trim(std::string value);
    bool ParseBool(std::string value, bool& result);
    bool ParseHotkey(std::string value, int& result);
    const char* HotkeyName(int key);
    bool ParseGameplayMode(std::string value, GameplayMode& result);
    const char* GameplayModeName(GameplayMode mode);
    bool ParseCinematicAspectPolicy(std::string value, CinematicAspectPolicy& result);
    const char* CinematicAspectPolicyName(CinematicAspectPolicy policy);
    bool ParseCinematicFovMode(std::string value, CinematicFovMode& result);
    const char* CinematicFovModeName(CinematicFovMode mode);
    bool ParseDialogueZoomPolicy(std::string value, DialogueZoomPolicy& result);
    const char* DialogueZoomPolicyName(DialogueZoomPolicy policy);
    DialogueZoomPolicy NextDialogueZoomPolicy(DialogueZoomPolicy policy);
    CinematicAspectPolicy NextCinematicAspectPolicy(CinematicAspectPolicy policy);
    CinematicFovMode NextCinematicFovMode(CinematicFovMode mode);
    GameplayMode NextGameplayMode(GameplayMode mode);
}
