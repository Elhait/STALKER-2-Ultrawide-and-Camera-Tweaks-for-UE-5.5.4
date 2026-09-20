#include "../../src/config/config_repository.hpp"
#include "../../src/config/config_template.hpp"

#include <Windows.h>

#include <array>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace
{
    std::string ReadText(const std::filesystem::path& path)
    {
        std::ifstream input(path, std::ios::binary);
        return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    }

    bool WriteText(const std::filesystem::path& path, const std::string& value)
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        output << value;
        return static_cast<bool>(output);
    }

    std::filesystem::path MakeTestDirectory()
    {
        wchar_t temporaryPath[MAX_PATH]{};
        if (GetTempPathW(MAX_PATH, temporaryPath) == 0) return {};
        const auto directory = std::filesystem::path(temporaryPath) /
            (L"STALKER2ConfigPersistenceHarness-" + std::to_wstring(GetCurrentProcessId()));
        std::error_code error;
        std::filesystem::remove_all(directory, error);
        std::filesystem::create_directories(directory, error);
        return error ? std::filesystem::path{} : directory;
    }

    bool Persist(const std::filesystem::path& path, const char* value)
    {
        return config::PersistConfigValue(path, "Gameplay", "Enabled", value,
            [](std::string) {});
    }

    bool Synchronize(const std::filesystem::path& path)
    {
        return config::SynchronizeManagedConfigTemplate(path,
            [](std::string) {});
    }

    bool TestNormalTemplateSynchronization(const std::filesystem::path& path,
        const std::string& original)
    {
        if (!WriteText(path, original)) return false;
        if (!Synchronize(path)) return false;
        const auto synchronized = ReadText(path);
        return synchronized.find("[Hotkeys]") != std::string::npos &&
            synchronized.find("[Diagnostics]") != std::string::npos &&
            synchronized.find("Enabled=false") != std::string::npos &&
            synchronized.find("; Enables the gameplay aspect-ratio correction.") != std::string::npos;
    }

    bool TestTemplateStagingFailurePreservesOriginal(const std::filesystem::path& path,
        const std::string& original)
    {
        if (!WriteText(path, original)) return false;
        const auto staging = std::filesystem::path(path.wstring() + L".tmp");
        std::error_code error;
        std::filesystem::create_directory(staging, error);
        if (error) return false;
        const bool result = Synchronize(path);
        std::filesystem::remove(staging, error);
        return !result && ReadText(path) == original;
    }

    bool TestTemplateReplacementFailurePreservesOriginal(const std::filesystem::path& path,
        const std::string& original)
    {
        if (!WriteText(path, original)) return false;
        const auto handle = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0,
            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle == INVALID_HANDLE_VALUE) return false;

        const bool result = Synchronize(path);
        CloseHandle(handle);
        const auto staging = std::filesystem::path(path.wstring() + L".tmp");
        std::error_code error;
        std::filesystem::remove(staging, error);
        return !result && ReadText(path) == original;
    }

    bool TestTemplateRepairPreservesValuesAndIsIdempotent(const std::filesystem::path& path)
    {
        const std::string damaged =
            "; user header\n"
            "[Gameplay]\n"
            "; user gameplay note\n"
            "Enabled=false\n"
            "UnknownGameplay=keep\n"
            "\n"
            "[Dialogue]\n"
            "Zoom=Potato\n";
        if (!WriteText(path, damaged)) return false;

        if (!Synchronize(path)) return false;
        const auto repaired = ReadText(path);
        const bool preserved =
            repaired.find("; user header") != std::string::npos &&
            repaired.find("; user gameplay note") != std::string::npos &&
            repaired.find("Enabled=false") != std::string::npos &&
            repaired.find("UnknownGameplay=keep") != std::string::npos &&
            repaired.find("Zoom=Potato") != std::string::npos &&
            repaired.find("[Cinematics]") != std::string::npos &&
            repaired.find("[Hotkeys]") != std::string::npos &&
            repaired.find("; Native   - use the game's original dialogue zoom behavior.") != std::string::npos;
        if (!preserved) return false;

        const auto secondBefore = repaired;
        if (Synchronize(path)) return false;
        return ReadText(path) == secondBefore;
    }

    bool TestNormalPersistence(const std::filesystem::path& path, const std::string& original)
    {
        if (!WriteText(path, original)) return false;
        return Persist(path, "false") && ReadText(path).find("Enabled=false") != std::string::npos;
    }

    bool TestDuplicateManagedOccurrencesConverge(const std::filesystem::path& path)
    {
        const std::string duplicate =
            "[Gameplay]\nEnabled=true\n\n"
            "[Gameplay]\nEnabled=true\n";
        if (!WriteText(path, duplicate) || !Persist(path, "false")) return false;
        config::FeatureConfig configuration{};
        if (!config::LoadFeatureConfig(path, configuration,
            [](const std::filesystem::path&) { return false; },
            [](std::string) {}) || configuration.gameplayEnabled)
            return false;
        const auto text = ReadText(path);
        const auto first = text.find("Enabled=false");
        const auto second = first == std::string::npos ? std::string::npos :
            text.find("Enabled=false", first + 1);
        return first != std::string::npos && second != std::string::npos;
    }

    bool TestStagingFailurePreservesOriginal(const std::filesystem::path& path,
        const std::string& original)
    {
        if (!WriteText(path, original)) return false;
        const auto staging = std::filesystem::path(path.wstring() + L".tmp");
        std::error_code error;
        std::filesystem::create_directory(staging, error);
        if (error) return false;
        const bool result = Persist(path, "false");
        std::filesystem::remove(staging, error);
        return !result && ReadText(path) == original;
    }

    bool TestReplacementFailurePreservesOriginal(const std::filesystem::path& path,
        const std::string& original)
    {
        if (!WriteText(path, original)) return false;
        const auto handle = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0,
            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle == INVALID_HANDLE_VALUE) return false;

        const bool result = Persist(path, "false");
        CloseHandle(handle);
        const auto staging = std::filesystem::path(path.wstring() + L".tmp");
        std::error_code error;
        std::filesystem::remove(staging, error);
        return !result && ReadText(path) == original;
    }

    bool TestDiagnosticsConfig(const std::filesystem::path& path)
    {
        if (!WriteText(path, "[Diagnostics]\nEnabled=true\n")) return false;
        config::FeatureConfig configuration{};
        return config::LoadFeatureConfig(path, configuration,
            [](const std::filesystem::path&) { return false; },
            [](std::string) {}) && configuration.diagnosticsEnabled;
    }

    bool TestCinematicFovModeConfig(const std::filesystem::path& path)
    {
        if (!WriteText(path, "[Cinematics]\nFovMode=GameplayHorPlus\n")) return false;
        config::FeatureConfig configuration{};
        if (!config::LoadFeatureConfig(path, configuration,
            [](const std::filesystem::path&) { return false; },
            [](std::string) {})) return false;
        if (configuration.cinematicFovMode != config::CinematicFovMode::GameplayHorPlus)
            return false;
        if (!WriteText(path, "[Cinematics]\nFovMode=invalid\n")) return false;
        configuration = {};
        if (!config::LoadFeatureConfig(path, configuration,
            [](const std::filesystem::path&) { return false; },
            [](std::string) {})) return false;
        if (configuration.cinematicFovMode != config::CinematicFovMode::GameplayHorPlus)
            return false;
        if (config::NextCinematicFovMode(config::CinematicFovMode::NativeHorPlus) !=
                config::CinematicFovMode::GameplayHorPlus ||
            config::NextCinematicFovMode(config::CinematicFovMode::GameplayHorPlus) !=
                config::CinematicFovMode::NativeHorPlus)
            return false;
        const std::array<config::CinematicAspectPolicy, 5> aspectPolicies{
            config::CinematicAspectPolicy::Auto,
            config::CinematicAspectPolicy::Native,
            config::CinematicAspectPolicy::Forced16x9,
            config::CinematicAspectPolicy::Forced21x9,
            config::CinematicAspectPolicy::Forced32x9};
        auto policy = aspectPolicies.front();
        for (std::size_t index = 1; index <= aspectPolicies.size(); ++index) {
            policy = config::NextCinematicAspectPolicy(policy);
            if (policy != aspectPolicies[index % aspectPolicies.size()]) return false;
        }
        if (!WriteText(path, "[Hotkeys]\nCinematicFovCycle=F12\n")) return false;
        configuration = {};
        if (!config::LoadFeatureConfig(path, configuration,
            [](const std::filesystem::path&) { return false; },
            [](std::string) {})) return false;
        return configuration.cinematicFovCycleKey == VK_F12;
    }
}

int main()
{
    const auto directory = MakeTestDirectory();
    if (directory.empty()) {
        std::printf("setup=FAIL\n");
        return 1;
    }

    const auto path = directory / "config.ini";
    const std::string original = "[Gameplay]\nEnabled=true\n";
    const bool normal = TestNormalPersistence(path, original);
    const bool duplicateOccurrences = TestDuplicateManagedOccurrencesConverge(path);
    const bool staging = TestStagingFailurePreservesOriginal(path, original);
    const bool replacement = TestReplacementFailurePreservesOriginal(path, original);
    const bool templateNormal = TestNormalTemplateSynchronization(path, original);
    const bool templateStaging = TestTemplateStagingFailurePreservesOriginal(path, original);
    const bool templateReplacement = TestTemplateReplacementFailurePreservesOriginal(path, original);
    const bool templateRepair = TestTemplateRepairPreservesValuesAndIsIdempotent(path);
    const bool diagnostics = TestDiagnosticsConfig(path);
    const bool cinematicFovMode = TestCinematicFovModeConfig(path);

    std::error_code error;
    std::filesystem::remove_all(directory, error);
    std::printf("normal=%s staging_failure_preserves=%s replacement_failure_preserves=%s "
        "template_normal=%s template_staging_failure_preserves=%s "
        "template_replacement_failure_preserves=%s template_repair_idempotent=%s "
        "diagnostics_config=%s cinematic_fov_mode=%s duplicate_occurrences=%s\n",
        normal ? "PASS" : "FAIL", staging ? "PASS" : "FAIL",
        replacement ? "PASS" : "FAIL", templateNormal ? "PASS" : "FAIL",
        templateStaging ? "PASS" : "FAIL", templateReplacement ? "PASS" : "FAIL",
        templateRepair ? "PASS" : "FAIL", diagnostics ? "PASS" : "FAIL",
        cinematicFovMode ? "PASS" : "FAIL", duplicateOccurrences ? "PASS" : "FAIL");
    return normal && staging && replacement && templateNormal && templateStaging &&
        templateReplacement && templateRepair && diagnostics && cinematicFovMode &&
        duplicateOccurrences ? 0 : 1;
}
