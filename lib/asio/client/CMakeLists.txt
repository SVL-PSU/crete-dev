cmake_minimum_required(VERSION 2.8.7)

project(asio-client)


add_library(crete_asio_client SHARED client_pimpl.cpp client.cpp)
add_dependencies(crete_asio_client boost)

install(TARGETS crete_asio_client LIBRARY DESTINATION lib)
