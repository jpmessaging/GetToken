#pragma once
#include "pch.h"

// #include "console.h"
#include "util.h"
#include "wam.h"

#define WINDOWS
#include "popl.hpp"
#undef WINDOWS

class Option final
{
public:
    Option() :
        m_parser{ "Available options" },
        m_help{ m_parser.add<popl::Switch>("h", "help", "Show this help message") },
        m_helpAlias{ m_parser.add<popl::Switch>("?", "", "Show this help message") },
        m_signOut{ m_parser.add<popl::Switch>("", "signout", "Sign out of Web Accounts") },
        m_clientId{ m_parser.add<popl::Value<std::string>>("c", "clientid", std::format("Client ID. Default: {}", Util::to_string(WAM::ClientId::MSOFFICE))) },
        m_scopes{ m_parser.add<popl::Value<std::string>>("", "scopes", std::format(R"(Requested scopes of the token. Default: "{}")", Util::to_string(WAM::Scopes::DEFAULT_SCOPES))) },
        m_properties{ m_parser.add<popl::Value<std::string>>("p", "property", "Request property (e.g., longin_hint=user01@example.com, prompt=login)") },
        m_tracePath{ m_parser.add<popl::Value<std::string>>("t", "tracepath", "Folder path for a trace file") },
        m_wait{ m_parser.add<popl::Switch>("w", "wait", "Wait execution until user enters") }

    { /* empty */ }

    Option(int argc, char** argv) : Option()
    {
        m_parser.parse(argc, argv);
    }

    // detor, copy, and move are compiler-generated default.

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

    auto SignOut() const
    {
        return m_signOut->value();
    }

    const std::optional<std::wstring>& ClientId() const noexcept
    {
        static auto value = [this]() -> std::optional<std::wstring> {
            if (m_clientId->is_set())
            {
                return Util::to_wstring(m_clientId->value());
            }

            return std::nullopt;
        }();

        return value;
    }

    const std::optional<std::wstring>& Scopes() const noexcept
    {
        static auto value = [this]() -> std::optional<std::wstring> {
            if (m_scopes->is_set())
            {
                return Util::to_wstring(m_scopes->value());
            }

            return std::nullopt;
        }();

        return value;
    }

    const std::unordered_map<std::wstring, std::wstring>& Properties() const
    {
        // Parse properties and put them in a map
        // Property value should look like "key=value"
        static auto value = [this]() -> std::unordered_map<std::wstring, std::wstring> {
            auto map = std::unordered_map<std::wstring, std::wstring>{ m_properties->count() };

            for (int i = 0; i < m_properties->count(); ++i)
            {
                const auto& propValue = m_properties->value(i);
                auto pos = propValue.find('=');

                if (pos != std::string::npos)
                {
                    auto key = propValue.substr(0, pos);
                    auto value = propValue.substr(pos + 1);
                    map.emplace(Util::to_wstring(key), Util::to_wstring(value));
                }
            }

            return map;
        }();

        return value;
    }

    const std::optional<std::wstring>& TracePath() const noexcept
    {
        static auto value = [this]() {
            auto value = std::optional<std::wstring>{};

            if (m_tracePath->is_set())
            {
                value = Util::to_wstring(m_tracePath->value());
            }

            return value;
        }();

        return value;
    }

    bool Wait() const noexcept
    {
        return m_wait->value();
    }

    void PrintHelp() const noexcept
    {
        // Get this executable file name
        static const auto exeName = []() {
            auto exePath = std::string(MAX_PATH, 0);
            auto cch = GetModuleFileNameA(nullptr, exePath.data(), static_cast<DWORD>(exePath.size()));
            exePath.resize(cch);
            return exePath.substr(exePath.rfind('\\') + 1);
        }();

        std::print("{}", m_parser.help());

        std::println(R"(
Note: All options are case insensitive.

Example 1: {0}
Run with default configurations

Example 2: {0} --property login_hint=user01@example.com --property prompt=login --property resource=https://graph.windows.net
Add the given properties to the request

Example 3: {0} -p login_hint=user01@example.com -p prompt=login -p resource=https://graph.windows.net
Same as Example 2, using the short option name -p

Example 4: {0} --scopes open_id profiles
Use the given scopes for the token

Example 5: {0} --signout
Sign out from all web accounts before making token requests
)", exeName);

    }

private:
    popl::OptionParser m_parser;

    std::shared_ptr<const popl::Switch> m_help;
    std::shared_ptr<const popl::Switch> m_helpAlias;
    std::shared_ptr<const popl::Switch> m_signOut;
    std::shared_ptr<const popl::Value<std::string>> m_clientId;
    std::shared_ptr<const popl::Value<std::string>> m_scopes;
    std::shared_ptr<const popl::Value<std::string>> m_properties;
    std::shared_ptr<const popl::Value<std::string>> m_tracePath;
    std::shared_ptr<const popl::Switch> m_wait;
};

