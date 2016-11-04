cmake_minimum_required(VERSION 2.8.7)

project(run-preload)

add_library(crete_run_preload SHARED run_preload.cpp)
target_link_libraries(crete_run_preload crete_harness crete_vm_comm boost_serialization boost_filesystem boost_system dl)

add_dependencies(crete_run_preload boost)
