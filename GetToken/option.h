#pragma once
#include "pch.h"
#include "util.h"

#define WINDOWS
#include "popl.hpp"
#undef WINDOWS

class Option
{
public:
    Option(int argc, char** argv) :
        m_parser{ "Available options" },
        m_help{ m_parser.add<popl::Switch>("h", "help", "Show this help message") },
        m_helpAlias{ m_parser.add<popl::Switch>("?", "", "Show this help message") },
        m_signOut{ m_parser.add<popl::Switch>("", "signout", "sign out of Web Accounts") },
        m_clientId{ m_parser.add<popl::Value<std::string>>("c", "clientid", std::format("Client ID. Default: {}", Util::to_string(WAM::ClientId::MSOFFICE))) },
        m_scopes{ m_parser.add<popl::Value<std::string>>("", "scopes", std::format(R"(requested scopes of the token. Default: "{}")", Util::to_string(WAM::Scopes::DEFAULT_SCOPES))) },
        m_properties { m_parser.add<popl::Value<std::string>>("p", "property", "Request property (e.g., longin_hint=user01@example.com, prompt=login)") }
    {
        m_parser.parse(argc, argv);
    }

    // No copy
    Option(const Option&) = delete;
    Option& operator=(const Option&) = delete;

    // Movable
    Option(Option&&) = default;
    Option& operator=(Option&&) = default;

    const auto& UnknownOptions() const
    {
        return m_parser.unknown_options();
    }

    auto Help() const
    {
        return m_help->value() || m_helpAlias->value();
    }

    auto SignOut() const
    {
        return m_signOut->value();
    }

    const std::optional<std::wstring>& ClientId() const
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

    const std::optional<std::wstring>& Scopes() const
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

    const std::unordered_map<std::wstring, const std::wstring>& Properties() const
    {
        // Parse properties and put them in a map
        // Property value should look like "key=value"
        static auto value = [this]() -> std::unordered_map<std::wstring, const std::wstring> {
            auto map = std::unordered_map<std::wstring, const std::wstring>{ m_properties->count() };

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

    void PrintHelp() const
    {
        // Get this executable file name
        static const auto exeName = []() {
            auto exePath = std::string(MAX_PATH, 0);
            auto cch = GetModuleFileNameA(nullptr, exePath.data(), static_cast<DWORD>(exePath.size()));
            exePath.resize(cch);
            return exePath.substr(exePath.rfind('\\') + 1);
        }();

        std::println("{}", m_parser.help());
        std::println("Note: All options are case insensitive\n");

        std::println("Example 1: {}", exeName);
        std::println("Run with default configurations\n");

        std::println("Example 2: {} --property login_hint=user01@example.com --property prompt=login --property resource=https://graph.windows.net", exeName);
        std::println("Add the given properties to the request\n");

        std::println("Example 3: {} -p login_hint=user01@example.com -p prompt=login -p resource=https://graph.windows.net", exeName);
        std::println("Same as Example 2, using the short option name -p\n");

        std::println("Example 4: {} --scopes open_id profiles", exeName);
        std::println("Use the given scopes for the token\n");

        std::println("Example 5: {} --signout", exeName);
        std::println("Sign out from all web accounts before making token requests\n");
    }

private:
    popl::OptionParser m_parser;

    std::shared_ptr<const popl::Switch> m_help;
    std::shared_ptr<const popl::Switch> m_helpAlias;
    std::shared_ptr<const popl::Switch> m_signOut;
    std::shared_ptr<const popl::Value<std::string>> m_clientId;
    std::shared_ptr<const popl::Value<std::string>> m_scopes;
    std::shared_ptr<const popl::Value<std::string>> m_properties;
};

