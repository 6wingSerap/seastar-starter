#pragma once

#include <seastar/coroutine/all.hh>
#include <seastar/coroutine/parallel_for_each.hh>
#include <seastar/core/coroutine.hh>
#include <seastar/core/future.hh>

#include <boost/type_index.hpp>

#include <memory.h>

#include "GLOBALS.h"

template <class EventT>
class event_router
{
    private:
    mutable std::shared_mutex lock;
    size_t next_handle = 1;

    std::unordered_map<size_t, std::function<seastar::future<>(const EventT&)>> dispatch_container;

    public:
    event_router() = default;

    seastar::future<> call(const EventT& event)
    {
        if (dispatch_container.empty())
        {
            co_return;
        }
        LOG_DURATION("Handling event {" + boost::typeindex::type_id_with_cvr<EventT>().pretty_name() + "}");
        co_return co_await seastar::coroutine::parallel_for_each(dispatch_container, [&event] (auto& pair)
            {
                return pair.second(event);
            });
    }

    seastar::future<size_t> attach(std::function<seastar::future<>(const EventT&)> handler)
    {
        std::unique_lock l(lock);
        size_t handle = next_handle++;
        dispatch_container.emplace(handle, handler);
        co_return handle;
    }

    seastar::future<size_t> operator()(std::function<seastar::future<>(const EventT&)> handler)
    {
        return this->attach(handler);
    }

    seastar::future<bool> detach(size_t handle)
    {
        std::unique_lock l(lock);
        bool erased = this->dispatch_container.erase(handle);
        if (erased)
        {
            next_handle--;
        }
        co_return erased;
    }
};

template <class EventT>
class event_router_RAII
{
    private:
    event_router<EventT>& router;
    size_t handle = -1;

    public:
    event_router_RAII(event_router<EventT>& router, std::function<seastar::future<>(const EventT&)> handler)
        : router(router)
    {
        try
        {
            handle = router.attach(handler).get();
            // LG.info("Attaching event handler with id: {}", handle);
        }
        catch (...)
        {
            LG.error("Failed to attach event handler: {}", std::current_exception());
            if (handle != -1)
            {
                router.detach(handle).get();
            }
        }
    }

    ~event_router_RAII()
    {
        // LG.info("Detaching event handler with id: {}", handle);
        router.detach(handle).get();
    }

    event_router_RAII(const event_router_RAII&) = delete;
    event_router_RAII& operator=(const event_router_RAII&) = delete;
};

class NEXUS
{
    
};