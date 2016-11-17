cmake_minimum_required(VERSION 2.8.7)

project(replay-preload)


add_definitions(-Wl,-z,defs)

add_library(crete_replay_preload SHARED replay-preload.cpp)
target_link_libraries(crete_replay_preload boost_system boost_filesystem boost_serialization dl crete_test_case)
add_dependencies(crete_replay_preload boost)
