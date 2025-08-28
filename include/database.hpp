// lightweight sqlite3 cpp wrapper

#pragma once

#include <iostream>
#include <optional>
#include <sqlite3.h>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <vector>

class database {
  sqlite3 *db;

public:
  class statement {
    sqlite3_stmt *stmt;

  public:
    statement(sqlite3 *db, const std::string &sql) {
      if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error(sqlite3_errmsg(db));
      }
    }
    ~statement() { sqlite3_finalize(stmt); }

    template <typename... Args>
    void bind(Args &&...args) {
      int idx = 1;
      (bind_one(idx, std::forward<Args>(args)), ...);
    }

    template <typename... Args>
    void run(Args &&...args) {
      bind(std::forward<Args>(args)...);
      if (sqlite3_step(stmt) != SQLITE_DONE) {
        throw std::runtime_error(sqlite3_errmsg(sqlite3_db_handle(stmt)));
      }
      sqlite3_reset(stmt);
    }

    template <typename... Ts, typename... Args>
    std::optional<std::tuple<Ts...>> get(Args &&...args) {
      bind(std::forward<Args>(args)...);
      if (sqlite3_step(stmt) == SQLITE_ROW) {
        auto row = read_row<Ts...>();
        sqlite3_reset(stmt);
        return row;
      }
      sqlite3_reset(stmt);
      return std::nullopt;
    }

    template <typename... Ts, typename... Args>
    std::vector<std::tuple<Ts...>> all(Args &&...args) {
      std::vector<std::tuple<Ts...>> rows;
      bind(std::forward<Args>(args)...);
      while (sqlite3_step(stmt) == SQLITE_ROW) {
        rows.push_back(read_row<Ts...>());
      }
      sqlite3_reset(stmt);
      return rows;
    }

  private:
    void bind_one(int &idx, int v) {
      sqlite3_bind_int(stmt, idx++, v);
    }
    void bind_one(int &idx, sqlite3_int64 v) {
      sqlite3_bind_int64(stmt, idx++, v);
    }
    void bind_one(int &idx, double v) {
      sqlite3_bind_double(stmt, idx++, v);
    }
    void bind_one(int &idx, const std::string &v) {
      sqlite3_bind_text(stmt, idx++, v.c_str(), -1, SQLITE_TRANSIENT);
    }
    void bind_one(int &idx, const char *v) {
      sqlite3_bind_text(stmt, idx++, v, -1, SQLITE_TRANSIENT);
    }
    void bind_one(int &idx, std::nullptr_t) {
      sqlite3_bind_null(stmt, idx++);
    }
    template <typename T>
    void bind_one(int &idx, const std::vector<T> &vec) {
      for (auto &i : vec) {
        bind_one(idx, i);
      }
    }

    template <typename... Ts>
    std::tuple<Ts...> read_row() {
      return read_row_impl<Ts...>(std::index_sequence_for<Ts...>{});
    }

    template <typename... Ts, size_t... Is>
    std::tuple<Ts...> read_row_impl(std::index_sequence<Is...>) {
      return {get_column<Ts>(Is)...};
    }

    template <typename T>
    T get_column(int col) {
      if constexpr (std::is_same_v<T, int>) {
        return sqlite3_column_int(stmt, col);
      } else if constexpr (std::is_same_v<T, sqlite3_int64>) {
        return sqlite3_column_int64(stmt, col);
      } else if constexpr (std::is_same_v<T, double>) {
        return sqlite3_column_double(stmt, col);
      } else if constexpr (std::is_same_v<T, std::string>) {
        const unsigned char *txt = sqlite3_column_text(stmt, col);
        return txt ? reinterpret_cast<const char *>(txt) : "";
      } else if constexpr (std::is_same_v<T, bool>) {
        return sqlite3_column_int(stmt, col) != 0;
      } else {
        static_assert(!sizeof(T), "unsupported column type");
      }
    }
  };

  database(const std::string &filename) {
    if (sqlite3_open(filename.c_str(), &db) != SQLITE_OK) {
      throw std::runtime_error(sqlite3_errmsg(db));
    }
  }
  ~database() { sqlite3_close(db); }

  statement prepare(const std::string &sql) { return statement(db, sql); }
};