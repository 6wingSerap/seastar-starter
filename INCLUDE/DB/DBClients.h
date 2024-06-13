#pragma once

#include <mongocxx/instance.hpp>
#include <mongocxx/v_noabi/mongocxx/client.hpp>
#include <mongocxx/v_noabi/mongocxx/uri.hpp>

#include <seastar/core/sharded.hh>
#include <seastar/core/smp.hh>
#include <seastar/coroutine/all.hh>

#include <memory>

#include "GLOBALS.h"
#include "MACROS.h"

using namespace std;
using namespace mongocxx::v_noabi;
using bsoncxx::builder::basic::kvp;
using bsoncxx::builder::basic::make_array;
using bsoncxx::builder::basic::make_document;

typedef mongocxx::v_noabi::client client_t;

// sudo mongod --dbpath /var/lib/mongo --port 27017 --bind_ip 0.0.0.0

inline string GetMongoDBURI()
{
    const char* uri_env = std::getenv("MONGODB_URI");
    if (uri_env)
    {
        LG.info("Using MONGODB_URI: {}", uri_env);
        return uri_env;
    }
    LG.info("Using default mongodb uri");
    // return "mongodb://localhost:27017/";
    return "mongodb://localhost:27017";
}

class DBClients
{
    std::shared_ptr<mongocxx::uri> uri = std::make_shared<mongocxx::uri>(GetMongoDBURI());

    public:
    bool failed_to_init = false;
    bool initialized = false;

    operator bool() const
    {
        return !failed_to_init;
    }

    void Failed()
    {
        failed_to_init = true;
    }

    seastar::sharded<client_t> clients;

    seastar::future<client_t&> operator()()
    {
        co_return clients.local();
    }

    seastar::future<> Init()
    {
        LOG_DURATION("Started mongodb clients");
        co_await clients.start().then_wrapped([this] (seastar::future<> f) -> seastar::future<>
            {
                if (f.failed())
                {
                    LG.error("Failed to start mongodb client: {}", f.get_exception());
                    failed_to_init = true;
                    co_return;
                }
                else
                {
                    co_return co_await clients.invoke_on_all([this] (client_t& client)
                        {
                            client = mongocxx::client(*uri);
                        }).then_wrapped([this] (seastar::future<> f) -> seastar::future<>
                            {
                                if (f.failed())
                                {
                                    LG.error("Failed to start mongodb client: {}", f.get_exception());
                                    failed_to_init = true;
                                    co_return;
                                }
                                co_return co_await clients.invoke_on_all([this] (client_t& client)
                                    {
                                        mongocxx::database db = client["CONN_INIT"];
                                        unique_ptr<collection> coll;
                                        if (!db.has_collection("CONN_LOG"))
                                        {
                                            coll = make_unique<collection>(db.create_collection("CONN_LOG"));
                                        }
                                        else
                                        {
                                            coll = make_unique<collection>(db["CONN_LOG"]);
                                            static bool cleared = false;
                                            if (!cleared && coll)
                                            {
                                                coll->delete_many({});
                                                cleared = true;
                                            }
                                        }

                                        if (!coll)
                                        {
                                            LG.error("Failed to create collection");
                                        }

                                        coll->insert_one(make_document(kvp("SHARD_ID", to_string(seastar::this_shard_id()))));
                                    });
                            });
                }
            });

        initialized = true;

        // co_return co_await bot.on_db_clients_ready_s.call({ failed_to_init });
    }

    DBClients(bool should_init = true)
    {
        if (should_init)
        {
            Init().get();
        }
    }

    ~DBClients()
    {
        clients.stop().then_wrapped([this] (seastar::future<> f)
            {
                if (f.failed())
                {
                    LG.error("Failed to stop mongodb client: {}", f.get_exception());
                }
            }).get();
        LG.info("Stopped mongodb clients");
    }
};

// unique_ptr<DBClients> db_clients = nullptr;

inline DBClients db_clients(false);