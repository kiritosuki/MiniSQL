cmake_minimum_required(VERSION 3.16)
project(MiniSQL VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(-Wall -Wextra -Wpedantic)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library(minisql_core
  src/engine.cpp
  src/sql_parser.cpp
  src/storage.cpp
  src/table.cpp
  src/util.cpp
)

add_executable(minisqld src/server_main.cpp src/net.cpp)
target_link_libraries(minisqld PRIVATE minisql_core)

add_executable(minisql src/client_main.cpp src/net.cpp)
target_link_libraries(minisql PRIVATE minisql_core)

add_executable(minisql_tests tests/minisql_tests.cpp)
target_link_libraries(minisql_tests PRIVATE minisql_core)

install(TARGETS minisql minisqld RUNTIME DESTINATION bin)
