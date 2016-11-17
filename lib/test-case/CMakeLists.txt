cmake_minimum_required(VERSION 2.8.7)

project(test-case)


add_library(crete_test_case SHARED test_case.cpp)
target_link_libraries(crete_test_case boost_system boost_filesystem boost_serialization)
add_dependencies(crete_test_case boost)

install(TARGETS crete_test_case LIBRARY DESTINATION lib)
