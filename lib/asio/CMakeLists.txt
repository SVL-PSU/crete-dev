cmake_minimum_required(VERSION 2.8.7)

project(asio)

LIST(APPEND CMAKE_CXX_FLAGS -std=c++11)

add_subdirectory(client)
add_subdirectory(server)

