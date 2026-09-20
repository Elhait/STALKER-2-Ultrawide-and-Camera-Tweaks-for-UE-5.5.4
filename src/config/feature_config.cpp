#include "feature_config.hpp"

#include <algorithm>
#include <cctype>
#include <utility>

namespace config
{
    std::string Trim(std::string value)
    {
        const auto notSpace = [](unsigned char character) { return !std::isspace(character); };
        value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
        value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
        return value;
    }

    bool ParseBool(std::string value, bool& result)
    {
        value = Trim(std::move(value));
        std::transform(value.begin(), value.end(), value.begin(),
            [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
        if (value == "1" || value == "true" || value == "yes" || value == "on") {
            result = true;
            return true;
        }
        if (value == "0" || value == "false" || value == "no" || value == "off") {
            result = false;
            return true;
        }
        return false;
    }

    bool ParseHotkey(std::string value, int& result)
    {
        value = Trim(std::move(value));
        if (value.size() == 1) {
            const char key = static_cast<char>(std::toupper(static_cast<unsigned char>(value.front())));
            if (key >= 'A' && key <= 'Z') { result = key; return true; }
            if (key >= '0' && key <= '9') { result = key; return true; }
            return false;
        }
        if (value.size() < 2 || (value.front() != 'F' && value.front() != 'f')) return false;
        int number = 0;
        for (std::size_t index = 1; index < value.size(); ++index) {
            if (value[index] < '0' || value[index] > '9') return false;
            number = number * 10 + (value[index] - '0');
        }
        if (number < 1 || number > 12) return false;
        result = VK_F1 + number - 1;
        return true;
    }

    const char* HotkeyName(int key)
    {
        static const char* names[] = {
            "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8",
            "F9", "F10", "F11", "F12"
        };
        if (key >= VK_F1 && key <= VK_F12) return names[key - VK_F1];
        static thread_local char singleKey[2];
        if ((key >= '0' && key <= '9') || (key >= 'A' && key <= 'Z')) {
            singleKey[0] = static_cast<char>(key);
            singleKey[1] = '\0';
            return singleKey;
        }
        return "unknown";
    }

    bool ParseGameplayMode(std::string value, GameplayMode& result)
    {
        value = Trim(std::move(value));
        std::transform(value.begin(), value.end(), value.begin(),
            [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
        if (value == "aspectrecalculation" || value == "aspect-recalculation")
            result = GameplayMode::AspectRecalculation;
        else if (value == "horplus" || value == "hor+")
            result = GameplayMode::HorPlus;
        else return false;
        return true;
    }

    const char* GameplayModeName(GameplayMode mode)
    {
        switch (mode) {
        case GameplayMode::AspectRecalculation: return "AspectRecalculation";
        case GameplayMode::HorPlus: return "HorPlus";
        }
        return "AspectRecalculation";
    }

    bool ParseCinematicAspectPolicy(std::string value, CinematicAspectPolicy& result)
    {
        value = Trim(std::move(value));
        std::transform(value.begin(), value.end(), value.begin(),
            [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
        if (value == "auto" || value == "default" || value == "fix") result = CinematicAspectPolicy::Auto;
        else if (value == "native") result = CinematicAspectPolicy::Native;
        else if (value == "16:9" || value == "16x9") result = CinematicAspectPolicy::Forced16x9;
        else if (value == "21:9" || value == "21x9") result = CinematicAspectPolicy::Forced21x9;
        else if (value == "32:9" || value == "32x9") result = CinematicAspectPolicy::Forced32x9;
        else return false;
        return true;
    }

    const char* CinematicAspectPolicyName(CinematicAspectPolicy policy)
    {
        switch (policy) {
        case CinematicAspectPolicy::Auto: return "Auto";
        case CinematicAspectPolicy::Native: return "Native";
        case CinematicAspectPolicy::Forced16x9: return "16:9";
        case CinematicAspectPolicy::Forced21x9: return "21:9";
        case CinematicAspectPolicy::Forced32x9: return "32:9";
        }
        return "Auto";
    }

    bool ParseCinematicFovMode(std::string value, CinematicFovMode& result)
    {
        value = Trim(std::move(value));
        std::transform(value.begin(), value.end(), value.begin(),
            [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
        if (value == "nativehorplus" || value == "horplus" || value == "hor+")
            result = CinematicFovMode::NativeHorPlus;
        else if (value == "matchgameplay" || value == "match-gameplay")
            result = CinematicFovMode::GameplayHorPlus;
        else if (value == "gameplayhorplus" || value == "gameplay-horplus")
            result = CinematicFovMode::GameplayHorPlus;
        else return false;
        return true;
    }

    const char* CinematicFovModeName(CinematicFovMode mode)
    {
        switch (mode) {
        case CinematicFovMode::NativeHorPlus: return "NativeHorPlus";
        case CinematicFovMode::GameplayHorPlus: return "GameplayHorPlus";
        }
        return "NativeHorPlus";
    }

    bool ParseDialogueZoomPolicy(std::string value, DialogueZoomPolicy& result)
    {
        value = Trim(std::move(value));
        std::transform(value.begin(), value.end(), value.begin(),
            [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
        if (value == "native") result = DialogueZoomPolicy::Native;
        else if (value == "adaptive") result = DialogueZoomPolicy::Adaptive;
        else if (value == "reduced") result = DialogueZoomPolicy::Reduced;
        else if (value == "disabled") result = DialogueZoomPolicy::Disabled;
        else return false;
        return true;
    }

    const char* DialogueZoomPolicyName(DialogueZoomPolicy policy)
    {
        switch (policy) {
        case DialogueZoomPolicy::Native: return "Native";
        case DialogueZoomPolicy::Adaptive: return "Adaptive";
        case DialogueZoomPolicy::Reduced: return "Reduced";
        case DialogueZoomPolicy::Disabled: return "Disabled";
        }
        return "Native";
    }

    DialogueZoomPolicy NextDialogueZoomPolicy(DialogueZoomPolicy policy)
    {
        switch (policy) {
        case DialogueZoomPolicy::Native: return DialogueZoomPolicy::Adaptive;
        case DialogueZoomPolicy::Adaptive: return DialogueZoomPolicy::Reduced;
        case DialogueZoomPolicy::Reduced: return DialogueZoomPolicy::Disabled;
        case DialogueZoomPolicy::Disabled: return DialogueZoomPolicy::Native;
        }
        return DialogueZoomPolicy::Native;
    }

    CinematicAspectPolicy NextCinematicAspectPolicy(CinematicAspectPolicy policy)
    {
        switch (policy) {
        case CinematicAspectPolicy::Auto: return CinematicAspectPolicy::Native;
        case CinematicAspectPolicy::Native: return CinematicAspectPolicy::Forced16x9;
        case CinematicAspectPolicy::Forced16x9: return CinematicAspectPolicy::Forced21x9;
        case CinematicAspectPolicy::Forced21x9: return CinematicAspectPolicy::Forced32x9;
        case CinematicAspectPolicy::Forced32x9: return CinematicAspectPolicy::Auto;
        }
        return CinematicAspectPolicy::Auto;
    }

    CinematicFovMode NextCinematicFovMode(CinematicFovMode mode)
    {
        switch (mode) {
        case CinematicFovMode::NativeHorPlus: return CinematicFovMode::GameplayHorPlus;
        case CinematicFovMode::GameplayHorPlus: return CinematicFovMode::NativeHorPlus;
        }
        return CinematicFovMode::NativeHorPlus;
    }

    GameplayMode NextGameplayMode(GameplayMode mode)
    {
        switch (mode) {
        case GameplayMode::AspectRecalculation: return GameplayMode::HorPlus;
        case GameplayMode::HorPlus: return GameplayMode::AspectRecalculation;
        }
        return GameplayMode::AspectRecalculation;
    }
}
