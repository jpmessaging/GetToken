#pragma once

#include <print>
#include <string>

#include "util.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#undef WIN32_LEAN_AND_MEAN

/*
* See the following article for VT:
* Console Virtual Terminal Sequences
* https://learn.microsoft.com/en-us/windows/console/console-virtual-terminal-sequences
*/
namespace Console
{
    /// <summary>
    /// Enable Virtual Terminal
    /// </summary>
    /// <returns>bool true if successful</returns>
    inline bool EnableVirtualTerminal()
    {
        auto hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);

        if (hStdOut == INVALID_HANDLE_VALUE)
        {
            return false;
        }

        auto mode = DWORD{};

        if (not GetConsoleMode(hStdOut, &mode))
        {
            return false;
        }

        if (not SetConsoleMode(hStdOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING))
        {
            return false;
        }
    }

    /// <summary>
    /// Enable Virtual Terminal
    /// </summary>
    /// <returns>bool true if successful</returns>
    inline bool DisableVirtualTerminal()
    {
        auto hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);

        if (hStdOut == INVALID_HANDLE_VALUE)
        {
            return false;
        }

        auto mode = DWORD{};

        if (not GetConsoleMode(hStdOut, &mode))
        {
            return false;
        }

        if (not SetConsoleMode(hStdOut, mode & ~ENABLE_VIRTUAL_TERMINAL_PROCESSING))
        {
            return false;
        }
    }

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
#define CSI ESC "["
#define OSC ESC "]"

    /// <summary>
    /// Reset certain properties (Rendition, Charset, Cursor Keys Mode, etc.) to their default values
    /// </summary>
    inline void SoftReset()
    {
        std::print(CSI "!p");
    }

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

        void Write(const std::initializer_list<Format>& consoleFormat, std::string_view text)
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
    }

    /*
    * Following Write/WriteLine functions are bacially thin wrappers around std::print/println with optional Console::Format parameter.
    * Overloaded for both char & wchar_t variants.
    */

    template <class... Args>
    void Write(const std::format_string<Args...> format, Args&&... args) {
        // Just forward to std::print
        std::print(format, std::forward<Args>(args)...);
    }

    template <class... Args>
    void Write(const std::wformat_string<Args...> format, Args&&... args) {
        // std::print does not work on wchar_t (on MSVC), but std::format does.
        std::print("{}", Util::to_string(std::format(format, std::forward<Args>(args)...)));
    }

    template <class... Args>
    void Write(const std::initializer_list<Format>& consoleFormat, const std::format_string<Args...> format, Args&&... args) {
        detail::Write(consoleFormat, std::format(format, std::forward<Args>(args)...));
    }

    template <class... Args>
    void Write(const std::initializer_list<Format>& consoleFormat, const std::wformat_string<Args...> format, Args&&... args) {
        detail::Write(consoleFormat, Util::to_string(std::format(format, std::forward<Args>(args)...)));
    }

    template <class... Args>
    void WriteLine(const std::format_string<Args...> format, Args&&... args) {
        // Just forward to std::println
        std::println(format, std::forward<Args>(args)...);
    }

    template <class... Args>
    void WriteLine(const std::wformat_string<Args...> format, Args&&... args) {
        // std::println does not work on wchar_t (on MSVC), but std::format does.
        std::println("{}", Util::to_string(std::format(format, std::forward<Args>(args)...)));
    }

    template <class... Args>
    void WriteLine(const std::initializer_list<Format>& consoleFormat, const std::format_string<Args...> format, Args&&... args) {
        detail::Write(consoleFormat, std::format(format, std::forward<Args>(args)...));
        std::println("");
    }

    template <class... Args>
    void WriteLine(const std::initializer_list<Format>& consoleFormat, const std::wformat_string<Args...> format, Args&&... args) {
        detail::Write(consoleFormat, Util::to_string(std::format(format, std::forward<Args>(args)...)));
        std::println("");
    }


    //
    // Write version
    //
    //inline void Write(std::string_view text, const std::initializer_list<Format>& formatList = std::initializer_list<Format>{})
    //{
    //    auto formatString = std::string{ CSI };

    //    for (const auto& fmt : formatList)
    //    {
    //        formatString.append(std::to_string(detail::to_value(fmt))).append(";");
    //    }

    //    // Replace the last ";" with "m"
    //    formatString.back() = 'm';

    //    std::print("{}{}", formatString, text);
    //    ResetFormat();
    //}

    //template<typename... Args>
    //void Write(std::string_view text, Args... args)
    //{
    //    Write(text, { args... });
    //}

    //inline void WriteLine(std::string_view text, const std::initializer_list<Format>& formatList = std::initializer_list<Format>{})
    //{
    //    Write(text, formatList);
    //    std::println("");
    //}

    //template<typename... Args>
    //void WriteLine(std::string_view text, Args... args)
    //{
    //    WriteLine(text, { args... });
    //}

    ////
    //// Wide versions
    ////

    //inline void Write(std::wstring_view text, const std::initializer_list<Format>& formatList)
    //{
    //    Write(Util::to_string(text), formatList);
    //}

    //template<typename... Args>
    //void Write(std::wstring_view text, Args... args)
    //{
    //    Write(text, { args... });
    //}

    //inline void WriteLine(std::wstring_view text, const std::initializer_list<Format>& formatList)
    //{
    //    Write(text, formatList);
    //    std::println("");
    //}

    //template<typename... Args>
    //void WriteLine(std::wstring_view text, Args... args)
    //{
    //    WriteLine(text, {args...});
    //}
}

#undef ESC
#undef CSI ESC "["
#undef OSC ESC "]"