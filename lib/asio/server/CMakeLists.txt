cmake_minimum_required(VERSION 2.8.7)

project(asio-server)

add_library(crete_asio_server SHARED server.cpp)
add_dependencies(crete_asio_server boost)

install(TARGETS crete_asio_server LIBRARY DESTINATION lib)
