cmake_minimum_required(VERSION 2.8.7)

LIST(APPEND CMAKE_CXX_FLAGS -std=c++11)

project(debug)

add_executable(crete-debug debug.cpp asm_mode.cpp)

target_link_libraries(crete-debug crete_proc_reader boost_system boost_filesystem boost_regex boost_program_options)
