/*
  Header-only utility library
*/
#pragma once

#include <expected>
#include <format>
#include <filesystem>
#include <stdexcept>
#include <string>

#include "base64.h"

#ifndef WIN32_MEAN_AND_LEAN
#define WIN32_MEAN_AND_LEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>

#define SECURITY_WIN32
#include <security.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Security.Credentials.h>
#include <winrt/Windows.Security.Authentication.Web.h>
#include <winrt/Windows.Security.Authentication.Web.Core.h>
#include <winrt/Windows.Security.Authentication.Web.Provider.h>

using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Security::Credentials;
using namespace Windows::Security::Authentication::Web::Core;

namespace Util::detail
{
    // Maps to convert some enums to strings
#define MAPENTRY(value) { value, #value }

    inline extern const auto WebAccountStateMap = std::unordered_map<WebAccountState, std::string>
    {
        MAPENTRY(WebAccountState::None),
        MAPENTRY(WebAccountState::Connected),
        MAPENTRY(WebAccountState::Error)
    };

    inline extern const auto FindAllWebAccountsStatusMap = std::unordered_map<FindAllWebAccountsStatus, std::string>
    {
        MAPENTRY(FindAllWebAccountsStatus::Success),
        MAPENTRY(FindAllWebAccountsStatus::ProviderError),
        MAPENTRY(FindAllWebAccountsStatus::NotAllowedByProvider),
        MAPENTRY(FindAllWebAccountsStatus::NotSupportedByProvider)
    };

    inline extern const auto WebTokenRequestStatusMap = std::unordered_map<WebTokenRequestStatus, std::string>
    {
        MAPENTRY(WebTokenRequestStatus::AccountProviderNotAvailable),
        MAPENTRY(WebTokenRequestStatus::AccountSwitch),
        MAPENTRY(WebTokenRequestStatus::ProviderError),
        MAPENTRY(WebTokenRequestStatus::Success),
        MAPENTRY(WebTokenRequestStatus::UserCancel),
        MAPENTRY(WebTokenRequestStatus::UserInteractionRequired),
    };

    inline extern const auto WebTokenRequestPromptTypeMap = std::unordered_map<WebTokenRequestPromptType, std::string>
    {
        MAPENTRY(WebTokenRequestPromptType::Default),
        MAPENTRY(WebTokenRequestPromptType::ForceAuthentication)
    };

#undef MAPENTRY
}

namespace winrt::Windows::Security::Authentication::Web::Core
{
    // Add to_string() functions in the same namespace as the target object so that ADL works.

    inline const std::string& to_string(WebAccountState accountState) noexcept
    {
        return Util::detail::WebAccountStateMap.at(accountState);
    }

    inline const std::string& to_string(FindAllWebAccountsStatus status) noexcept
    {
        return Util::detail::FindAllWebAccountsStatusMap.at(status);
    }

    inline const std::string& to_string(WebTokenRequestStatus status) noexcept
    {
        return Util::detail::WebTokenRequestStatusMap.at(status);
    }

    inline const std::string& to_string(WebTokenRequestPromptType prompt)
    {
        return Util::detail::WebTokenRequestPromptTypeMap.at(prompt);
    }
}

// Custom formatters for WAM types
template<>
struct std::formatter<WebAccountState> : std::formatter<std::string_view>
{
    auto format(const WebAccountState& val, std::format_context& ctx) const
    {
        return std::formatter<std::string_view>::format(Util::detail::WebAccountStateMap.at(val), ctx);
    }
};

template<>
struct std::formatter<FindAllWebAccountsStatus> : std::formatter<std::string_view>
{
    auto format(const FindAllWebAccountsStatus& val, std::format_context& ctx) const
    {
        return std::formatter<std::string_view>::format(Util::detail::FindAllWebAccountsStatusMap.at(val), ctx);
    }
};

template<>
struct std::formatter<WebTokenRequestStatus> : std::formatter<std::string_view>
{
    auto format(const WebTokenRequestStatus& val, std::format_context& ctx) const
    {
        return std::formatter<std::string_view>::format(Util::detail::WebTokenRequestStatusMap.at(val), ctx);
    }
};

template<>
struct std::formatter<WebTokenRequestPromptType> : std::formatter<std::string_view>
{
    auto format(const WebTokenRequestPromptType& val, std::format_context& ctx) const
    {
        return std::formatter<std::string_view>::format(Util::detail::WebTokenRequestPromptTypeMap.at(val), ctx);
    }
};

namespace Util
{
    inline std::string to_string(std::wstring_view str)
    {
        if (str.empty())
        {
            return {};
        }

        auto cb = ::WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), nullptr, 0, nullptr, nullptr);
        auto utf8 = std::string(cb, 0);
        ::WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), utf8.data(), cb, nullptr, nullptr);

        return utf8;
    }

    inline std::wstring to_wstring(std::string_view str)
    {
        if (str.empty())
        {
            return {};
        }

        auto cch = ::MultiByteToWideChar(CP_ACP, 0, str.data(), static_cast<int>(str.size()), nullptr, 0);
        auto wstr = std::wstring(cch, 0);
        ::MultiByteToWideChar(CP_ACP, 0, str.data(), static_cast<int>(str.size()), wstr.data(), cch);

        return wstr;
    }

    inline bool equals(std::string_view s1, std::string_view s2)
    {
        // string_view is not guaranteed to be null-terminated, 
        // but it must be null-terminated for _stricmp
        if (*(data(s1) + s1.length()) != '\0' || *(data(s2) + s2.length()) != '\0')
        {
            throw std::invalid_argument{ "input must be null terminated" };
        }

        return _stricmp(data(s1), data(s2)) == 0;
    }

    inline bool equals(std::wstring_view s1, std::wstring_view s2)
    {
        // wstring_view is not guaranteed to be null-terminated,
        // but it must be null-terminated for _wcsicmp
        if (*(data(s1) + s1.length()) != L'\0' || *(data(s2) + s2.length()) != L'\0')
        {
            throw std::invalid_argument{ "input must be null terminated" };
        }

        return _wcsicmp(data(s1), data(s2)) == 0;
    }

    inline std::string decode_base64url(std::string_view base64url)
    {
        auto result = std::string{ base64url };

        std::replace(begin(result), end(result), '-', '+');
        std::replace(begin(result), end(result), '_', '/');

        switch (base64url.length() % 4)
        {
        case 0:
            break;

        case 2:
            result += "==";
            break;
        case 3:
            result += "=";
            break;
        default:
            throw std::runtime_error{ "Invalid Base64URL string" };
        }

        return base64::from_base64(result);
    }

    /// <summary>
    /// Get path to the given module
    /// </summary>
    /// <param name="hModule">module handle. nullptr for current executable.</param>
    /// <returns>std::filesystem::path to the module</returns>
    inline std::filesystem::path GetModulePath(HMODULE hModule)
    {
        auto exePath = std::wstring(MAX_PATH, 0);
        auto cch = ::GetModuleFileNameW(hModule, exePath.data(), static_cast<DWORD>(exePath.size()));
        exePath.resize(cch);

        return exePath;
    }

    /// <summary>
    /// Get file version as std::string
    /// </summary>
    /// <param name="filePath">Path to the file</param>
    /// <returns>version string as std::optional</returns>
    inline std::expected<std::string, std::string> GetFileVersion(const std::filesystem::path& filePath) noexcept
    {
        const auto size = ::GetFileVersionInfoSize(filePath.c_str(), nullptr);

        if (size == 0)
        {
            return std::unexpected{ std::format("GetFileVersionInfoSize failed with {}", GetLastError()) };
        }

        const auto pData = std::make_unique<BYTE[]>(size);

        if (!::GetFileVersionInfoW(filePath.c_str(), 0, size, pData.get()))
        {
            return std::unexpected { std::format("GetFileVersionInfoW failed with {}\n", GetLastError()) };
        }

        VS_FIXEDFILEINFO* pFileInfo{};
        auto fileInfoSize = UINT{};

        if (!::VerQueryValue(pData.get(), L"\\", (LPVOID*)&pFileInfo, &fileInfoSize))
        {
            return std::unexpected{ std::format("VerQueryValue failed with {}", GetLastError()) };
        }

        const auto major = (pFileInfo->dwFileVersionMS >> 16) & 0xffff;
        const auto minor = pFileInfo->dwFileVersionMS & 0xffff;
        const auto revision = (pFileInfo->dwFileVersionLS >> 16) & 0xffff;

        return std::format("{}.{}.{}", major, minor, revision);
    }

    inline std::expected<std::string, std::string> GetCurrentUserName()
    {
        auto name = std::wstring{};
        auto cch = DWORD{};

        ::GetUserNameExW(NameSamCompatible, name.data(), &cch);
        auto ec = ::GetLastError();

        if (ec == ERROR_MORE_DATA)
        {
            // This cch includes terminating null
            name.resize(cch);
        }

        if (::GetUserNameExW(NameSamCompatible, name.data(), &cch))
        {
            // This cch does NOT include terminating null
            name.resize(cch);
            return to_string(name);
        }

        ec = ::GetLastError();
        return std::unexpected{ std::format("GetUserNameExW failed with {:#x}", static_cast<std::uint32_t>(ec)) };
    }

    template <std::invocable F>
    struct DtorAction
    {
        [[nodiscard]] explicit DtorAction(F f) : action{ f } {}
        ~DtorAction() { action(); };
        F action;
    };
}
