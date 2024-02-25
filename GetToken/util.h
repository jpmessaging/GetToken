#pragma once
#include "pch.h"

// Header-only util library
using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Security::Credentials;
using namespace Windows::Security::Authentication::Web::Core;


namespace Util::details
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
    inline std::string to_string(WebAccountState accountState)
    {
        return details::WebAccountStateMap.at(accountState);
    }

    inline std::string to_string(FindAllWebAccountsStatus status)
    {
        return details::FindAllWebAccountsStatusMap.at(status);
    }

    inline std::string to_string(WebTokenRequestStatus status)
    {
        return details::WebTokenRequestStatusMap.at(status);
    }

    inline std::string to_string(std::wstring_view str)
    {
        if (str.empty())
        {
            return {};
        }

        auto cb = WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), nullptr, 0, nullptr, nullptr);
        auto utf8 = std::string(cb, 0);
        WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), utf8.data(), cb, nullptr, nullptr);

        return utf8;
    }

    inline std::wstring to_wstring(std::string_view str)
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
}
