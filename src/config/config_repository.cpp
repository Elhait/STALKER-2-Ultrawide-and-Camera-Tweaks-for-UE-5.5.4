#include "config_repository.hpp"

#include "feature_config.hpp"

#include <Windows.h>

#include <fstream>
#include <utility>
#include <vector>

namespace config
{
    std::filesystem::path DefaultConfigPath(const std::filesystem::path& moduleDirectory)
    {
        return moduleDirectory / "STALKER2CameraTweaks.ini";
    }

    bool LoadFeatureConfig(const std::filesystem::path& path, FeatureConfig& config,
        const TemplateSynchronizer& synchronizeTemplate, const LogFunction& log)
    {
        if (!std::filesystem::exists(path)) {
            std::ofstream created(path, std::ios::out | std::ios::trunc);
            if (!created) return false;
            created << "; STALKER 2 Ultrawide and Camera Tweaks v1.0.0\n"
                << "; Author: Elhait\n"
                << "; GitHub: https://github.com/Elhait/STALKER-2-Ultrawide-Fix-for-UE-5.5.4\n"
                << "; Nexus Mods: https://www.nexusmods.com/stalker2heartofchornobyl/mods/2416\n"
                << ";\n"
                << "; Most configuration changes require restarting the game.\n"
                << "; When runtime hotkeys are enabled, gameplay mode changes apply immediately,\n"
                << "; while cinematic and dialogue selections apply to the next applicable event.\n"
                << ";\n"
                << "; ---------------------------------------------------------------------------\n"
                << "; DEFAULT / RECOMMENDED CONFIGURATION\n"
                << "; ---------------------------------------------------------------------------\n"
                << ";\n"
                << "; The default configuration is designed to provide the most seamless and\n"
                << "; consistent transition between gameplay and cinematics:\n"
                << ";\n"
                << ";   [Gameplay]\n"
                << ";   Mode=HorPlus\n"
                << ";\n"
                << ";   [Cinematics]\n"
                << ";   AspectRatio=Auto\n"
                << ";   FovMode=GameplayHorPlus\n"
                << ";\n"
                << "; HorPlus corrects gameplay for the current runtime aspect ratio.\n"
                << "; GameplayHorPlus uses the current Gameplay FOV as the cinematic baseline,\n"
                << "; so changes to the game's FOV setting are reflected in cinematics.\n"
                << "; Auto follows the current runtime aspect, including arbitrary/custom aspects.\n"
                << "; A forced cinematic aspect can also be seamless when it matches gameplay.\n"
                << "; AspectRecalculation and NativeHorPlus remain available as alternatives.\n"
                << "; When using Gameplay.Mode=HorPlus, GameplayHorPlus is strongly recommended.\n"
                << "; Another cinematic FovMode, or a forced aspect that differs from the\n"
                << "; actual Gameplay/display aspect, causes an aspect/FOV rebuild after\n"
                << "; Cinematics and may produce a visible transition.\n"
                << "\n[Gameplay]\n"
                << "; Enables the gameplay aspect-ratio correction.\n"
                << ";\n"
                << "; true  - enable the selected gameplay correction mode.\n"
                << "; false - leave the game's original gameplay camera/aspect behavior untouched.\n"
                << "Enabled=true\n"
                << "; Gameplay correction mode: HorPlus or AspectRecalculation.\n"
                << ";\n"
                << "; HorPlus - default and recommended mode. Preserves the game's native Gameplay FOV changes\n"
                << ";          and adapts them in real time to the current runtime aspect ratio. Supports\n"
                << ";          arbitrary/custom aspect ratios and native FOV changes such as ADS and binocular zoom.\n"
                << ";\n"
                << "; AspectRecalculation - alternative mode that uses the game's native aspect/projection transition\n"
                << ";          to correct Gameplay framing while preserving the selected Gameplay FOV.\n"
                << ";\n"
                << "; On custom windowed aspect ratios not represented by a native game aspect mode,\n"
                << ";          AspectRecalculation may return the window to the display's native aspect/size\n"
                << ";          when the game restores Auto. This limitation does not apply to HorPlus.\n"
                << ";\n"
                << "; HorPlus examples when idle:\n"
                << "; Gameplay FOV 90°:  16:9 -> 90°; 21:9 -> approximately 106.69°; 32:9 -> approximately 126.87°.\n"
                << "; Gameplay FOV 100°: 16:9 -> 100°; 21:9 -> approximately 116.04°; 32:9 -> approximately 134.48°.\n"
                << "; Gameplay FOV 110°: 16:9 -> 110°; 21:9 -> approximately 124.95°; 32:9 -> approximately 141.41°.\n"
                << "Mode=HorPlus\n"
                << "\n\n\n[Cinematics]\n"
                << "; Controls how cinematics are framed independently from the physical display.\n"
                << ";\n"
                << "; IMPORTANT FOR GAMEPLAY HORPLUS: use AspectRatio=Auto and\n"
                << "; FovMode=GameplayHorPlus, or force an aspect matching gameplay.\n"
                << ";\n"
                << "; Auto   - use the current runtime viewport/display aspect ratio.\n"
                << ";          Supports arbitrary valid runtime aspect ratios.\n"
                << ";          Recommended for most users and for GameplayHorPlus.\n"
                << ";\n"
                << "; Native - leave the game's original cinematic aspect and FOV behavior untouched.\n"
                << ";          In the current game version, Native may look similar or identical to 16:9\n"
                << ";          on ultrawide displays because this is how the game currently presents\n"
                << ";          its cinematics without intervention from the mod.\n"
                << ";          Native is kept as a true vanilla option and may automatically benefit\n"
                << ";          from future improvements to native ultrawide cinematic support.\n"
                << ";\n"
                << "; 16:9   - force 16:9 cinematic framing.\n"
                << ";\n"
                << "; 21:9   - force 21:9 cinematic framing, regardless of the physical display.\n"
                << ";\n"
                << "; 32:9   - force 32:9 cinematic framing, regardless of the physical display.\n"
                << ";\n"
                << "; Forced modes can also be used on displays with a different aspect ratio.\n"
                << "; For example, 32:9 on a 16:9 display produces a wider cinematic presentation\n"
                << "; with black bars above and below.\n"
                << "AspectRatio=Auto\n"
                << "; Cinematic FOV mode: GameplayHorPlus or NativeHorPlus.\n"
                << "; GameplayHorPlus - default when Gameplay.Mode=HorPlus. Uses the current\n"
                << ";                  native Gameplay FOV as the cinematic baseline, so\n"
                << ";                  cinematic FOV follows changes to the game's FOV setting.\n"
                << ";                  Preserves authored cinematic FOV variation and falls\n"
                << ";                  back safely when the required context is unavailable.\n"
                << ";                  Examples for authored cinematic 90° with Gameplay FOV 90°:\n"
                << ";                  16:9 -> 90°; 21:9 -> approximately 106.69°;\n"
                << ";                  32:9 -> approximately 126.87°. With Gameplay FOV 112.6°,\n"
                << ";                  the same cinematic becomes 112.6° at 16:9, 118.51° at 21:9\n"
                << ";                  and approximately 143.13° at 32:9.\n"
                << "; NativeHorPlus   - default-independent alternative. Applies Hor+ to the\n"
                << ";                  native/authored cinematic FOV and does not follow\n"
                << ";                  changes to the player's Gameplay FOV setting.\n"
                << ";                  Examples for authored cinematic 90°: 16:9 -> 90°;\n"
                << ";                  21:9 -> approximately 106.69°; 32:9 -> approximately\n"
                << ";                  126.87° regardless of Gameplay FOV.\n"
                << "FovMode=GameplayHorPlus\n"
                << "\n\n\n[Dialogue]\n"
                << "; Controls the camera zoom applied during dialogue.\n"
                << ";\n"
                << "; Native   - use the game's original dialogue zoom behavior.\n"
                << ";            Example: 90° gameplay FOV -> 70° during dialogue.\n"
                << ";            Example: 110° gameplay FOV -> 70° during dialogue.\n"
                << ";\n"
                << "; Adaptive - preserve the game's original optical zoom strength relative\n"
                << ";            to the current gameplay FOV.\n"
                << ";            Example: 90° gameplay FOV -> 70° during dialogue.\n"
                << ";            Example: 110° gameplay FOV -> approximately 90° during dialogue.\n"
                << ";\n"
                << "; Reduced  - apply half of the Adaptive optical zoom strength.\n"
                << ";            Example: 90° gameplay FOV -> approximately 80° during dialogue.\n"
                << ";            Example: 110° gameplay FOV -> approximately 100° during dialogue.\n"
                << ";\n"
                << "; Disabled - disable dialogue zoom and keep the current gameplay FOV.\n"
                << ";            Example: 90° gameplay FOV -> 90° during dialogue.\n"
                << ";            Example: 110° gameplay FOV -> 110° during dialogue.\n"
                << "Zoom=Adaptive\n"
                << "\n\n[Diagnostics]\n"
                << "; Enables the supported read-only runtime telemetry in the canonical ASI.\n"
                << "; false - keep diagnostic hooks and telemetry disabled.\n"
                << "; true  - enable CameraState, ZOOM and HorPlus FOV telemetry.\n"
                << "Enabled=false\n"
                << "\n\n\n[Hotkeys]\n"
                << "; Optional runtime controls for quickly comparing cinematic and dialogue modes\n"
                << "; without restarting the game.\n"
                << ";\n"
                << "; true  - enable all runtime hotkeys listed below.\n"
                << "; false - disable all runtime hotkeys. Recommended for normal gameplay.\n"
                << "Enabled=false\n"
                << "\n"
                << "; Key used to cycle the gameplay correction mode immediately.\n"
                << "; AspectRecalculation -> HorPlus -> AspectRecalculation.\n"
                << "; Supported keys: F1-F12, 0-9 and A-Z.\n"
                << "GameplayCycle=F9\n"
                << "; Key used to cycle the cinematic mode for the next cinematic.\n"
                << "; Auto -> Native -> 16:9 -> 21:9 -> 32:9 -> Auto.\n"
                << "; Does not affect a cinematic that is already playing.\n"
                << "; Supported keys: F1-F12, 0-9 and A-Z.\n"
                << "CinematicCycle=F10\n"
                << "; Key used to cycle the cinematic FOV mode for the next cinematic.\n"
                << "; NativeHorPlus -> GameplayHorPlus -> NativeHorPlus.\n"
                << "; Does not affect a cinematic that is already playing.\n"
                << "; Supported keys: F1-F12, 0-9 and A-Z.\n"
                << "CinematicFovCycle=F11\n"
                << "; Key used to cycle the dialogue zoom mode for the next dialogue.\n"
                << "; Native -> Adaptive -> Reduced -> Disabled -> Native.\n"
                << "; Does not affect a dialogue that is already in progress.\n"
                << "; Supported keys: F1-F12, 0-9 and A-Z.\n"
                << "DialogueCycle=F12\n";
            created.flush();
            if (!created) return false;
            created.close();
            return !created.fail();
        }

        if (synchronizeTemplate && synchronizeTemplate(path) && log)
            log("Config template synchronized: updated managed descriptions and hotkey settings.");

        std::ifstream input(path);
        if (!input) return false;
        std::string section;
        std::string line;
        while (std::getline(input, line)) {
            line = Trim(line);
            if (line.empty() || line.front() == ';' || line.front() == '#') continue;
            if (line.front() == '[' && line.back() == ']') {
                section = Trim(line.substr(1, line.size() - 2));
                continue;
            }
            const auto separator = line.find('=');
            if (separator == std::string::npos) continue;
            const auto key = Trim(line.substr(0, separator));
            if (section == "Cinematics" && key == "AspectRatio") {
                CinematicAspectPolicy policy{};
                if (ParseCinematicAspectPolicy(line.substr(separator + 1), policy)) {
                    config.cinematicAspectPolicy = policy;
                    config.cinematicAspectPolicyExplicit = true;
                }
                continue;
            }
            if (section == "Cinematics" && key == "FovMode") {
                CinematicFovMode mode{};
                if (ParseCinematicFovMode(line.substr(separator + 1), mode))
                    config.cinematicFovMode = mode;
                else if (log)
                    log("Unknown Cinematics.FovMode; using GameplayHorPlus.");
                continue;
            }
            if (section == "Gameplay" && key == "Mode") {
                GameplayMode mode{};
                if (ParseGameplayMode(line.substr(separator + 1), mode))
                    config.gameplayMode = mode;
                else if (log)
                    log("Unknown Gameplay.Mode; using HorPlus.");
                continue;
            }
            if (section == "Dialogue" && key == "Zoom") {
                DialogueZoomPolicy policy{};
                if (ParseDialogueZoomPolicy(line.substr(separator + 1), policy))
                    config.dialogueZoomPolicy = policy;
                continue;
            }
            if (section == "Diagnostics" && key == "Enabled") {
                bool enabled = false;
                if (ParseBool(line.substr(separator + 1), enabled))
                    config.diagnosticsEnabled = enabled;
                continue;
            }
            if (section == "Hotkeys" && key == "Enabled") {
                bool enabled = true;
                if (ParseBool(line.substr(separator + 1), enabled)) config.hotkeysEnabled = enabled;
                continue;
            }
            if (section == "Hotkeys" && key == "CinematicCycle") {
                int hotkey = VK_F10;
                if (ParseHotkey(line.substr(separator + 1), hotkey)) config.cinematicCycleKey = hotkey;
                continue;
            }
            if (section == "Hotkeys" && key == "CinematicFovCycle") {
                int hotkey = VK_F11;
                if (ParseHotkey(line.substr(separator + 1), hotkey)) config.cinematicFovCycleKey = hotkey;
                continue;
            }
            if (section == "Hotkeys" && key == "GameplayCycle") {
                int hotkey = VK_F9;
                if (ParseHotkey(line.substr(separator + 1), hotkey)) config.gameplayCycleKey = hotkey;
                continue;
            }
            if (section == "Hotkeys" && key == "DialogueCycle") {
                int hotkey = VK_F12;
                if (ParseHotkey(line.substr(separator + 1), hotkey)) config.dialogueCycleKey = hotkey;
                continue;
            }
            bool value = true;
            if (!ParseBool(line.substr(separator + 1), value)) continue;
            if (section == "Gameplay" && key == "Enabled") config.gameplayEnabled = value;
            else if (section == "Cinematics" && key == "AspectFix" && !config.cinematicAspectPolicyExplicit)
                config.cinematicAspectPolicy = value ? CinematicAspectPolicy::Auto : CinematicAspectPolicy::Native;
            else if (section == "Features" && key == "GameplayAspectFix") config.gameplayEnabled = value;
            else if (section == "Features" && key == "CinematicAspectFix" && !config.cinematicAspectPolicyExplicit)
                config.cinematicAspectPolicy = value ? CinematicAspectPolicy::Auto : CinematicAspectPolicy::Native;
        }
        return true;
    }

    bool PersistConfigValue(const std::filesystem::path& path,
        const char* targetSection, const char* targetKey,
        const std::string& value, const LogFunction& log)
    {
        std::ifstream input(path);
        if (!input) return false;

        std::vector<std::string> lines;
        std::string line;
        while (std::getline(input, line)) lines.push_back(std::move(line));
        input.close();

        bool inSection = false;
        bool replaced = false;
        for (auto& current : lines) {
            const auto trimmed = Trim(current);
            if (trimmed.size() >= 2 && trimmed.front() == '[' && trimmed.back() == ']')
                inSection = Trim(trimmed.substr(1, trimmed.size() - 2)) == targetSection;
            if (!inSection) continue;

            const auto separator = trimmed.find('=');
            if (separator != std::string::npos && Trim(trimmed.substr(0, separator)) == targetKey) {
                current = std::string(targetKey) + "=" + value;
                replaced = true;
            }
        }
        if (!replaced) {
            lines.push_back("");
            lines.push_back(std::string("[") + targetSection + "]");
            lines.push_back(std::string(targetKey) + "=" + value);
        }

        const auto temporary = path.wstring() + L".tmp";
        {
            std::ofstream output(temporary, std::ios::out | std::ios::trunc);
            if (!output) {
                DeleteFileW(temporary.c_str());
                return false;
            }
            for (std::size_t index = 0; index < lines.size(); ++index) {
                output << lines[index];
                if (index + 1 < lines.size()) output << '\n';
            }
            if (!output) {
                DeleteFileW(temporary.c_str());
                return false;
            }
            output.flush();
            if (!output) {
                DeleteFileW(temporary.c_str());
                return false;
            }
            output.close();
            if (output.fail()) {
                DeleteFileW(temporary.c_str());
                return false;
            }
        }

        if (MoveFileExW(temporary.c_str(), path.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) return true;

        const auto replaceError = GetLastError();
        if (log) log("Atomic INI replacement unavailable: win32Error=" +
            std::to_string(replaceError) + "; preserving existing config; no destructive fallback.");
        DeleteFileW(temporary.c_str());
        return false;
    }
}
