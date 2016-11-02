cmake_minimum_required(VERSION 2.8.7)

project(dispatch)

LIST(APPEND CMAKE_CXX_FLAGS -std=c++11)

add_executable(crete-dispatch dispatch_ui.cpp)

target_link_libraries(crete-dispatch crete_cluster boost_program_options boost_filesystem boost_system)

install(TARGETS crete-dispatch DESTINATION bin)
