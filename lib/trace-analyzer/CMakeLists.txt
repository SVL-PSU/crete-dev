cmake_minimum_required(VERSION 2.8.7)

LIST(APPEND CMAKE_CXX_FLAGS -std=c++11)

project(trace-analyzer)


add_library(crete_trace_analyzer SHARED selector.cpp trace_graph.cpp trace_analyzer.cpp)
target_link_libraries(crete_trace_analyzer boost_filesystem boost_random pthread)
add_dependencies(crete_trace_analyzer boost)

install(TARGETS crete_trace_analyzer LIBRARY DESTINATION lib)
