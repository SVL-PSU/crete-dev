cmake_minimum_required(VERSION 2.8.7)

LIST(APPEND CMAKE_CXX_FLAGS -std=c++11)

project(memcheck)

add_executable(crete-memcheck memcheck.cpp)

target_link_libraries(crete-memcheck crete_proc_reader crete_test_case boost_system boost_filesystem boost_regex boost_program_options boost_serialization)
