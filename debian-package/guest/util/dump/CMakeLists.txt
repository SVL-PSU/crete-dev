cmake_minimum_required(VERSION 2.8.7)

project(dump)

add_executable(crete-dump dump.cpp)

target_link_libraries(crete-dump crete_vm_comm)

