#pragma once

#include <chrono>
#include <iostream>
#include <utility>

#include "GLOBALS.h"

using namespace std;
using namespace chrono;
using namespace literals;

// Макрос для начала блока try
#define TRY try {

// Макрос для блока catch, который возвращает future с исключением
#define SEASTAR_CATCH } catch (...) { co_return co_await seastar::make_exception_future<>(std::current_exception()); }
#define CATCH } catch (...)
// #define SEASTAR_CATCH } catch (...) { return seastar::make_exception_future<>(std::current_exception()); }

#define LG_ERROR(msg, ...) LG.error("[{}-{}] | " msg, __LINE__, __FILE_NAME__, ##__VA_ARGS__)
#define LG_WARN(msg, ...) LG.warn("[{}-{}] | " msg, __LINE__, __FILE_NAME__, ##__VA_ARGS__)
#define LG_INFO(msg, ...) LG.info("[{}-{}] | " msg, __LINE__, __FILE_NAME__, ##__VA_ARGS__)
#define LG_DEBUG(msg, ...) LG.debug("[{}-{}] | " msg, __LINE__, __FILE_NAME__, ##__VA_ARGS__)
#define LG_TRACE(msg, ...) LG.trace("[{}-{}] | " msg, __LINE__, __FILE_NAME__, ##__VA_ARGS__)

#define MVCAPT(var) var = std::move(var)

#define PROFILE_CONCAT_INTERNAL(X, Y) X##Y
#define PROFILE_CONCAT(X, Y) PROFILE_CONCAT_INTERNAL(X, Y)
#define UNIQUE_VAR_NAME_PROFILE PROFILE_CONCAT(profileGuard, __LINE__)
#define LOG_DURATION(x) LogDuration UNIQUE_VAR_NAME_PROFILE(x)

class LogDuration
{
    public:
    LogDuration(const std::string&& id) noexcept : id_(std::move(id))
    {}

    ~LogDuration()
    {
        const auto end_time = steady_clock::now();
        const auto dur = end_time - start_time_;
        const auto ms = duration_cast<microseconds>(dur).count();

        std::ostringstream os;
        os << "DUR: [" << id_ << "]: ";

        if (ms < 1000)
        {
            os << ms << " μs";
        }
        else if (ms < 1000000)
        {
            os << std::fixed << std::setprecision(2) << ms / 1000.0 << " ms";
        }
        else
        {
            os << std::fixed << std::setprecision(2) << ms / 1000000.0 << " s";
        }

        LG.info("{}", os.str());
    }

    private:
    const std::string id_;
    const steady_clock::time_point start_time_ = steady_clock::now();
};