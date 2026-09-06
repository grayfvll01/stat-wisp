#include "startup/StartupManager.h"

#include <Windows.h>

#include <array>
#include <string>

namespace statwisp
{
namespace
{

constexpr wchar_t kRunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kValueName[] = L"stat-wisp";

} // namespace

bool StartupManager::SetEnabled(bool enabled) noexcept
{
    HKEY key{};
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kRunKey, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) !=
        ERROR_SUCCESS)
    {
        return false;
    }
    LSTATUS result = ERROR_SUCCESS;
    if (enabled)
    {
        std::array<wchar_t, 32768> executable{};
        const auto length = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
        if (length == 0 || length >= executable.size())
        {
            RegCloseKey(key);
            return false;
        }
        const std::wstring command = L"\"" + std::wstring(executable.data(), length) + L"\"";
        result = RegSetValueExW(key, kValueName, 0, REG_SZ, reinterpret_cast<const BYTE *>(command.c_str()),
                                static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));
    }
    else
    {
        result = RegDeleteValueW(key, kValueName);
        if (result == ERROR_FILE_NOT_FOUND)
        {
            result = ERROR_SUCCESS;
        }
    }
    RegCloseKey(key);
    return result == ERROR_SUCCESS;
}

bool StartupManager::IsEnabled() noexcept
{
    return RegGetValueW(HKEY_CURRENT_USER, kRunKey, kValueName, RRF_RT_REG_SZ, nullptr, nullptr, nullptr) ==
           ERROR_SUCCESS;
}

} // namespace statwisp
