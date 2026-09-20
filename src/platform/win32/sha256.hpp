#pragma once

#include <filesystem>
#include <string>

namespace platform::win32
{
    bool ComputeSha256(const std::filesystem::path& path, std::string& result);
}
