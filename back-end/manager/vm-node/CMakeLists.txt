cmake_minimum_required(VERSION 2.8.7)

project(vm-node)

LIST(APPEND CMAKE_CXX_FLAGS -std=c++11)

add_executable(crete-vm-node node_vm.cpp)

target_link_libraries(crete-vm-node crete_cluster boost_filesystem boost_system boost_program_options boost_thread stdc++)

install(TARGETS crete-vm-node DESTINATION bin)
