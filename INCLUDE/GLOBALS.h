#pragma once
#include <fmt/core.h>
#include <seastar/util/log.hh>
#include <seastar/core/app-template.hh>

using namespace std;

inline seastar::logger LG("SIXW_SS_REST");
#define LG_ERROR(msg, ...) LG.error("[{}-{}] | " msg, __LINE__, __FILE_NAME__, ##__VA_ARGS__)
#define LG_WARN(msg, ...) LG.warn("[{}-{}] | " msg, __LINE__, __FILE_NAME__, ##__VA_ARGS__)
#define LG_INFO(msg, ...) LG.info("[{}-{}] | " msg, __LINE__, __FILE_NAME__, ##__VA_ARGS__)
#define LG_DEBUG(msg, ...) LG.debug("[{}-{}] | " msg, __LINE__, __FILE_NAME__, ##__VA_ARGS__)
#define LG_TRACE(msg, ...) LG.trace("[{}-{}] | " msg, __LINE__, __FILE_NAME__, ##__VA_ARGS__)

template <typename E>
bool is_exception_of_type(const std::exception_ptr& e) noexcept
{
    try
    {
        if (e)
        {
            std::rethrow_exception(e);
        }
    }
    catch (const E&)
    {
        return true;
    }
    catch (...)
    {
        return false;
    }
    return false;
}

inline seastar::app_template app;