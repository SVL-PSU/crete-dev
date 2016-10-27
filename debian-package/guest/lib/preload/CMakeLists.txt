cmake_minimum_required(VERSION 2.8.7)

project(preload)

add_library(crete_preload SHARED preload.cpp argv_processor.cpp)
target_link_libraries(crete_preload crete_harness crete_vm_comm boost_serialization boost_filesystem boost_system dl)

add_dependencies(crete_preload boost)
