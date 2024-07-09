#include <mongocxx/instance.hpp>
#include <mongocxx/v_noabi/mongocxx/client.hpp>
#include <mongocxx/v_noabi/mongocxx/uri.hpp>

#include <mutex>
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
#include "MACROS.h"
#include "bsoncxx/document/view-fwd.hpp"
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
/*
sudo bash -c 'echo 264624 > /proc/sys/fs/aio-max-nr'
sudo bash -c 'echo "fs.aio-max-nr = 264624" >> /etc/sysctl.conf'
sudo bash -c 'echo 1 > /proc/sys/kernel/perf_event_paranoid'
sudo bash -c 'echo "kernel.perf_event_paranoid = 1" >> /etc/sysctl.conf'

*/
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

class sharded_mutex
{
    private:
    seastar::sharded<std::mutex> _mutexes;

    public:
    sharded_mutex() {}

    seastar::future<> start()
    {
        return _mutexes.start();
    }

    seastar::future<> lock()
    {
        _mutexes.local().lock();
        co_return;
    }

    seastar::future<> unlock()
    {
        _mutexes.local().unlock();
        co_return;
    }
};

class sharded_lock
{
    private:
    sharded_mutex& _mutex;

    public:
    sharded_lock(sharded_mutex& mutex) : _mutex(mutex)
    {
        _mutex.lock().get();
    }

    ~sharded_lock()
    {
        _mutex.unlock().get();
    }
};

inline sharded_mutex _mutex;

class crud_handler : public httpd::handler_base
{
    public:
    virtual future<std::unique_ptr<http::reply>> handle(const sstring& path, std::unique_ptr<http::request> req, std::unique_ptr<http::reply> rep) override
    {
        // sharded_lock lock(_mutex);
        bsoncxx::document::view_or_value result_doc = make_document(kvp("error", "No result"));
        TRY;

        if (req->_method != "POST")
        {
            rep->set_status(http::reply::status_type::bad_request);
            rep->done("json");
            co_return std::move(rep);
        }

        size_t json_size_bytes = req->content.length();
        double json_size_mb = static_cast<double>(json_size_bytes) / (1024 * 1024);

        LOG_DURATION("Operation took: " + req->get_query_param("operation") + ", Class: " + req->get_query_param("class") + ", JSON size: " + std::to_string(json_size_mb) + " MB");
        LG.info("Operation: {}, Class: {}, Json: {}", req->get_query_param("operation"), req->get_query_param("class"), req->content/* , "" */);

        // auto uid = req->get_query_param("uid");
        auto operation = get_operation_type(req->get_query_param("operation"));
        auto mongo_class = get_mongo_class(req->get_query_param("class"));
        bsoncxx::document::view_or_value doc;
        TRY;
        doc = bsoncxx::from_json(req->content);
        CATCH
        {
            LG_ERROR("Error in {} : {}", "from_json", fmt::format("{}",std::current_exception()));
        }

            switch (operation)
            {
                case crud_operation::CREATE:
                    {
                        switch (mongo_class)
                        {
                            case mongo_class::db:
                                {
                                    TRY;
                                    co_await MongoDBInterface::createDB(doc.view()["name"].get_string());
                                    result_doc = make_document(kvp("result", "ok"));
                                    CATCH
                                    {
                                        LG_ERROR("Error in {} : {}", "createDB", fmt::format("{}",std::current_exception()));
                                        result_doc = make_document(kvp("error", fmt::format("{}",std::current_exception())));
                                    }
                                    break;
                                }
                            case mongo_class::collection:
                                {
                                    TRY;
                                    co_await MongoDBInterface::createCollection(
                                        doc.view()["dbname"].get_string(),
                                        doc.view()["collectionname"].get_string(),
                                        doc.view()["collection_options"] ? doc.view()["collection_options"].get_document().view() : bsoncxx::document::view({})
                                    );
                                    result_doc = make_document(kvp("result", "ok"));
                                    CATCH
                                    {
                                        LG_ERROR("Error in {} : {}", "createCollection", fmt::format("{}",std::current_exception()));
                                        result_doc = make_document(kvp("error", fmt::format("{}",std::current_exception())));
                                    }
                                    break;
                                }
                            case mongo_class::document:
                                {
                                    document::view_or_value json_data = [&] () -> document::view_or_value
                                        {
                                            if (doc.view()["json_data"].type() == bsoncxx::type::k_document)
                                            {
                                                return doc.view()["json_data"].get_document().view();
                                            }
                                            else if (doc.view()["json_data"].type() == bsoncxx::type::k_array)
                                            {
                                                return make_document(kvp("json_data", doc.view()["json_data"].get_array()));
                                            }
                                            else if (doc.view()["json_data"].type() == bsoncxx::type::k_string)
                                            {
                                                return bsoncxx::from_json(doc.view()["json_data"].get_string());
                                            }
                                            else
                                            {
                                                throw std::runtime_error("Invalid json_data type");
                                            }
                                        }();
                                    TRY;
                                    co_await MongoDBInterface::createDocument(
                                        doc.view()["dbname"].get_string(),
                                        doc.view()["collectionname"].get_string(),
                                        json_data
                                    );
                                    result_doc = make_document(kvp("result", "ok"));
                                    CATCH
                                    {
                                        LG_ERROR("Error in {} : {}", "createDocument", fmt::format("{}",std::current_exception()));
                                        result_doc = make_document(kvp("error", fmt::format("{}",std::current_exception())));
                                    }
                                    break;
                                }
                            case mongo_class::index:
                                {
                                    TRY;
                                    co_await MongoDBInterface::createIndex(
                                        doc.view()["dbname"].get_string(),
                                        doc.view()["collectionname"].get_string(),
                                        doc.view()["keys"].get_document().view(),
                                        doc.view().find("index_options")->get_document().view()
                                    );
                                    result_doc = make_document(kvp("result", "ok"));
                                    CATCH
                                    {
                                        LG_ERROR("Error in {} : {}", "createIndex", fmt::format("{}",std::current_exception()));
                                        result_doc = make_document(kvp("error", fmt::format("{}",std::current_exception())));
                                    }
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
                                    TRY;
                                    auto res = co_await MongoDBInterface::readDB(doc.view()["name"].get_string(), doc.view()["list_all"].get_bool());
                                    if (res) { result_doc = make_document(kvp("result", "ok"), kvp("data", *res)); }
                                    if (res) { LG.debug("{}", bsoncxx::to_json(*res)); }
                                    CATCH
                                    {
                                        LG_ERROR("Error in {} : {}", "readDB", fmt::format("{}",std::current_exception()));
                                        result_doc = make_document(kvp("error", fmt::format("{}",std::current_exception())));
                                    }
                                    break;
                                }
                            case mongo_class::collection:
                                {
                                    TRY;
                                    auto res = co_await MongoDBInterface::readCollection(
                                        doc.view()["dbname"].get_string(),
                                        doc.view()["collectionname"].get_string(),
                                        doc.view()["list_all"].get_bool()
                                    );
                                    if (res) { result_doc = make_document(kvp("result", "ok"), kvp("data", *res)); }
                                    if (res) { LG.debug("{}", bsoncxx::to_json(*res)); }
                                    CATCH
                                    {
                                        LG_ERROR("Error in {} : {}", "readCollection", fmt::format("{}",std::current_exception()));
                                        result_doc = make_document(kvp("error", fmt::format("{}",std::current_exception())));
                                    }
                                    break;
                                }
                            case mongo_class::document:
                                {
                                    TRY;
                                    auto res = co_await MongoDBInterface::readDocument(
                                        doc.view()["dbname"].get_string(),
                                        doc.view()["collectionname"].get_string(),
                                        doc.view()["filter"] ? doc.view()["filter"].get_document().view() : bsoncxx::document::view({}),
                                        doc.view()["options"] ? doc.view()["options"].get_document().view() : bsoncxx::document::view({})
                                    );
                                    if (res) { result_doc = make_document(kvp("result", "ok"), kvp("data", *res)); }
                                    if (res) { LG.debug("{}", bsoncxx::to_json(*res)); }
                                    CATCH
                                    {
                                        LG_ERROR("Error in {} : {}", "readDocument", fmt::format("{}",std::current_exception()));
                                        result_doc = make_document(kvp("error", fmt::format("{}",std::current_exception())));
                                    }
                                    break;
                                }
                            case mongo_class::index:
                                {
                                    TRY;
                                    auto res = co_await MongoDBInterface::readIndex(
                                        doc.view()["dbname"].get_string(),
                                        doc.view()["collectionname"].get_string(),
                                        doc.view()["indexname"].get_string()
                                    );
                                    if (res) { result_doc = make_document(kvp("result", "ok"), kvp("data", *res)); }
                                    if (res) { LG.debug("{}", bsoncxx::to_json(*res)); }
                                    CATCH
                                    {
                                        LG_ERROR("Error in {} : {}", "readIndex", fmt::format("{}",std::current_exception()));
                                        result_doc = make_document(kvp("error", fmt::format("{}",std::current_exception())));
                                    }
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
                                    result_doc = make_document(kvp("error", "Cannot update a database"));
                                    throw std::runtime_error("Cannot update a database");
                                }
                            case mongo_class::collection:
                                {
                                    TRY;
                                    co_await MongoDBInterface::updateCollection(
                                        doc.view()["dbname"].get_string(),
                                        doc.view()["oldname"].get_string(),
                                        doc.view()["newname"].get_string()
                                    );
                                    result_doc = make_document(kvp("result", "ok"));
                                    CATCH
                                    {
                                        LG_ERROR("Error in {} : {}", "updateCollection", fmt::format("{}",std::current_exception()));
                                        result_doc = make_document(kvp("error", fmt::format("{}",std::current_exception())));
                                    }
                                    break;
                                }
                            case mongo_class::document:
                                {
                                    TRY;
                                    co_await MongoDBInterface::updateDocument(
                                        doc.view()["dbname"].get_string(),
                                        doc.view()["collectionname"].get_string(),
                                        doc.view()["updmany"].get_bool(),
                                        doc.view()["filter"].get_document().view(),
                                        doc.view()["fields_toupd"].get_document().view()
                                    );
                                    result_doc = make_document(kvp("result", "ok"));
                                    CATCH
                                    {
                                        LG_ERROR("Error in {} : {}", "updateDocument", fmt::format("{}",std::current_exception()));
                                        result_doc = make_document(kvp("error", fmt::format("{}",std::current_exception())));
                                    }
                                    break;
                                }
                            case mongo_class::index:
                                {
                                    result_doc = make_document(kvp("error", "Cannot update an index"));
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
                                    TRY;
                                    co_await MongoDBInterface::deleteDB(doc.view()["dbname"].get_string());
                                    result_doc = make_document(kvp("result", "ok"));
                                    CATCH
                                    {
                                        LG_ERROR("Error in {} : {}", "deleteDB", fmt::format("{}",std::current_exception()));
                                        result_doc = make_document(kvp("error", fmt::format("{}",std::current_exception())));
                                    }
                                    break;
                                }
                            case mongo_class::collection:
                                {
                                    TRY;
                                    co_await MongoDBInterface::deleteCollection(
                                        doc.view()["dbname"].get_string(),
                                        doc.view()["collectionname"].get_string()
                                    );
                                    result_doc = make_document(kvp("result", "ok"));
                                    CATCH
                                    {
                                        LG_ERROR("Error in {} : {}", "deleteCollection", fmt::format("{}",std::current_exception()));
                                        result_doc = make_document(kvp("error", fmt::format("{}",std::current_exception())));
                                    }
                                    break;
                                }
                            case mongo_class::document:
                                {
                                    TRY;
                                    co_await  MongoDBInterface::deleteDocument(
                                        doc.view()["dbname"].get_string(),
                                        doc.view()["collectionname"].get_string(),
                                        doc.view()["delmany"].get_bool(),
                                        doc.view()["filter"].get_document().view()
                                    );
                                    result_doc = make_document(kvp("result", "ok"));
                                    CATCH
                                    {
                                        LG_ERROR("Error in {} : {}", "deleteDocument", fmt::format("{}",std::current_exception()));
                                        result_doc = make_document(kvp("error", fmt::format("{}",std::current_exception())));
                                    }
                                    break;
                                }
                            case mongo_class::index:
                                {
                                    TRY;
                                    co_await MongoDBInterface::deleteIndex(
                                        doc.view()["dbname"].get_string(),
                                        doc.view()["collectionname"].get_string(),
                                        doc.view()["indexname"].get_string()
                                    );
                                    result_doc = make_document(kvp("result", "ok"));
                                    CATCH
                                    {
                                        LG_ERROR("Error in {} : {}", "deleteIndex", fmt::format("{}",std::current_exception()));
                                        result_doc = make_document(kvp("error", fmt::format("{}",std::current_exception())));
                                    }
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
            auto exception = std::current_exception();
            LG_ERROR("Error in crud_handler: {}\n{}", exception, seastar::current_backtrace_tasklocal());

            result_doc = make_document(kvp("error", fmt::format("{}",exception)));
            // rep->set_status(http::reply::status_type::internal_server_error, bsoncxx::to_json(result_doc));
            rep->_headers["Content-Type"] = "application/json";
            rep->_content = bsoncxx::to_json(result_doc);
            rep->done("json");

            co_return std::move(rep);
        }
    }
};

inline seastar::sharded<crud_handler> crud_handlers;
inline auto server = std::make_unique<http_server_control>();

void create_routes(seastar::httpd::routes& routes, seastar::sharded<crud_handler>& crud_handlers)
{
    routes.add(operation_type::POST, url("/crud").remainder("path"), &crud_handlers.local());
}

seastar::future<> main_fut()
{
    TRY;
    seastar_apps_lib::stop_signal stop_signal;
    LG.set_level(seastar::log_level::trace);
    // #ifdef NEXUS_DEBUG
    //     engine().update_blocked_reactor_notify_ms(10s);
    // #else
    engine().update_blocked_reactor_notify_ms(15s);
    // #endif
    // reactor::test::set_stall_detector_report_function([]
    //     {
    //         LG.info("Stall detected");
    //     });

    mongocxx::instance inst{};
    co_await db_clients.Init();

    co_await _mutex.start();

    co_await crud_handlers.start();

    co_await server->start();

    co_await server->set_routes([] (routes& r) { create_routes(r, crud_handlers); });

    seastar::listen_options lo;
    lo.reuse_address = false;
    lo.lba = server_socket::load_balancing_algorithm::round_robin;
    co_await server->listen(uint16_t(6665), lo);

    co_await stop_signal.wait();
    co_await server->stop();
    co_await crud_handlers.stop();
    co_return;
    CATCH
    {
        LG_ERROR("Error in main_fut: {}", fmt::format("{}",std::current_exception()));
        co_return;
    }
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