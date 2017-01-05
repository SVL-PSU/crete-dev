cmake_minimum_required(VERSION 2.8.7)

project(cluster)


LIST(APPEND CMAKE_CXX_FLAGS -std=c++11)

add_definitions(-DBOOST_MPL_CFG_NO_PREPROCESSED_HEADERS -DBOOST_MPL_LIMIT_VECTOR_SIZE=30 -DBOOST_MPL_LIMIT_MAP_SIZE=30 -DFUSION_MAX_VECTOR_SIZE=30)

add_library(crete_cluster SHARED node_registrar.cpp node.cpp svm_node_fsm.cpp svm_node.cpp vm_node_fsm.cpp vm_node.cpp dispatch.cpp test_pool.cpp trace_pool.cpp common.cpp node_options.cpp vm_node_options.cpp svm_node_options.cpp)

target_link_libraries(crete_cluster crete_asio_server crete_asio_client crete_elf_reader crete_logger crete_proc_reader crete_test_case boost_chrono boost_date_time)

add_dependencies(crete_cluster boost)

install(TARGETS crete_cluster LIBRARY DESTINATION lib)
