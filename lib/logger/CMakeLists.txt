cmake_minimum_required(VERSION 2.8.7)

project(logger)


add_definitions(-DBOOST_LOG_DYN_LINK)

add_library(crete_logger SHARED logger_attr_impl.cpp logger_attr.cpp logger_impl.cpp log_stream.cpp logger.cpp)

target_link_libraries(crete_logger boost_log boost_atomic)

add_dependencies(crete_logger boost)

install(TARGETS crete_logger LIBRARY DESTINATION lib)
