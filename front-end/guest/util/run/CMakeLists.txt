cmake_minimum_required(VERSION 2.8.7)

project(run)

#zl3 add debug info
add_definitions(-g)
add_definitions(-DFUSION_MAX_VECTOR_SIZE=20)

add_executable(crete-run runner.cpp)

target_link_libraries(crete-run crete_vm_comm crete_elf_reader crete_proc_reader crete_asio_client boost_regex boost_serialization boost_program_options boost_filesystem boost_system pthread)
