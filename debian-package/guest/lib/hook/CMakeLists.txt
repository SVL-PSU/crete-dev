cmake_minimum_required(VERSION 2.8.7)

project(hook)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}) 
add_definitions(-D_GNU_SOURCE)

add_library(crete_hook SHARED hook.c utility.c klee/fd_32.c klee/fd_64.c klee/fd.c)
target_link_libraries(crete_hook dl)
