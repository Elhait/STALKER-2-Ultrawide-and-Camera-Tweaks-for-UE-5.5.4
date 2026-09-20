#include "config_template.hpp"

#include "feature_config.hpp"

#include <array>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>
#include <Windows.h>

namespace config
{
    namespace
    {
        struct ManagedKey
        {
            std::string_view name;
            std::string_view defaultLine;
            const std::string_view* comments;
            std::size_t commentCount;
        };

        constexpr std::string_view gameplayEnabledComments[] = {
            "; Enables the gameplay aspect-ratio correction.", ";",
            "; true  - enable the selected gameplay correction mode.",
            "; false - leave the game's original gameplay camera/aspect behavior untouched.",
        };
        constexpr std::string_view gameplayModeComments[] = {
            "; Gameplay correction mode: HorPlus or AspectRecalculation.",
            ";",
            "; HorPlus - default and recommended mode. Preserves the game's native Gameplay FOV changes",
            ";          and adapts them in real time to the current runtime aspect ratio. Supports",
            ";          arbitrary/custom aspect ratios and native FOV changes such as ADS and binocular zoom.",
            ";",
            "; AspectRecalculation - alternative mode that uses the game's native aspect/projection transition",
            ";          to correct Gameplay framing while preserving the selected Gameplay FOV.",
            ";",
            "; On custom windowed aspect ratios not represented by a native game aspect mode,",
            ";          AspectRecalculation may return the window to the display's native aspect/size",
            ";          when the game restores Auto. This limitation does not apply to HorPlus.",
            ";",
            "; HorPlus examples when idle:",
            "; Gameplay FOV 90°:  16:9 -> 90°; 21:9 -> approximately 106.69°; 32:9 -> approximately 126.87°.",
            "; Gameplay FOV 100°: 16:9 -> 100°; 21:9 -> approximately 116.04°; 32:9 -> approximately 134.48°.",
            "; Gameplay FOV 110°: 16:9 -> 110°; 21:9 -> approximately 124.95°; 32:9 -> approximately 141.41°.",
        };
        constexpr std::string_view cinematicComments[] = {
            "; Controls how cinematics are framed independently from the physical display.", ";",
            "; Auto   - use the current runtime viewport/display aspect ratio.",
            ";          Supports arbitrary runtime aspects and is recommended for most users.", ";",
            "; Native - leave the game's original cinematic aspect and FOV behavior untouched.",
            ";          In the current game version, Native may look similar or identical to 16:9",
            ";          on ultrawide displays because this is how the game currently presents",
            ";          its cinematics without intervention from the mod.",
            ";          Native is kept as a true vanilla option and may automatically benefit",
            ";          from future improvements to native ultrawide cinematic support.", ";",
            "; 16:9   - force 16:9 cinematic framing.", ";",
            "; 21:9   - force 21:9 cinematic framing, regardless of the physical display.", ";",
            "; 32:9   - force 32:9 cinematic framing, regardless of the physical display.", ";",
            "; Forced modes can also be used on displays with a different aspect ratio.",
            "; For example, 32:9 on a 16:9 display produces a wider cinematic presentation",
            "; with black bars above and below.",
        };
        constexpr std::string_view cinematicFovComments[] = {
            "; Cinematic FOV mode: GameplayHorPlus or NativeHorPlus.",
            "; GameplayHorPlus - default with Gameplay.Mode=HorPlus; follows changes to",
            ";                  the game's Gameplay FOV while preserving authored variation.",
            ";                  Falls back safely when required context is unavailable.",
            ";                  Examples with authored cinematic 90° and Gameplay FOV 90°:",
            ";                  16:9 -> 90°; 21:9 -> 106.69°; 32:9 -> 126.87°.",
            ";                  With Gameplay FOV 112.6°: 16:9 -> 112.6°; 21:9 -> 118.51°;",
            ";                  32:9 -> approximately 143.13°.",
            "; NativeHorPlus   - alternative native/authored cinematic FOV mode that does",
            ";                  not follow the player's Gameplay FOV setting.",
            ";                  Examples for authored cinematic 90°: 16:9 -> 90°;",
            ";                  21:9 -> 106.69°; 32:9 -> approximately 126.87°.",
        };
        constexpr std::string_view dialogueComments[] = {
            "; Controls the camera zoom applied during dialogue.", ";",
            "; Native   - use the game's original dialogue zoom behavior.",
            ";            Example: 90° gameplay FOV -> 70° during dialogue.",
            ";            Example: 110° gameplay FOV -> 70° during dialogue.", ";",
            "; Adaptive - preserve the game's original optical zoom strength relative",
            ";            to the current gameplay FOV.",
            ";            Example: 90° gameplay FOV -> 70° during dialogue.",
            ";            Example: 110° gameplay FOV -> approximately 90° during dialogue.", ";",
            "; Reduced  - apply half of the Adaptive optical zoom strength.",
            ";            Example: 90° gameplay FOV -> approximately 80° during dialogue.",
            ";            Example: 110° gameplay FOV -> approximately 100° during dialogue.", ";",
            "; Disabled - disable dialogue zoom and keep the current gameplay FOV.",
            ";            Example: 90° gameplay FOV -> 90° during dialogue.",
            ";            Example: 110° gameplay FOV -> 110° during dialogue.",
        };
        constexpr std::string_view diagnosticsComments[] = {
            "; Enables the supported read-only runtime telemetry in the canonical ASI.",
            "; false - keep diagnostic hooks and telemetry disabled.",
            "; true  - enable CameraState, ZOOM and HorPlus FOV telemetry.",
        };
        constexpr std::string_view hotkeyComments[] = {
            "; Optional runtime controls for quickly comparing cinematic and dialogue modes",
            "; without restarting the game.", ";",
            "; true  - enable all runtime hotkeys listed below.",
            "; false - disable all runtime hotkeys. Recommended for normal gameplay.",
            "; Key used to cycle the gameplay correction mode immediately.",
            "; AspectRecalculation -> HorPlus -> AspectRecalculation.",
            "; Supported keys: F1-F12, 0-9 and A-Z.",
            "; Key used to cycle the cinematic mode for the next cinematic.",
            "; Auto -> Native -> 16:9 -> 21:9 -> 32:9 -> Auto.",
            "; Does not affect a cinematic that is already playing.",
            "; Supported keys: F1-F12, 0-9 and A-Z.",
            "; Key used to cycle the cinematic FOV mode for the next cinematic.",
            "; NativeHorPlus -> GameplayHorPlus -> NativeHorPlus.",
            "; Does not affect a cinematic that is already playing.",
            "; Supported keys: F1-F12, 0-9 and A-Z.",
            "; Key used to cycle the dialogue zoom mode for the next dialogue.",
            "; Native -> Adaptive -> Reduced -> Disabled -> Native.",
            "; Does not affect a dialogue that is already in progress.",
            "; Supported keys: F1-F12, 0-9 and A-Z.",
        };

        constexpr ManagedKey gameplayKeys[] = {
            {"Enabled", "Enabled=true", gameplayEnabledComments, std::size(gameplayEnabledComments)},
            {"Mode", "Mode=HorPlus", gameplayModeComments, std::size(gameplayModeComments)},
        };
        constexpr ManagedKey cinematicKeys[] = {
            {"AspectRatio", "AspectRatio=Auto", cinematicComments, std::size(cinematicComments)},
            {"FovMode", "FovMode=GameplayHorPlus", cinematicFovComments, std::size(cinematicFovComments)},
        };
        constexpr ManagedKey dialogueKeys[] = {
            {"Zoom", "Zoom=Adaptive", dialogueComments, std::size(dialogueComments)},
        };
        constexpr ManagedKey diagnosticsKeys[] = {
            {"Enabled", "Enabled=false", diagnosticsComments, std::size(diagnosticsComments)},
        };
        constexpr ManagedKey hotkeyKeys[] = {
            {"Enabled", "Enabled=false", hotkeyComments, 5},
            {"GameplayCycle", "GameplayCycle=F9", hotkeyComments + 5, 4},
            {"CinematicCycle", "CinematicCycle=F10", hotkeyComments + 9, 4},
            {"CinematicFovCycle", "CinematicFovCycle=F11", hotkeyComments + 13, 4},
            {"DialogueCycle", "DialogueCycle=F12", hotkeyComments + 17, 4},
        };

        const ManagedKey* KeysForSection(const std::string& section, std::size_t& count)
        {
            if (section == "Gameplay") { count = std::size(gameplayKeys); return gameplayKeys; }
            if (section == "Cinematics") { count = std::size(cinematicKeys); return cinematicKeys; }
            if (section == "Dialogue") { count = std::size(dialogueKeys); return dialogueKeys; }
            if (section == "Diagnostics") { count = std::size(diagnosticsKeys); return diagnosticsKeys; }
            if (section == "Hotkeys") { count = std::size(hotkeyKeys); return hotkeyKeys; }
            count = 0;
            return nullptr;
        }

        bool IsManagedComment(const std::string& section, const std::string& line)
        {
            std::size_t keyCount = 0;
            const auto* keys = KeysForSection(section, keyCount);
            for (std::size_t keyIndex = 0; keyIndex < keyCount; ++keyIndex)
                for (std::size_t commentIndex = 0; commentIndex < keys[keyIndex].commentCount; ++commentIndex)
                    if (line == keys[keyIndex].comments[commentIndex]) return true;

            if (section == "Gameplay")
                return line == "; Correct gameplay aspect behavior on ultrawide displays." ||
                    line == "; Enables ultrawide aspect-ratio correction during gameplay." ||
                    line == "; Use true to enable the feature or false to disable it.";
            if (section == "Cinematics")
                return line == "; Controls cinematic framing on ultrawide displays." ||
                    line == "; Auto   - use the detected display aspect ratio." ||
                    line == "; Native - keep the game's original cinematic behavior." ||
                    line == "; 16:9   - force the native 16:9 cinematic frame." ||
                    line == "; 21:9   - force a 21:9 cinematic frame." ||
                    line == "; 32:9   - force a 32:9 cinematic frame." ||
                    line == "; Auto, Native, 16:9, 21:9, 32:9";
            if (section == "Dialogue")
                return line == "; Controls the native dialogue camera zoom." ||
                    line == "; Native   - use the game's original dialogue zoom, currently targeting 70°." ||
                    line == "; Adaptive - preserve the native optical zoom strength relative to the current gameplay FOV." ||
                    line == "; Reduced  - apply half of the Adaptive optical zoom strength." ||
                    line == ";            Example: 110° gameplay FOV -> Adaptive ≈90°, Reduced ≈100°." ||
                    line == "; Disabled - keep the current gameplay FOV during dialogue." ||
                    line == "; Native, Reduced, Disabled";
            if (section == "Hotkeys")
                return line == "; Enables or disables all runtime hotkeys." ||
                    line == "; Use true to enable all runtime hotkeys or false to disable them." ||
                    line == "; Optional runtime controls for quickly testing different settings without restarting the game." ||
                    line == "; Intended mainly for comparing modes and finding a preferred configuration; disable for normal use." ||
                    line == "; Key used to cycle the cinematic FOV mode for the next cinematic." ||
                    line == "; NativeHorPlus -> GameplayHorPlus -> NativeHorPlus." ||
                    line == "; Key used to cycle the gameplay correction mode immediately." ||
                    line == "; AspectRecalculation -> HorPlus -> AspectRecalculation." ||
                    line == "; It does not change a cinematic that is already playing." ||
                    line == "; It does not change a dialogue that is already in progress.";
            return false;
        }

        bool IsSectionHeader(const std::string& line, std::string& section)
        {
            if (line.size() < 2 || line.front() != '[' || line.back() != ']') return false;
            section = Trim(line.substr(1, line.size() - 2));
            return true;
        }
    }

    bool SynchronizeManagedConfigTemplate(const std::filesystem::path& path,
        const TemplateLogFunction& log)
    {
        std::ifstream input(path);
        if (!input) return false;

        std::vector<std::string> source;
        std::string line;
        while (std::getline(input, line)) source.push_back(std::move(line));
        input.close();

        const auto appendSection = [](std::vector<std::string>& output,
            const char* sectionName, const ManagedKey* keys, std::size_t keyCount) {
                if (!output.empty() && !output.back().empty()) output.emplace_back("");
                output.emplace_back("[" + std::string(sectionName) + "]");
                for (std::size_t keyIndex = 0; keyIndex < keyCount; ++keyIndex) {
                    for (std::size_t commentIndex = 0; commentIndex < keys[keyIndex].commentCount; ++commentIndex)
                        output.emplace_back(std::string(keys[keyIndex].comments[commentIndex]));
                    output.emplace_back(std::string(keys[keyIndex].defaultLine));
                }
            };

        const auto repairSection = [](const std::vector<std::string>& body,
            const std::string& section, const ManagedKey* keys, std::size_t keyCount) {
                std::vector<std::string> repaired;
                std::vector<bool> found(keyCount, false);
                for (const auto& original : body) {
                    const auto trimmed = Trim(original);
                    if (IsManagedComment(section, trimmed)) continue;

                    std::size_t matchingKey = keyCount;
                    const auto separator = trimmed.find('=');
                    if (separator != std::string::npos) {
                        const auto key = Trim(trimmed.substr(0, separator));
                        for (std::size_t keyIndex = 0; keyIndex < keyCount; ++keyIndex)
                            if (key == keys[keyIndex].name) matchingKey = keyIndex;
                    }

                    if (matchingKey < keyCount && !found[matchingKey]) {
                        for (std::size_t commentIndex = 0; commentIndex < keys[matchingKey].commentCount; ++commentIndex)
                            repaired.emplace_back(std::string(keys[matchingKey].comments[commentIndex]));
                        found[matchingKey] = true;
                    }
                    repaired.emplace_back(original);
                }

                for (std::size_t keyIndex = 0; keyIndex < keyCount; ++keyIndex) {
                    if (found[keyIndex]) continue;
                    if (!repaired.empty() && !repaired.back().empty()) repaired.emplace_back("");
                    for (std::size_t commentIndex = 0; commentIndex < keys[keyIndex].commentCount; ++commentIndex)
                        repaired.emplace_back(std::string(keys[keyIndex].comments[commentIndex]));
                    repaired.emplace_back(std::string(keys[keyIndex].defaultLine));
                }
                return repaired;
            };

        std::vector<std::string> output;
        bool changed = false;
        std::array<bool, 5> foundSections{};
        std::size_t index = 0;
        while (index < source.size()) {
            std::string section;
            if (!IsSectionHeader(Trim(source[index]), section)) {
                output.emplace_back(source[index++]);
                continue;
            }

            const auto sectionStart = index++;
            std::size_t sectionEnd = index;
            while (sectionEnd < source.size()) {
                std::string nextSection;
                if (IsSectionHeader(Trim(source[sectionEnd]), nextSection)) break;
                ++sectionEnd;
            }

            std::size_t keyCount = 0;
            const auto* keys = KeysForSection(section, keyCount);
            if (keys == nullptr) {
                output.insert(output.end(), source.begin() + sectionStart, source.begin() + sectionEnd);
                index = sectionEnd;
                continue;
            }

            const std::vector<std::string> body(source.begin() + sectionStart + 1,
                source.begin() + sectionEnd);
            const auto repaired = repairSection(body, section, keys, keyCount);
            output.emplace_back(source[sectionStart]);
            output.insert(output.end(), repaired.begin(), repaired.end());
            if (repaired != body) changed = true;

            if (section == "Gameplay") foundSections[0] = true;
            if (section == "Cinematics") foundSections[1] = true;
            if (section == "Dialogue") foundSections[2] = true;
            if (section == "Diagnostics") foundSections[3] = true;
            if (section == "Hotkeys") foundSections[4] = true;
            index = sectionEnd;
        }

        constexpr const char* sectionNames[] = {
            "Gameplay", "Cinematics", "Dialogue", "Diagnostics", "Hotkeys"};
        for (std::size_t sectionIndex = 0; sectionIndex < std::size(sectionNames); ++sectionIndex) {
            if (foundSections[sectionIndex]) continue;
            std::size_t keyCount = 0;
            const auto* keys = KeysForSection(sectionNames[sectionIndex], keyCount);
            appendSection(output, sectionNames[sectionIndex], keys, keyCount);
            changed = true;
        }

        if (!changed && output == source) return false;
        const auto temporary = std::filesystem::path(path.wstring() + L".tmp");
        {
            std::ofstream file(temporary, std::ios::out | std::ios::trunc);
            if (!file) {
                DeleteFileW(temporary.c_str());
                if (log) log("Config template staging open failed; existing config preserved.");
                return false;
            }
            for (std::size_t outputIndex = 0; outputIndex < output.size(); ++outputIndex) {
                file << output[outputIndex];
                if (outputIndex + 1 < output.size()) file << '\n';
            }
            file.flush();
            if (!file) {
                file.close();
                DeleteFileW(temporary.c_str());
                if (log) log("Config template staging write failed; existing config preserved.");
                return false;
            }
            file.close();
            if (file.fail()) {
                DeleteFileW(temporary.c_str());
                if (log) log("Config template staging close failed; existing config preserved.");
                return false;
            }
        }

        if (MoveFileExW(temporary.c_str(), path.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) return true;

        const auto replaceError = GetLastError();
        DeleteFileW(temporary.c_str());
        if (log) log("Config template replacement failed; existing config preserved. win32Error=" +
            std::to_string(replaceError) + ".");
        return false;
    }
}
