cmake_minimum_required(VERSION 3.10)
project(CrowTouchServer)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Crow needs Asio and Boost
find_package(Boost REQUIRED COMPONENTS system thread)
find_package(Threads REQUIRED)
find_package(Crow REQUIRED CONFIG)
find_package(magic_args REQUIRED CONFIG)

add_executable(crow_server main.cpp)

target_link_libraries(crow_server
    PRIVATE
    magic_args::magic_args
    Crow::Crow
    Boost::system
    Boost::thread
    Threads::Threads
)
