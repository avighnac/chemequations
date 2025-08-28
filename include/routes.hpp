#pragma once

#include <database.hpp>
#include <sock.hpp>

#define add_route(name) void name(const sock::socket::request &req, sock::socket::response &res)

// security
// std::string sha256(std::string_view input);
// std::string make_session_id();

// db stuff
database get_db();

namespace chemapp {
// session checker
// bool session_required(const sock::socket::request &req, sock::socket::response &res, int &id);

// routes

} // namespace chemapp

#undef add_route