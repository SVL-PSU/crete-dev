cmake_minimum_required(VERSION 2.8.7)

project(proc-reader)

add_library(crete_proc_reader SHARED proc_reader.cpp)
target_link_libraries(crete_proc_reader boost_regex boost_filesystem)

install(TARGETS crete_proc_reader LIBRARY DESTINATION lib)
