add_executable(test
  main.cpp
  routes/data.cpp
)

target_include_directories(test PRIVATE ${CMAKE_SOURCE_DIR}/include)