#pragma once
#ifndef TRACE_HPP
#define TRACE_HPP

#include <chrono>
#include <filesystem>
#include <future>
#include <optional>
#include <string>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <agents.h>
#undef NOMINMAX
#undef WIN32_LEAN_AND_MEAN

#include "util.h"

namespace Diagnostics::detail
{
    struct TraceData
    {
        DWORD ThreadId = 0;
        std::chrono::system_clock::time_point Time;
        std::wstring Message;

        explicit TraceData(const std::wstring_view message)
            : ThreadId{ GetCurrentThreadId() }, Time{ std::chrono::system_clock::now() }, Message{ message }
        {}
    };

    class CsvTracer
    {
    public:
        explicit CsvTracer(const std::filesystem::path& filePath);
        ~CsvTracer();
        auto Write(std::wstring_view message) -> void;

    private:
        auto WriteAllMessages() -> void;
        Concurrency::unbounded_buffer<std::shared_ptr<TraceData>> _buffer;
        std::future<void> _writeTask;
        Concurrency::cancellation_token_source _cts;
        std::unique_ptr<void, decltype(&CloseHandle)> _hFile;
        std::unique_ptr<void, decltype(&CloseHandle)> _hBuffReady;
    };

#pragma region CsvTracer member definitions
    inline CsvTracer::CsvTracer(const std::filesystem::path& filePath) :
        _hFile{ nullptr, CloseHandle },
        _hBuffReady{ CreateEventW(nullptr, false, false, nullptr), CloseHandle }
    {
        _hFile.reset(::CreateFileW(filePath.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));

        if (INVALID_HANDLE_VALUE == _hFile.get())
        {
            throw std::system_error{ static_cast<int>(GetLastError()), std::system_category(), std::format("Failed to open {}", Util::to_string(filePath.c_str())) };
        }

        // Start a consumer
        _writeTask = std::async(
            std::launch::async,
            [this, cancelToken = _cts.get_token()]() {
                while (true) {
                    WaitForSingleObject(_hBuffReady.get(), INFINITE);
                    WriteAllMessages();
                    if (cancelToken.is_canceled())
                    {
                        return;
                    }
                }
            });

        // Write the header;
        auto header = Util::to_string(L"date-time,thread-id,message\n");
        auto writtenCount = DWORD{};
        ::WriteFile(_hFile.get(), header.data(), static_cast<DWORD>(header.size()), &writtenCount, nullptr);
    }

    inline CsvTracer::~CsvTracer()
    {
        try {
            // Stop consumer
            _cts.cancel();
            SetEvent(_hBuffReady.get());

            // Write the rest of the messages in the buffer
            WriteAllMessages();
        }
        catch (...)
        {
            // Ignore exception to comply with (implicit) noexcept specifier
        }
    }

    inline auto CsvTracer::Write(std::wstring_view message) -> void
    {
        Concurrency::send(_buffer, std::make_shared<TraceData>(message));
        SetEvent(_hBuffReady.get());
    }

    inline auto CsvTracer::WriteAllMessages() -> void
    {
        auto data = std::shared_ptr<TraceData>{};
        auto writtenCount = DWORD{};

        while (Concurrency::try_receive(_buffer, data))
        {
            auto& message = data->Message;

            // Replace double-quotation (") with a sigle-quotation (')
            for (auto& c : message)
            {
                if (c == '"')
                    c = '\'';
            }

            auto view = message.ends_with('\n') ?
                std::wstring_view{ message.begin(), message.end() - 1 } :
                std::wstring_view{ message.begin(), message.end() };

            auto formatted = std::format(L"{0:%F}T{0:%T%z},{1},\"{2}\"\n", data->Time, data->ThreadId, view);
            auto utf8Str = Util::to_string(formatted);

            ::WriteFile(_hFile.get(), utf8Str.data(), static_cast<DWORD>(utf8Str.size()), &writtenCount, nullptr);
        }
    }
#pragma endregion CsvTracer member definitions
} // end of namespace Diagnostics::detail

namespace Diagnostics::Trace
{
    inline auto Enabled() -> bool;
}

namespace Diagnostics::detail
{
    inline std::optional<CsvTracer> _tracer = std::nullopt;

    inline auto Write(std::wstring_view message) -> void
    {
        if (Diagnostics::Trace::Enabled())
        {
            _tracer->Write(message);
        }
    }

    inline auto Write(std::string_view message) -> void
    {
        if ((Diagnostics::Trace::Enabled()))
        {
            Write(Util::to_wstring(message));
        }
    }
}

namespace Diagnostics::Trace
{
        inline auto Enable(std::wstring_view path) -> void
        {
            if (detail::_tracer) 
                throw std::runtime_error{ std::format("Trace has been already initialized with {}", Util::to_string(path)) };

            detail::_tracer.emplace(path);
        }

        inline auto Disable() -> void
        {
            if (detail::_tracer)
                detail::_tracer.reset();
        }

        inline auto Enabled() -> bool
        {
            return detail::_tracer.has_value();
        }

        template <class... Args>
        void Write(const std::format_string<Args...> format, Args&&... args) 
        {
            Diagnostics::detail::Write(std::format(format, std::forward<Args>(args)...));
        }

        template <class... Args>
        void Write(const std::wformat_string<Args...> format, Args&&... args) {

            Diagnostics::detail::Write(std::format(format, std::forward<Args>(args)...));
        }
} // end of namespace Diagnostics::Trace

//namespace Diagnostics::detail
//{
//    inline std::optional<CsvTracer> _tracer = std::nullopt;
//
//    inline auto Write(std::wstring_view message) -> void
//    {
//        if (Diagnostics::Trace::Enabled())
//        {
//            _tracer->Write(message);
//        }
//    }
//
//    inline auto Write(std::string_view message) -> void
//    {
//        if ((::Trace::Enabled()))
//        {
//            Write(Util::to_wstring(message));
//        }
//    }
//}
#endif