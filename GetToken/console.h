#pragma once

#include <string>
#include <print>

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
    namespace details
    {
        /// <summary>
        /// Get a underlying value of an enum type variable
        /// </summary>
        template <typename Enum>
        auto to_value(Enum const value) -> typename std::underlying_type<Enum>::type
        {
            return static_cast<typename std::underlying_type<Enum>::type>(value);
        }
    }

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

        if (!GetConsoleMode(hStdOut, &mode))
        {
            return false;
        }

        if (!SetConsoleMode(hStdOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING))
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

        if (!GetConsoleMode(hStdOut, &mode))
        {
            return false;
        }

        if (!SetConsoleMode(hStdOut, mode & ~ENABLE_VIRTUAL_TERMINAL_PROCESSING))
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

    inline void Write(std::string_view text, const std::initializer_list<Format>& formatList)
    {
        auto formatString = std::string{ CSI };

        for (const auto& fmt : formatList)
        {
            formatString.append(std::to_string(details::to_value(fmt))).append(";");
        }

        // Replace the last ";" with "m"
        formatString.back() = 'm';

        std::print("{}{}", formatString, text);
        ResetFormat();
    }

    inline void WriteLine(std::string_view text, const std::initializer_list<Format>& formatList)
    {
        Write(text, formatList);
        std::println("");
    }

    template<typename... Args>
    void Write(std::string_view text, Args... args)
    {
        Write(text, { args... });
    }

    template<typename... Args>
    void WriteLine(std::string_view text, Args... args)
    {
        WriteLine(text, { args... });
    }
}

#undef ESC
#undef CSI ESC "["
#undef OSC ESC "]"