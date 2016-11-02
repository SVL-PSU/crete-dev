cmake_minimum_required(VERSION 2.8.7)

project(elf-reader)

add_library(crete_elf_reader SHARED elf_reader.cpp)
target_link_libraries(crete_elf_reader elf)

install(TARGETS crete_elf_reader LIBRARY DESTINATION lib)
