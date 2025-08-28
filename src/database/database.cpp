#include <cstdlib>
#include <database.hpp>

database get_db() {
  const char *path = std::getenv("DATABASE_PATH");
  if (!path) {
    throw std::runtime_error("DATABASE_PATH not set");
  }
  return database(path);
}