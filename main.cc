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
#include "seastar/http/common.hh"

#include <bsoncxx/json.hpp>

#include "stop_signal.hh"

#include <memory>
#include <string_view>
// #include <google/protobuf/

#include "GLOBALS.h"
#include "DB/DBClients.h"
#include "DB/MongoInterface.h"

using namespace std;
using namespace seastar;
using namespace seastar::httpd;

// sudo /home/6wingSeraph/SIXW_SS_NEXUS/seastar/scripts/seastar-addr2line -e /home/6wingSeraph/SIXW_SS_NEXUS/build/NEXUS
// sudo mongod --dbpath /var/lib/mongo --port 27017 --bind_ip 0.0.0.0

enum class mongo_class
{
    db,
    collection,
    document,
    index
};

enum class crud_operation
{
    CREATE,
    READ,
    UPDATE,
    DELETE
};

crud_operation get_operation_type(const sstring& operation_str)
{
    if (operation_str == "create")
    {
        return crud_operation::CREATE;
    }
    else if (operation_str == "read")
    {
        return crud_operation::READ;
    }
    else if (operation_str == "update")
    {
        return crud_operation::UPDATE;
    }
    else if (operation_str == "delete")
    {
        return crud_operation::DELETE;
    }
    else
    {
        throw std::runtime_error("Invalid operation type: " + operation_str);
    }
}

mongo_class get_mongo_class(const sstring& mongo_class_str)
{
    if (mongo_class_str == "db")
    {
        return mongo_class::db;
    }
    else if (mongo_class_str == "collection")
    {
        return mongo_class::collection;
    }
    else if (mongo_class_str == "document")
    {
        return mongo_class::document;
    }
    else if (mongo_class_str == "index")
    {
        return mongo_class::index;
    }
    else
    {
        throw std::runtime_error("Invalid mongo class type: " + mongo_class_str);
    }
}

class crud_handler : public httpd::handler_base
{
    public:
    virtual future<std::unique_ptr<http::reply>> handle(const sstring& path, std::unique_ptr<http::request> req, std::unique_ptr<http::reply> rep) override
    {
        TRY;

        if (req->_method != "POST")
        {
            rep->set_status(http::reply::status_type::bad_request);
            rep->done("json");
            co_return std::move(rep);
        }

        auto uid = req->get_query_param("uid");
        auto operation = get_operation_type(req->get_query_param("operation"));
        auto mongo_class = get_mongo_class(req->get_query_param("class"));
        bsoncxx::document::view_or_value doc = bsoncxx::from_json(req->content);

        LG.info("Received uid: {}", uid);
        LG.info("Received operation: {}", req->get_query_param("operation"));
        LG.info("Received class: {}", req->get_query_param("class"));
        LG.info("Received json: {}", bsoncxx::to_json(doc));


        bsoncxx::document::view_or_value result_doc;
        switch (operation)
        {
            case crud_operation::CREATE:
                {
                    switch (mongo_class)
                    {
                        case mongo_class::db:
                            {
                                co_await MongoDBInterface::createDB(doc.view()["name"].get_string());
                                break;
                            }
                        case mongo_class::collection:
                            {
                                co_await MongoDBInterface::createCollection(
                                    doc.view()["dbname"].get_string(),
                                    doc.view()["collectionname"].get_string(),
                                    doc.view().find("collection_options")->get_document().view()
                                );
                                break;
                            }
                        case mongo_class::document:
                            {
                                co_await MongoDBInterface::createDocument(
                                    doc.view()["dbname"].get_string(),
                                    doc.view()["collectionname"].get_string(),
                                    doc.view()["json_data"].get_document().view()
                                );
                                break;
                            }
                        case mongo_class::index:
                            {
                                co_await MongoDBInterface::createIndex(
                                    doc.view()["dbname"].get_string(),
                                    doc.view()["collectionname"].get_string(),
                                    doc.view()["keys"].get_document().view(),
                                    doc.view().find("index_options")->get_document().view()
                                );
                                break;
                            }
                    }
                    break;
                }
            case crud_operation::READ:
                {
                    switch (mongo_class)
                    {
                        case mongo_class::db:
                            {
                                auto res = co_await MongoDBInterface::readDB(doc.view()["name"].get_string(), doc.view()["list_all"].get_bool());
                                if (res) { result_doc = *res; }
                                break;
                            }
                        case mongo_class::collection:
                            {
                                auto res = co_await MongoDBInterface::readCollection(
                                    doc.view()["dbname"].get_string(),
                                    doc.view()["collectionname"].get_string(),
                                    doc.view()["list_all"].get_bool()
                                );
                                if (res) { result_doc = *res; }
                                break;
                            }
                        case mongo_class::document:
                            {
                                auto res = co_await MongoDBInterface::readDocument(
                                    doc.view()["dbname"].get_string(),
                                    doc.view()["collectionname"].get_string(),
                                    doc.view()["filter"].get_document().view(),
                                    doc.view()["options"].get_document().view()
                                );
                                if (res) { result_doc = *res; }
                                break;
                            }
                        case mongo_class::index:
                            {
                                auto res = co_await MongoDBInterface::readIndex(
                                    doc.view()["dbname"].get_string(),
                                    doc.view()["collectionname"].get_string(),
                                    doc.view()["indexname"].get_string()
                                );
                                if (res) { result_doc = *res; }
                                break;
                            }
                    }
                    break;
                }
            case crud_operation::UPDATE:
                {
                    switch (mongo_class)
                    {
                        case mongo_class::db:
                            {
                                throw std::runtime_error("Cannot update a database");
                            }
                        case mongo_class::collection:
                            {
                                co_await MongoDBInterface::updateCollection(
                                    doc.view()["dbname"].get_string(),
                                    doc.view()["oldname"].get_string(),
                                    doc.view()["newname"].get_string()
                                );
                                break;
                            }
                        case mongo_class::document:
                            {
                                co_await MongoDBInterface::updateDocument(
                                    doc.view()["dbname"].get_string(),
                                    doc.view()["collectionname"].get_string(),
                                    doc.view()["updmany"].get_bool(),
                                    doc.view()["filter"].get_document().view(),
                                    doc.view()["fields_toupd"].get_document().view()
                                );
                                break;
                            }
                        case mongo_class::index:
                            {
                                throw std::runtime_error("Cannot update an index");
                            }
                    }
                    break;
                }
            case crud_operation::DELETE:
                {
                    switch (mongo_class)
                    {
                        case mongo_class::db:
                            {
                                co_await MongoDBInterface::deleteDB(doc.view()["name"].get_string());
                                break;
                            }
                        case mongo_class::collection:
                            {
                                co_await MongoDBInterface::deleteCollection(
                                    doc.view()["dbname"].get_string(),
                                    doc.view()["collectionname"].get_string()
                                );
                                break;
                            }
                        case mongo_class::document:
                            {
                                co_await  MongoDBInterface::deleteDocument(
                                    doc.view()["dbname"].get_string(),
                                    doc.view()["collectionname"].get_string(),
                                    doc.view()["delmany"].get_bool(),
                                    doc.view()["filter"].get_document().view()
                                );
                                break;
                            }
                        case mongo_class::index:
                            {
                                co_await MongoDBInterface::deleteIndex(
                                    doc.view()["dbname"].get_string(),
                                    doc.view()["collectionname"].get_string(),
                                    doc.view()["indexname"].get_string()
                                );
                                break;
                            }
                    }
                    break;
                }
        }
        rep->_content = bsoncxx::to_json(result_doc);
        rep->_headers["Content-Type"] = "application/json";

        rep->done("json");

        // co_await seastar::sleep(100ms);
        co_return std::move(rep);
        CATCH
        {
            LG_ERROR("Error in crud_handler: {}", std::current_exception());
            rep->set_status(http::reply::status_type::internal_server_error);
            rep->done("json");
            co_return std::move(rep);
        }
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
    co_await server->listen(uint16_t(6666), lo);

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