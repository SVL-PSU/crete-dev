cmake_minimum_required(VERSION 2.8.7)

project(tc-compare)

add_executable(crete-tc-compare tc-compare.cpp)

target_link_libraries(crete-tc-compare crete_test_case boost_system boost_filesystem boost_serialization boost_program_options stdc++ m rt)
