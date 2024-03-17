#pragma once

#include <print>
#include <string>

#ifndef WIN32_MEAN_AND_LEAN
#define WIN32_MEAN_AND_LEAN
#endif

#include <Windows.h>

#include "util.h"

namespace Console
{
    namespace detail
    {
        inline auto savedCP = UINT{};

        // State of Virtual Terminal
        inline auto vtEnabled = false;

        inline bool SetVirtualTerminal(bool enable)
        {
            // No need to close this handle
            if (auto hStdOut = ::GetStdHandle(STD_OUTPUT_HANDLE); hStdOut != INVALID_HANDLE_VALUE)
            {
                if (auto mode = DWORD{}; ::GetConsoleMode(hStdOut, &mode))
                {
                    auto newMode = enable ? mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING : mode & ~ENABLE_VIRTUAL_TERMINAL_PROCESSING;

                    if (::SetConsoleMode(hStdOut, newMode))
                    {
                        detail::vtEnabled = enable;
                        return true;
                    }
                }
            }

            return false;
        }
    }

    inline auto Init()
    {
        detail::savedCP = ::GetConsoleOutputCP();
        ::SetConsoleOutputCP(CP_UTF8);
    }

    inline auto Uninit()
    {
        if (detail::savedCP != 0)
        {
            ::SetConsoleOutputCP(detail::savedCP);
        }
    }

    /// <summary>
    /// Enable Virtual Terminal
    /// </summary>
    /// <returns>true if successful</returns>
    inline bool EnableVirtualTerminal()
    {
        return detail::SetVirtualTerminal(true);
    }

    /// <summary>
    /// Disable Virtual Terminal
    /// </summary>
    /// <returns>true if successful</returns>
    inline bool DisableVirtualTerminal()
    {
        return detail::SetVirtualTerminal(false);
    }

    /// <summary>
    /// See the following article for VT:
    /// Console Virtual Terminal Sequence
    /// https://learn.microsoft.com/en-us/windows/console/console-virtual-terminal-sequences
    /// </summary>
    enum class Format
    {
        Default = 0,      // Returns all attributes to the default state prior to modification
        Bright,           // Applies brightness/intensity flag to foreground color
        NoBright = 22,    // Removes brightness/intensity flag from foreground color
        Unerline = 4,     // Adds underline
        NoUnderline = 24, // Removes underline
        Negative = 7,     // Swaps foreground and background colors
        Positive = 27,    // Returns foreground/background to normal
        ForegroundBlack = 30,
        ForegroundRed,
        ForegroundGreen,
        ForegroundYellow,
        ForegroundBlue,
        ForegroundMagenta,
        ForegroundCyan,
        ForegroundWhite,
        // Extended = 38
        ForegroundDefault = 39,
        BackgroundBlack = 40,
        BackgroundRed,
        BackgroundGreen,
        BackgroundYellow,
        BackgroundBlue,
        BackgroundMagenta,
        BackgroundCyan,
        BackgroundWhite,
        // Extended = 48
        BackGroundDefault = 49,
    };

#define ESC "\x1b"
#define CSI ESC "[" // Control Sequence Introducer
#define OSC ESC "]" // Operating System Command

    /// <summary>
    /// Reset certain properties (Rendition, Charset, Cursor Keys Mode, etc.) to their default values
    /// </summary>
    inline void SoftReset()
    {
        std::print(CSI "!p");
    }

    /// <summary>
    /// Reset all attributes to the default state prior to modification
    /// </summary>
    inline void ResetFormat()
    {
        std::print(CSI "0m");
    }

    namespace detail {
        /// <summary>
        /// Get an underlying value of an enum type variable
        /// </summary>
        template <typename Enum>
        auto to_value(Enum const value) -> typename std::underlying_type<Enum>::type
        {
            return static_cast<typename std::underlying_type<Enum>::type>(value);
        }

        inline void Write(const std::initializer_list<Format>& consoleFormat, std::string_view text)
        {
            if (vtEnabled)
            {
                auto formatString = std::string{ CSI };

                for (const auto& fmt : consoleFormat)
                {
                    formatString.append(std::to_string(detail::to_value(fmt))).append(";");
                }

                // Replace the last ";" with "m"
                formatString.back() = 'm';

                std::print("{}{}", formatString, text);
                ResetFormat();
            }
            else
            {
                std::print("{}",text);
            }
        }
    }

    /*
      Following Write/WriteLine functions are bacially thin wrappers around std::print/println with optional Console::Format parameter.
      Overloaded for both char & wchar_t variants.
    */

    template <typename... Args>
    void Write(const std::format_string<Args...> format, Args&&... args)
    {
        // Just forward to std::print
        std::print(format, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void Write(const std::wformat_string<Args...> format, Args&&... args)
    {
        // std::print does not work on wchar_t (on MSVC), but std::format does.
        std::print("{}", Util::to_string(std::format(format, std::forward<Args>(args)...)));
    }

    template <typename... Args>
    void Write(const std::initializer_list<Format>& consoleFormat, const std::format_string<Args...> format, Args&&... args)
    {
        detail::Write(consoleFormat, std::format(format, std::forward<Args>(args)...));
    }

    template <typename... Args>
    void Write(const std::initializer_list<Format>& consoleFormat, const std::wformat_string<Args...> format, Args&&... args)
    {
        detail::Write(consoleFormat, Util::to_string(std::format(format, std::forward<Args>(args)...)));
    }

    template <typename... Args>
    void WriteLine(const std::format_string<Args...> format, Args&&... args)
    {
        // Just forward to std::println
        std::println(format, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void WriteLine(const std::wformat_string<Args...> format, Args&&... args)
    {
        // std::println does not work on wchar_t (on MSVC), but std::format does.
        std::println("{}", Util::to_string(std::format(format, std::forward<Args>(args)...)));
    }

    template <typename... Args>
    void WriteLine(const std::initializer_list<Format>& consoleFormat, const std::format_string<Args...> format, Args&&... args)
    {
        detail::Write(consoleFormat, std::format(format, std::forward<Args>(args)...));
        std::println("");
    }

    template <typename... Args>
    void WriteLine(const std::initializer_list<Format>& consoleFormat, const std::wformat_string<Args...> format, Args&&... args)
    {
        detail::Write(consoleFormat, Util::to_string(std::format(format, std::forward<Args>(args)...)));
        std::println("");
    }

     /*
     Note: I could consolidate to a single template with char type as a type parameter like this:

     template<typename CharT, typename... Args>
     void Write(const std::basic_format_string<CharT, std::type_identity_t<Args>...> format, Args&&... args)
     {
         if constexpr (std::is_same_v<CharT, char>)
         {
             // Just forward to std::println
             std::print(format, std::forward<Args>(args)...);
         }
         else
         {
             // std::println does not work on wchar_t (on MSVC), but std::format does.
             std::println("{}", Util::to_string(std::format(format, std::forward<Args>(args)...)));
         }
     }

     But then the caller would need to specify the char type like this:

     Console::Write<char>("hello {}", "world");
     Console::Write<wchar_t>(L"hello {}", L"world");

     Since this is cumbersome, I overloaded with std::format_string & std::wformat_string
    */
}

#undef ESC
#undef CSI
#undef OSC