#include <seastar/core/app-template.hh>
#include <seastar/core/coroutine.hh>
#include <seastar/core/sharded.hh>
#include <seastar/core/sleep.hh>
#include <seastar/core/thread.hh>
#include <seastar/http/httpd.hh>
#include <seastar/http/handlers.hh>
#include <seastar/http/function_handlers.hh>
#include <seastar/http/file_handler.hh>
#include <seastar/core/seastar.hh>
#include <seastar/core/reactor.hh>

#include <seastar/http/api_docs.hh>
#include <seastar/core/thread.hh>
#include <seastar/core/prometheus.hh>
#include <seastar/core/print.hh>
#include <seastar/net/inet_address.hh>
#include <seastar/util/defer.hh>
#include "stop_signal.hh"

#include <memory>
// #include <google/protobuf/

#include "GLOBALS.h"

using namespace std;
namespace ss = seastar;
using namespace std::chrono_literals;


ss::future<> infinite_loop()
{
    return ss::keep_doing([] ()
        {
            return ss::sleep(100s);
        });
}

seastar::future<> main_fut()
{
    seastar_apps_lib::stop_signal stop_signal;

    LG.set_level(seastar::log_level::trace);

    co_await stop_signal.wait();
    co_return;
}

int main(int ac, char** av)
{
    std::set_terminate(__gnu_cxx::__verbose_terminate_handler);

    return app.run(ac, av, [&]
        {
            return seastar::async([&] ()
                {
                    main_fut().get();
                });
        });
}


