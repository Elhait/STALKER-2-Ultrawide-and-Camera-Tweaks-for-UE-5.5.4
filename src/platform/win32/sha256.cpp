#include "sha256.hpp"

#include <array>
#include <vector>
#include <windows.h>
#include <bcrypt.h>

#pragma comment(lib, "bcrypt.lib")

namespace platform::win32
{
    bool ComputeSha256(const std::filesystem::path& path, std::string& result)
    {
        const auto file = CreateFileW(path.c_str(), GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) return false;

        BCRYPT_ALG_HANDLE algorithm{};
        BCRYPT_HASH_HANDLE hash{};
        std::vector<std::uint8_t> hashObject;
        std::vector<std::uint8_t> buffer(1024 * 1024);
        std::array<std::uint8_t, 32> digest{};
        ULONG objectLength = 0;
        ULONG propertyLength = 0;
        bool success = false;
        do {
            if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0 ||
                BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH,
                    reinterpret_cast<PUCHAR>(&objectLength), sizeof(objectLength), &propertyLength, 0) != 0 ||
                objectLength == 0) break;
            hashObject.resize(objectLength);
            if (BCryptCreateHash(algorithm, &hash, hashObject.data(), objectLength,
                nullptr, 0, 0) != 0) break;
            for (;;) {
                DWORD count = 0;
                if (!ReadFile(file, buffer.data(), static_cast<DWORD>(buffer.size()), &count, nullptr)) break;
                if (count == 0) {
                    if (BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0) != 0) break;
                    static constexpr char digits[] = "0123456789ABCDEF";
                    result.clear();
                    result.reserve(digest.size() * 2);
                    for (const auto byte : digest) {
                        result.push_back(digits[byte >> 4]);
                        result.push_back(digits[byte & 0x0F]);
                    }
                    success = true;
                    break;
                }
                if (BCryptHashData(hash, buffer.data(), count, 0) != 0) break;
            }
        } while (false);
        if (hash) BCryptDestroyHash(hash);
        if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
        CloseHandle(file);
        return success;
    }
}
