cmake_minimum_required(VERSION 2.8.7)

project(svm-node)

LIST(APPEND CMAKE_CXX_FLAGS -std=c++11)

add_executable(crete-svm-node node_svm.cpp)

target_link_libraries(crete-svm-node crete_cluster boost_filesystem boost_system boost_program_options boost_thread stdc++)

install(TARGETS crete-svm-node DESTINATION bin)
