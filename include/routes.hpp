#pragma once

#include <sock.hpp>

#define add_route(name) void name(const sock::socket::request &req, sock::socket::response &res)

namespace chemapp {
add_route(balance);
add_route(get_atoms);
} // namespace chemapp

#undef add_route