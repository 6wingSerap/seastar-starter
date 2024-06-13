#include <mongocxx/instance.hpp>
#include <mongocxx/v_noabi/mongocxx/client.hpp>
#include <mongocxx/v_noabi/mongocxx/uri.hpp>

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
#include "DB/DBClients.h"
#include "DB/MongoInterface.h"

using namespace std;
using namespace seastar;
using namespace seastar::httpd;

// sudo /home/6wingSeraph/SIXW_SS_NEXUS/seastar/scripts/seastar-addr2line -e /home/6wingSeraph/SIXW_SS_NEXUS/build/NEXUS
// sudo mongod --dbpath /var/lib/mongo --port 27017 --bind_ip 0.0.0.0

class crud_handler : public httpd::handler_base
{
    public:
    virtual future<std::unique_ptr<http::reply>> handle(const sstring& path, std::unique_ptr<http::request> req, std::unique_ptr<http::reply> rep) override
    {
        if (req->_method == "POST")
        {
            auto uid = req->get_query_param("uid");
            auto operation = req->get_query_param("operation");
            auto mongo_class = req->get_query_param("class");

            // Выполнение CRUD операции в зависимости от параметров
            if (operation == "create")
            {
                if (mongo_class == "db")
                {
                    // Создание базы данных
                    LG.info("Creating database {}", uid);
                }
                else if (mongo_class == "collection")
                {
                    // Создание коллекции
                    LG.info("Creating collection {}", uid);
                }
                else if (mongo_class == "document")
                {
                    // Создание документа
                    LG.info("Creating document {}", uid);
                }
                else if (mongo_class == "index")
                {
                    // Создание индекса
                    LG.info("Creating index {}", uid);
                }
            }
            else if (operation == "read")
            {
                // Аналогично для операции чтения
                LG.info("Reading {}", uid);
            }
            else if (operation == "update")
            {
                // Аналогично для операции обновления
                LG.info("Updating {}", uid);
            }
            else if (operation == "delete")
            {
                // Аналогично для операции удаления
                LG.info("Deleting {}", uid);
            }

            // Формирование ответа в формате JSON
            // rep->_content = json::stream_object();
            // Добавление необходимых полей в ответ

            rep->done("json");
        }
        co_await seastar::sleep(100ms);
        co_return std::move(rep);
        // return make_ready_future<std::unique_ptr<http::reply>>(std::move(rep));
    }
};

seastar::sharded<crud_handler> crud_handlers;

void create_routes(seastar::httpd::routes& routes, seastar::sharded<crud_handler>& crud_handlers)
{
    routes.add(operation_type::POST, url("/crud").remainder("path"), &crud_handlers.local());
}

seastar::future<> main_fut()
{
    seastar_apps_lib::stop_signal stop_signal;
    LG.set_level(seastar::log_level::trace);

    mongocxx::instance inst{};
    co_await db_clients.Init();

    seastar::sharded<crud_handler> crud_handlers;
    co_await crud_handlers.start();

    auto server = std::make_unique<http_server_control>();
    co_await server->start();

    co_await server->set_routes([&crud_handlers] (routes& r) { create_routes(r, crud_handlers); });

    seastar::listen_options lo;
    lo.reuse_address = false;
    lo.lba = server_socket::load_balancing_algorithm::round_robin;
    co_await server->listen(uint16_t(10000), lo);

    co_await stop_signal.wait();
    co_await server->stop();
    co_await crud_handlers.stop();
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


#ifdef DOXYGEN
seastar::future<> handle_connection(seastar::connected_socket s)
{
    auto out = s.output();
    auto in = s.input();
    return do_with(std::move(s), std::move(out), std::move(in),
        [] (auto& s, auto& out, auto& in)
        {
            return seastar::repeat([&out, &in]
                {
                    return in.read().then([&out] (auto buf)
                        {
                            if (buf)
                            {
                                LG.info("Received: {}", buf.get());
                                return out.write(std::move(buf)).then([&out]
                                    {
                                        return out.flush();
                                    }).then([]
                                        {
                                            return seastar::stop_iteration::no;
                                        });
                            }
                            else
                            {
                                return seastar::make_ready_future<seastar::stop_iteration>(seastar::stop_iteration::yes);
                            }
                        });
                }).then([&out]
                    {
                        return out.close();
                    });
        });
}

seastar::future<> service_loop()
{
    seastar::listen_options lo;
    lo.reuse_address = false;
    return seastar::do_with(seastar::listen(seastar::make_ipv4_address({ 10000 }), lo),
        [] (auto& listener)
        {
            return seastar::keep_doing([&listener] ()
                {
                    return listener.accept().then([] (seastar::accept_result res)
                        {
                            // Note we ignore, not return, the future returned by
                            // handle_connection(), so we do not wait for one
                            // connection to be handled before accepting the next one.
                            (void) handle_connection(std::move(res.connection)).handle_exception(
                                [] (std::exception_ptr ep)
                                {
                                    fmt::print(stderr, "Could not handle connection: {}\n", ep);
                                });
                        });
                });
        });
}



seastar::future<> main_fut()
{
    // seastar_apps_lib::stop_signal stop_signal;
    LG.set_level(seastar::log_level::trace);

    mongocxx::instance inst{};
    co_await db_clients.Init();

    co_await seastar::parallel_for_each(boost::irange<unsigned>(0, seastar::smp::count),
        [] (unsigned c)
        {
            return seastar::smp::submit_to(c, service_loop);
        });
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
#endif