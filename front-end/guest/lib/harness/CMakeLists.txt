cmake_minimum_required(VERSION 2.8.7)

project(harness)

add_definitions(-D_LARGEFILE64_SOURCE)

add_library(crete_harness SHARED harness.cpp symfile.c)
target_link_libraries(crete_harness crete_vm_comm crete_hook boost_filesystem boost_system)

add_dependencies(crete_harness boost)
