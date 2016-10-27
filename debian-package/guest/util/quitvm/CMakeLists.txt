cmake_minimum_required(VERSION 2.8.7)

project(quitvm)

add_executable(crete-quitvm quitvm.cpp)

target_link_libraries(crete-quitvm crete_vm_comm)

