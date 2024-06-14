#pragma once

#include "DB/DBClients.h" 
#include "USINGS.h"

#include <seastar/core/sstring.hh>
#include <seastar/core/shared_ptr.hh>

#include "mongocxx/database-fwd.hpp"
#include <mongocxx/instance.hpp>

// using _json = bsoncxx::document::view_or_value

class MongoDBInterface
{
    public:
    static seastar::future<> createDB(const string_view db_name)
    {
        TRY;

        mongocxx::database db = (co_await db_clients())[db_name];
        db.create_collection("DummyCollection"sv);

        CATCH{ LG_ERROR("Failed to create database: {} exception: {}", db_name, std::current_exception()); }
    }

    static seastar::future<> createCollection(const string_view db_name, const string_view coll_name, const document::view_or_value& options = {})
    {
        TRY;

        mongocxx::database db = (co_await db_clients())[db_name];
        db.create_collection(coll_name, options);

        CATCH{ LG_ERROR("Failed to create collection: {} exception: {}", coll_name, std::current_exception()); }
    }

    static seastar::future<> createDocument(const string_view db_name, const string_view coll_name, const document::view_or_value& document = {})
    {
        TRY;

        mongocxx::database db = (co_await db_clients())[db_name];
        mongocxx::collection coll = db[coll_name];
        coll.insert_one(document);

        CATCH{ LG_ERROR("Failed to create document: {} exception: {}", coll_name, std::current_exception()); }
    }

    static seastar::future<seastar::lw_shared_ptr<document::view_or_value>> readDB(const string_view dbName, const bool list_all)
    {
        TRY;

        auto listeddbs = (co_await db_clients()).list_databases();

        bsoncxx::builder::basic::array array_builder;

        if (list_all)
        {
            for (auto&& db : listeddbs)
            {
                array_builder.append(db);
            }
        }
        else
        {
            bool found = false;
            for (auto&& db : listeddbs)
            {
                if (dbName == db["name"].get_string().value)
                {
                    array_builder.append(db);
                    found = true;
                    break;
                }
            }

            if (!found)
            {
                array_builder.append(bsoncxx::builder::basic::make_document(
                    bsoncxx::builder::basic::kvp("error", "Database not found")
                ));
            }
        }

        bsoncxx::builder::basic::document document_builder;
        document_builder.append(bsoncxx::builder::basic::kvp("databases", array_builder.extract()));

        co_return seastar::make_lw_shared<document::view_or_value>(document_builder.extract());

        CATCH{ LG_ERROR("Failed to read database: {} exception: {}", dbName, std::current_exception()); }
        co_return nullptr;
    }

    static seastar::future<seastar::lw_shared_ptr<document::view_or_value>> readCollection(const string_view dbName, const string_view collectionName, const bool listAll)
    {
        TRY;

        auto db = (co_await db_clients())[dbName];
        auto listedCollections = db.list_collections();

        bsoncxx::builder::basic::array arrayBuilder;

        if (listAll)
        {
            for (auto&& collection : listedCollections)
            {
                bsoncxx::builder::basic::document docBuilder;
                docBuilder.append(kvp("name", collection["name"].get_value()));
                docBuilder.append(kvp("type", collection["type"].get_value()));
                docBuilder.append(kvp("options", collection["options"].get_value()));
                docBuilder.append(kvp("info", collection["info"].get_value()));
                docBuilder.append(kvp("idIndex", collection["idIndex"].get_value()));
                arrayBuilder.append(docBuilder.extract());
            }
        }
        else
        {
            bool found = false;
            for (auto&& collection : listedCollections)
            {
                if (collectionName == collection["name"].get_string().value)
                {
                    bsoncxx::builder::basic::document docBuilder;
                    docBuilder.append(kvp("name", collection["name"].get_value()));
                    docBuilder.append(kvp("type", collection["type"].get_value()));
                    docBuilder.append(kvp("options", collection["options"].get_value()));
                    docBuilder.append(kvp("info", collection["info"].get_value()));
                    docBuilder.append(kvp("idIndex", collection["idIndex"].get_value()));
                    arrayBuilder.append(docBuilder.extract());
                    found = true;
                    break;
                }
            }

            if (!found)
            {
                arrayBuilder.append(make_document(kvp("error", "Collection not found")));
            }
        }

        bsoncxx::builder::basic::document documentBuilder;
        documentBuilder.append(kvp("collections", arrayBuilder.extract()));

        co_return seastar::make_lw_shared<document::view_or_value>(documentBuilder.extract());

        CATCH{ LG_ERROR("Failed to read collection: {} exception: {}", collectionName, std::current_exception()); }
        co_return nullptr;
    }

    static seastar::future<seastar::lw_shared_ptr<document::view_or_value>> readDocument(const string_view dbName, const string_view collectionName, const document::view_or_value& filter = {}, const document::view_or_value& options = {})
    {
        TRY;

        auto db = (co_await db_clients())[dbName];
        auto collection = db[collectionName];
        mongocxx::options::find opts{};

        if (options.view()["allow_disk_use"]) { opts.allow_disk_use(options.view()["allow_disk_use"].get_int32().value == 1); }

        if (options.view()["batch_size"]) { opts.batch_size(options.view()["batch_size"].get_int32().value); }

        if (options.view()["limit"]) { opts.limit(options.view()["limit"].get_int32().value); }
        // opts.hint()
        // TODO: больше настроек

        auto doc_cursor = collection.find(filter, opts);

        bsoncxx::builder::basic::array arrayBuilder;
        for (auto&& doc : doc_cursor)
        {
            arrayBuilder.append(doc);
        }

        bsoncxx::builder::basic::document documentBuilder;
        documentBuilder.append(kvp("documents", arrayBuilder.extract()));

        co_return seastar::make_lw_shared<document::view_or_value>(documentBuilder.extract());

        CATCH{ LG_ERROR("Failed to read document: {} exception: {}", collectionName, std::current_exception()); }
        co_return nullptr;
    }

    static seastar::future<> updateCollection(const string_view dbName, const string_view oldName, const string_view newName)
    {
        TRY;

        auto& client = co_await db_clients();
        auto db = (co_await db_clients())[dbName];
        db[oldName].rename(newName);

        CATCH{ LG_ERROR("Failed to update collection: {} exception: {}", oldName, std::current_exception()); }
    }

    static seastar::future<> updateDocument(const string_view dbName, const string_view collectionName, bool updmany = false, const document::view_or_value& filter = {}, const document::view_or_value& update = {})
    {
        TRY;

        auto db = (co_await db_clients())[dbName];
        auto collection = db[collectionName];

        if (!filter.view().empty() && !update.view().empty())
        {
            if (updmany)
            {
                collection.update_many(filter, update);
            }
            else
            {
                collection.update_one(filter, update);
            }
        }

        CATCH{ LG_ERROR("Failed to update document: {} exception: {}", collectionName, std::current_exception()); }
    }

    static seastar::future<> deleteDB(const string_view dbName)
    {
        TRY;

        (co_await db_clients())[dbName].drop();

        CATCH{ LG_ERROR("Failed to delete database: {} exception: {}", dbName, std::current_exception()); }
    }

    static seastar::future<> deleteCollection(const string_view dbName, const string_view collectionName)
    {
        TRY;

        (co_await db_clients())[dbName][collectionName].drop();

        CATCH{ LG_ERROR("Failed to delete collection: {} exception: {}", collectionName, std::current_exception()); }
    }

    static seastar::future<> deleteDocument(const string_view dbName, const string_view collectionName, bool delmany = false, const document::view_or_value& filter = {})
    {
        TRY;

        auto db = (co_await db_clients())[dbName];
        auto collection = db[collectionName];

        if (!filter.view().empty())
        {
            if (delmany)
            {
                collection.delete_many(filter);
            }
            else
            {
                collection.delete_one(filter);
            }
        }

        CATCH{ LG_ERROR("Failed to delete document: {} exception: {}", collectionName, std::current_exception()); }
    }

    static seastar::future<> createIndex(const string_view dbName, const string_view collectionName, const document::view_or_value& keys, const document::view_or_value& index_options = {})
    {
        TRY;

        auto db = (co_await db_clients())[dbName];
        auto collection = db[collectionName];

        if (!keys.view().empty())
        {
            collection.create_index(keys, index_options);
        }

        CATCH{ LG_ERROR("Failed to create index: {} exception: {}", collectionName, std::current_exception()); }
    }

    static seastar::future<seastar::lw_shared_ptr<document::view_or_value>> readIndex(const string_view dbName, const string_view collectionName, const string_view indexName = "")
    {
        TRY;

        auto db = (co_await db_clients())[dbName];
        auto collection = db[collectionName];
        auto indexes_cursor = collection.list_indexes(); // https://www.mongodb.com/docs/manual/reference/command/listIndexes/

        bsoncxx::builder::basic::array arrayBuilder;

        if (indexName == "")
        {
            for (auto&& index : indexes_cursor)
            {
                arrayBuilder.append(index);
            }
        }
        else
        {
            for (auto&& index : indexes_cursor)
            {
                if (indexName == index["name"].get_string().value)
                {
                    arrayBuilder.append(index);
                    break;
                }
            }
        }

        bsoncxx::builder::basic::document documentBuilder;
        documentBuilder.append(kvp("indexes", arrayBuilder.extract()));

        co_return seastar::make_lw_shared<document::view_or_value>(documentBuilder.extract());

        CATCH{ LG_ERROR("Failed to read index: {} exception: {}", collectionName, std::current_exception()); }
        co_return nullptr;
    }

    static seastar::future<> deleteIndex(const string_view dbName, const string_view collectionName, const string_view indexName)
    {
        TRY;

        auto db = (co_await db_clients())[dbName];
        auto collection = db[collectionName];

        collection.indexes().drop_one(indexName);

        CATCH{ LG_ERROR("Failed to delete index: {} exception: {}", collectionName, std::current_exception()); }
    }
};