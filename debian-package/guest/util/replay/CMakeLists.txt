cmake_minimum_required(VERSION 2.8.7)

project(replay)

add_executable(crete-replay replay.cpp)

target_link_libraries(crete-replay crete_test_case boost_system boost_filesystem boost_serialization boost_program_options)
