#pragma once
#ifndef TRACE_HPP
#define TRACE_HPP

#include <chrono>
#include <filesystem>
#include <future>
#include <semaphore>
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

namespace Diagnostics::Trace::detail
{
    class ITracer
    {
    public:
        virtual ~ITracer() = default;
        virtual void Write(std::string_view) = 0;
    };

    struct TraceData
    {
        DWORD ThreadId = 0;
        std::chrono::system_clock::time_point Time;
        std::string Message;

        explicit TraceData(const std::string_view message)
            : ThreadId{ GetCurrentThreadId() }, Time{ std::chrono::system_clock::now() }, Message{ message }
        {}
    };

    class CsvTracer final : public ITracer
    {
    public:
        explicit CsvTracer(const std::filesystem::path& filePath);
        ~CsvTracer();
        void Write(std::string_view msg) override;

    private:
        void WriteAllMessages();
        auto Stream() noexcept -> std::ostream&;

        Concurrency::unbounded_buffer<std::shared_ptr<TraceData>> _buffer;
        std::future<void> _writeTask;
        Concurrency::cancellation_token_source _cts;
        std::ofstream _ofstream;
        std::binary_semaphore _buffSemaphore{ 0 };
    };

    inline CsvTracer::CsvTracer(const std::filesystem::path& filePath) :
        _ofstream{ filePath }
    {
        if (not _ofstream.good())
        {
            throw std::runtime_error{ std::format("Failed to open {}", Util::to_string(filePath.c_str())) };
        }

        // Start a consumer
        _writeTask = std::async(
            std::launch::async,
            [this, cancelToken = _cts.get_token()]() {
                while (true) {
                    _buffSemaphore.acquire();
                    WriteAllMessages();

                    if (cancelToken.is_canceled())
                    {
                        return;
                    }
                }
            });

        // Write the header
        std::println(Stream(), "date-time,thread-id,message");
    }

    inline CsvTracer::~CsvTracer()
    {
        // Stop consumer
        _cts.cancel();
        _buffSemaphore.release();

        // Wait for the consumer to exit
        _writeTask.wait();

        // Write the rest of the messages in the buffer if any
        WriteAllMessages();
    }

    inline void CsvTracer::Write(std::string_view message)
    {
        Concurrency::send(_buffer, std::make_shared<TraceData>(message));
        _buffSemaphore.release();
    }

    inline void CsvTracer::WriteAllMessages()
    {
        auto data = std::shared_ptr<TraceData>{};

        while (Concurrency::try_receive(_buffer, data))
        {
            // Replace all occurrences of double-quotation (") with a sigle-quotation (')
            // Not using std::execution::par here because it's actually faster with sequential processing.
            std::replace(begin(data->Message), end(data->Message), '"', '\'');

            // Create a view without the last newline char if any
            auto view = std::string_view{ data->Message };

            if (view.ends_with('\n'))
            {
                view.remove_suffix(1);
            }

            std::println(Stream(), "{0:%F}T{0:%T%z},{1},\"{2}\"", data->Time, data->ThreadId, view);
        }
    }

    inline auto CsvTracer::Stream() noexcept -> std::ostream&
    {
        return _ofstream;
    }

    // ITracer instance
    auto _tracer = std::unique_ptr<ITracer>{};
} // end of namespace Diagnostics::Trace::detail

namespace Diagnostics::Trace
{
    using detail::_tracer;
    using detail::CsvTracer;

    inline bool IsEnabled() noexcept
    {
        return !!detail::_tracer;
    }

    inline void Enable(std::filesystem::path path)
    {
        if (IsEnabled())
        {
            throw std::runtime_error{ "Trace has been already initialized" };
        }

        _tracer = std::make_unique<CsvTracer>(path);
    }

    inline void Disable() noexcept
    {
        if (IsEnabled())
        {
            _tracer.reset();
        }
    }

    template <class... Args>
    void Write(const std::format_string<Args...> format, Args&&... args)
    {
        if (IsEnabled())
        {
            _tracer->Write(std::format(format, std::forward<Args>(args)...));
        }
    }

    template <class... Args>
    void Write(const std::wformat_string<Args...> format, Args&&... args)
    {
        if (IsEnabled())
        {
            _tracer->Write(Util::to_string(std::format(format, std::forward<Args>(args)...)));
        }
    }
}
#endif