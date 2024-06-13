#pragma once
#include <fmt/core.h>
#include <seastar/util/log.hh>
#include <seastar/core/app-template.hh>

using namespace std;

inline seastar::logger LG("SIXW_SS_REST");

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