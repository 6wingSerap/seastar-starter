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

// class NEXUS_REACTOR_OPTS : public seastar::reactor_options
// {
//     seastar::program_options::value<unsigned> blocked_reactor_notify_ms;
// };

class NEXUS_APP : public seastar::app_template
{
    public:
    struct config
    {
        seastar::sstring name = "NEXUS_APP";
    };

    struct seastar_options : public seastar::program_options::option_group
    {
        seastar::sstring name = "NEXUS_APP";
        // seastar::reactor_options reactor_opts;
    };
};

inline NEXUS_APP app;
