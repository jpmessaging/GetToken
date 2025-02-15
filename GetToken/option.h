#pragma once

#include <format>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

#include <winrt/base.h>

#include "popl.hpp"
#include "util.h"
#include "wam.h"

class Option final
{
public:
    Option() :
        m_parser{ "Available options" },
        m_help{ m_parser.add<popl::Switch>("h", "help", "Show this help message") },
        m_helpAlias{ m_parser.add<popl::Switch>("?", "", "Show this help message") },
        m_version{ m_parser.add<popl::Switch>("v", "version", "Show version") },
        m_clientId{ m_parser.add<popl::Value<std::string>>("c", "clientid", std::format("Client ID. Default: {}", Util::to_string(WAM::ClientId::MSOFFICE))) },
        m_scopes{ m_parser.add<popl::Value<std::string>>("", "scopes", std::format(R"(Scopes of the token. Default: "{}")", Util::to_string(WAM::Scopes::DEFAULT_SCOPES))) },
        m_properties{ m_parser.add<popl::Value<std::string>>("p", "property", "Request property (e.g., wam_compat=2.0, login_hint=user01@example.com, prompt=login). Can be used multiple times") },
        m_showAccounts{ m_parser.add<popl::Switch>("", "showaccounts", "Show Web Accounts and exit") },
        m_showToken{ m_parser.add<popl::Switch>("", "showToken", "Show Access Token") },
        m_signOut{ m_parser.add<popl::Switch>("", "signout", "Sign out of Web Accounts") },
        m_notrace{ m_parser.add<popl::Switch>("n", "notrace", "Disable trace" )},
        m_tracePath{ m_parser.add<popl::Value<std::string>>("t", "tracepath", "Folder path for a trace file") },
        m_wait{ m_parser.add<popl::Switch>("w", "wait", "Wait execution until user enters") }
    { /* empty */ }

    Option(int argc, char** argv) : Option()
    {
        m_parser.parse(argc, argv);
    }

    // dtor, copy, and move are compiler-generated default.

    void Parse(int argc, char** argv)
    {
        m_parser.parse(argc, argv);
    }

    const auto& UnknownOptions() const noexcept
    {
        return m_parser.unknown_options();
    }

    auto Help() const noexcept
    {
        return m_help->value() || m_helpAlias->value(); 
    }

    auto Version() const noexcept
    {
        return m_version->value();
    }

    auto SignOut() const noexcept
    {
        return m_signOut->value();
    }

    auto ShowAccounts() const noexcept
    {
        return m_showAccounts->value();
    }

    auto ShowToken() const noexcept
    {
        return m_showToken->value();
    }

    auto EnableTrace() const noexcept
    {
        // Trace is enabled by default, unless --notrace is specified
        return not m_notrace->value();
    }

    const std::optional<winrt::hstring>& ClientId() const noexcept
    {
        static auto value = [this]() -> std::optional<winrt::hstring> {
            if (m_clientId->is_set())
            {
                return winrt::to_hstring(m_clientId->value());
            }

            return std::nullopt;
        }();

        return value;
    }

    const std::optional<winrt::hstring>& Scopes() const noexcept
    {
        static auto value = [this]() -> std::optional<winrt::hstring> {
            if (m_scopes->is_set())
            {
                return winrt::to_hstring(m_scopes->value());
            }

            return std::nullopt;
        }();

        return value;
    }

    const std::unordered_map<winrt::hstring, winrt::hstring>& Properties() const
    {
        // Parse properties and put them in a map
        // Property value should look like "key=value"
        static auto value = [this]() {
            auto map = std::unordered_map<winrt::hstring, winrt::hstring>{ m_properties->count() };

            for (int i = 0; i < m_properties->count(); ++i)
            {
                const auto propValue = m_properties->value(i);
                auto pos = propValue.find('=');

                if (pos != std::string::npos)
                {
                    auto key = propValue.substr(0, pos);
                    auto val = propValue.substr(pos + 1);
                    map.emplace(winrt::to_hstring(key), winrt::to_hstring(val));
                }
            }

            return map;
        }();

        return value;
    }

    const std::optional<std::filesystem::path>& TracePath() const
    {
        static auto value = [this]() {
            auto path = std::optional<std::filesystem::path>{};

            if (m_tracePath->is_set())
            {
                path = m_tracePath->value();
            }

            return path;
        }();

        return value;
    }

    bool Wait() const noexcept
    {
        return m_wait->value();
    }

    std::string GetVersion() const
    {
        static auto ver = []() {
            const auto exePath = Util::GetModulePath(nullptr);
            const auto exeName = exePath.stem();
            auto version = Util::GetFileVersion(exePath);
            return std::format("{} (version {})", exeName.string(), version.value_or(""));
        }();

        return ver;
    }

    std::string GetHelp() const
    {
        static const auto exeName = []() {
            const auto exePath = Util::GetModulePath(nullptr);
            return exePath.filename().string();
        }();

        auto help = m_parser.help();
        help += std::format(R"(
Note: All options are case insensitive.

Example 1: {0}
Run with default configurations

Example 2: {0} --property login_hint=user01@example.com --property prompt=login --property resource=https://graph.windows.net
Add the given properties to the request

Example 3: {0} -p login_hint=user01@example.com -p prompt=login -p resource=https://graph.windows.net
Same as Example 2, using the short option name -p

Example 4: {0} -p wam_compat=2.0
Adding "wam_compat=2.0" gives you JWT (JSON Web Token).

Example 6: {0} --scopes open_id profiles
Use the given scopes for the token

Example 7: {0} --signout
Sign out from all web accounts before making token requests
)", exeName);

        return help;
    }

private:
    popl::OptionParser m_parser;

    // Help message shows the available options as listed here (so, the order matters here).
    std::shared_ptr<const popl::Switch> m_help;
    std::shared_ptr<const popl::Switch> m_helpAlias;
    std::shared_ptr<const popl::Switch> m_version;

    std::shared_ptr<const popl::Value<std::string>> m_clientId;
    std::shared_ptr<const popl::Value<std::string>> m_scopes;
    std::shared_ptr<const popl::Value<std::string>> m_properties;

    std::shared_ptr<const popl::Switch> m_showAccounts;
    std::shared_ptr<const popl::Switch> m_showToken;
    std::shared_ptr<const popl::Switch> m_signOut;
    std::shared_ptr<const popl::Value<std::string>> m_tracePath;
    std::shared_ptr<const popl::Switch> m_notrace;
    std::shared_ptr<const popl::Switch> m_wait;
};