/*
  Header-only utility library
*/
#pragma once

#include <expected>
#include <filesystem>
#include <string>

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

    inline extern const auto WebTokenRequestStatusMap = std::unordered_map<WebTokenRequestStatus, const char*>
    {
        MAPENTRY(WebTokenRequestStatus::AccountProviderNotAvailable),
        MAPENTRY(WebTokenRequestStatus::AccountSwitch),
        MAPENTRY(WebTokenRequestStatus::ProviderError),
        MAPENTRY(WebTokenRequestStatus::Success),
        MAPENTRY(WebTokenRequestStatus::UserCancel),
        MAPENTRY(WebTokenRequestStatus::UserInteractionRequired),
    };

#undef MAPENTRY
}

namespace Util
{
    inline std::string to_string(std::wstring_view str) noexcept
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

    inline std::wstring to_wstring(std::string_view str) noexcept
    {
        if (str.empty())
        {
            return {};
        }

        auto cch = ::MultiByteToWideChar(CP_ACP, 0, &str[0], static_cast<int>(str.size()), nullptr, 0);
        auto wstr = std::wstring(cch, 0);
        ::MultiByteToWideChar(CP_ACP, 0, str.data(), static_cast<int>(str.size()), wstr.data(), cch);

        return wstr;
    }

    inline std::string to_string(WebAccountState accountState) noexcept
    {
        return detail::WebAccountStateMap.at(accountState);
    }

    inline std::wstring to_wstring(WebAccountState accountState) noexcept
    {
        return to_wstring(detail::WebAccountStateMap.at(accountState));
    }

    inline std::string to_string(FindAllWebAccountsStatus status) noexcept
    {
        return detail::FindAllWebAccountsStatusMap.at(status);
    }

    inline std::wstring to_wstring(FindAllWebAccountsStatus status) noexcept
    {
        return to_wstring(detail::FindAllWebAccountsStatusMap.at(status));
    }

    inline std::string to_string(WebTokenRequestStatus status) noexcept
    {
        return detail::WebTokenRequestStatusMap.at(status);
    }

    inline std::wstring to_wstring(WebTokenRequestStatus status) noexcept
    {
        return to_wstring(detail::WebTokenRequestStatusMap.at(status));
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
    /// Get file version as std::wstring
    /// </summary>
    /// <param name="filePath">Path to the file</param>
    /// <returns>version string as std::optional</returns>
    inline std::optional<std::wstring> GetFileVersion(const wchar_t* filePath) noexcept
    {
        const auto size = ::GetFileVersionInfoSize(filePath, nullptr);

        if (size == 0) {
            ::OutputDebugStringW(std::format(L"GetFileVersionInfoSize failed with {}\n", GetLastError()).c_str());
            return {};
        }

        const auto pData = std::make_unique<BYTE[]>(size);

        if (!::GetFileVersionInfoW(filePath, 0, size, pData.get()))
        {
            ::OutputDebugStringW(std::format(L"GetFileVersionInfoW failed with {}\n", GetLastError()).c_str());
            return {};
        }

        VS_FIXEDFILEINFO* pFileInfo{};
        auto fileInfoSize = UINT{};

        if (!::VerQueryValue(pData.get(), L"\\", (LPVOID*)&pFileInfo, &fileInfoSize))
        {
            ::OutputDebugStringW(std::format(L"VerQueryValue failed with {}\n", GetLastError()).c_str());
            return {};
        }

        const auto major = (pFileInfo->dwFileVersionMS >> 16) & 0xffff;
        const auto minor = pFileInfo->dwFileVersionMS & 0xffff;
        const auto revision = (pFileInfo->dwFileVersionLS >> 16) & 0xffff;

        return std::format(L"{}.{}.{}", major, minor, revision);
    }

    inline std::expected<std::wstring, std::wstring> GetCurrentUserName()
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
            return name;
        }

        return std::unexpected<std::wstring>{ std::format(L"GetUserNameExW failed with {:#x}", static_cast<std::uint32_t>(ec)) };
    }

    template <std::invocable F>
    struct DtorAction
    {
        [[nodiscard]] explicit DtorAction(F f) : action{ f } {}
        ~DtorAction() { action(); };
        F action;
    };
}
