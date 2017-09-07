cmake_minimum_required(VERSION 2.8.7)

project(klee-tc-replay)

add_executable(klee-tc-replay klee-replay.cpp)

target_link_libraries(klee-tc-replay boost_system boost_filesystem boost_serialization boost_program_options stdc++)
add_dependencies(klee-tc-replay boost)
