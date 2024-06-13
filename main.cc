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
        return make_ready_future<std::unique_ptr<http::reply>>(std::move(rep));
    }
};

class CrudHandlers
{

    seastar::sharded<crud_handler> crud_handlers;

    seastar::future<> Init()
    {
        
    }
};

void create_routes(seastar::httpd::routes& routes)
{
    using namespace seastar;
    using namespace httpd;

    routes.add(operation_type::POST, url("/crud").remainder("path"), new crud_handler());
}

seastar::future<> main_fut()
{
    seastar_apps_lib::stop_signal stop_signal;
    LG.set_level(seastar::log_level::trace);

    mongocxx::instance inst{};
    co_await db_clients.Init();

    auto server = std::make_unique<http_server_control>();
    // auto rb = seastar::make_shared<api_registry_builder>("apps/nexus/");
    co_await server->start();

    auto stop_server = defer([&] () noexcept
        {
            std::cout << "Stoppping HTTP server" << std::endl; // This can throw, but won't.
            server->stop().get();
        });

    co_await server->set_routes(create_routes);
    co_await server->listen(uint16_t(10000));

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
