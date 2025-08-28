add_executable(test
  main.cpp
  # sha256.cpp
  database/database.cpp
)

target_include_directories(test
  PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

find_package(OpenSSL REQUIRED)
target_link_libraries(test PRIVATE sqlite3 OpenSSL::Crypto)