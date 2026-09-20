#pragma once

#include "feature_config.hpp"

#include <filesystem>
#include <functional>
#include <string>

namespace config
{
    using LogFunction = std::function<void(std::string)>;
    using TemplateSynchronizer = std::function<bool(const std::filesystem::path&)>;

    std::filesystem::path DefaultConfigPath(const std::filesystem::path& moduleDirectory);

    bool LoadFeatureConfig(const std::filesystem::path& path, FeatureConfig& config,
        const TemplateSynchronizer& synchronizeTemplate, const LogFunction& log);

    bool PersistConfigValue(const std::filesystem::path& path,
        const char* targetSection, const char* targetKey,
        const std::string& value, const LogFunction& log);
}
