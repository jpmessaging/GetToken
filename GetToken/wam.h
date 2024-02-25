#pragma once

namespace WAM
{
    namespace ClientId
    {
        // Client IDs
        // https://learn.microsoft.com/en-us/troubleshoot/azure/active-directory/verify-first-party-apps-sign-in
        // inline to workaround ODR
        inline extern constexpr auto MSOFFICE = L"d3590ed6-52b3-4102-aeff-aad2292ab01c";
        inline extern constexpr auto OFFICE365_EXO = L"00000002-0000-0ff1-ce00-000000000000";
    }

    namespace ProviderId
    {
        // Providers & Authority constants
        // https://learn.microsoft.com/en-us/windows/uwp/security/web-account-manager
        inline extern constexpr auto AAD = L"https://login.windows.net";
        inline extern const auto LOCAL = L"https://login.windows.local";
        inline extern const auto MICROSOFT = L"https://login.microsoft.com"; // For Microsoft Account & Azure Active Directory
    }

    namespace Authority
    {
        inline constexpr auto CONSUMER = L"consumers";
        inline constexpr auto ORGANIZATION = L"organizations";
    }

    namespace Scopes
    {
        // Note: scopes are space-delimited strings:
        // https://datatracker.ietf.org/doc/html/rfc6749#section-3.3
        /*
            The value of the scope parameter is expressed as a list of space - delimited, case-sensitive strings.
            scope       = scope-token *( SP scope-token )
        */
        inline constexpr auto DEFAULT_SCOPES = L"https://outlook.office365.com//.default offline_access openid profile";
    }
}