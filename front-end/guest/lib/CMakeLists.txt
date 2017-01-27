cmake_minimum_required(VERSION 2.8.7)

project(lib)

add_subdirectory(boost)
add_subdirectory(asio)
add_subdirectory(elf-reader)
add_subdirectory(run-preload)
add_subdirectory(proc-reader)
add_subdirectory(test-case)
add_subdirectory(vm-comm)
add_subdirectory(replay-preload)
