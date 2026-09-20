#pragma once

#include <filesystem>
#include <functional>
#include <string>

namespace config
{
    using TemplateLogFunction = std::function<void(std::string)>;

    bool SynchronizeManagedConfigTemplate(const std::filesystem::path& path,
        const TemplateLogFunction& log = {});
}
