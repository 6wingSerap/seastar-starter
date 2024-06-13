#pragma once

#include "GLOBALS.h"
#include "EVENTS.h"
#include "seastar/core/sstring.hh"
#include <mongocxx/v_noabi/mongocxx/uri.hpp>

using namespace std;
using namespace chrono;
using namespace literals;
using namespace bsoncxx::v_noabi;
using seastar::sstring;


template<typename T>
using future_uptr = seastar::future<std::unique_ptr<T>>;

template <typename T>
using event_router_ptr = std::unique_ptr<event_router_RAII<T>>;
