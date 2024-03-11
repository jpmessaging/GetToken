#pragma once
#ifndef TRACE_HPP
#define TRACE_HPP

#include <chrono>
#include <filesystem>
#include <future>
#include <optional>
#include <string>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef  NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <agents.h>

#include "util.h"

namespace Diagnostics::detail
{
    struct TraceData
    {
        DWORD ThreadId = 0;
        std::chrono::system_clock::time_point Time;
        std::string Message;

        explicit TraceData(const std::string_view message)
            : ThreadId{ GetCurrentThreadId() }, Time{ std::chrono::system_clock::now() }, Message{ message }
        {}
    };

    class CsvTracer
    {
    public:
        explicit CsvTracer(const std::filesystem::path& filePath);
        ~CsvTracer();
        auto Write(std::string_view message) -> void;

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

    inline auto CsvTracer::Write(std::string_view message) -> void
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

            auto view = std::string_view{ message };

            if (view.ends_with('\n'))
            {
                view.remove_suffix(1);
            }

            auto formatted = std::format("{0:%F}T{0:%T%z},{1},\"{2}\"\n", data->Time, data->ThreadId, view);
            ::WriteFile(_hFile.get(), formatted.data(), static_cast<DWORD>(formatted.size()), &writtenCount, nullptr);
        }
    }
#pragma endregion CsvTracer member definitions

    inline std::optional<CsvTracer> _tracer = std::nullopt;

    inline auto Write(std::string_view message) -> void
    {
        if (_tracer)
        {
            _tracer->Write(message);
        }
    }

    inline auto Write(std::wstring_view message) -> void
    {
        if (_tracer)
        {
            _tracer->Write(Util::to_string(message));
        }
    }
} // end of namespace Diagnostics::detail

namespace Diagnostics::Trace
{
        inline void Enable(std::wstring_view path)
        {
            if (detail::_tracer)
            {
                throw std::runtime_error{ std::format("Trace has been already initialized with {}", Util::to_string(path)) };
            }

            detail::_tracer.emplace(path);
        }

        inline void Disable()
        {
            if (detail::_tracer)
            {
                detail::_tracer.reset();
            }
        }

        inline bool Enabled()
        {
            return detail::_tracer.has_value();
        }

        template <class... Args>
        void Write(const std::format_string<Args...> format, Args&&... args)
        {
            Diagnostics::detail::Write(std::format(format, std::forward<Args>(args)...));
        }

        template <class... Args>
        void Write(const std::wformat_string<Args...> format, Args&&... args)
        {
            Diagnostics::detail::Write(std::format(format, std::forward<Args>(args)...));
        }
} // end of namespace Diagnostics::Trace
#endif