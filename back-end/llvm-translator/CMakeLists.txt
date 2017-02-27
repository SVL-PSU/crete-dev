cmake_minimum_required(VERSION 2.8.7)

project(llvm-translator)

include(ExternalProject)

# Dependency: llvm-3.4 is installed

execute_process(COMMAND llvm-config-3.4 --prefix
  OUTPUT_VARIABLE LLVM_PATH
  OUTPUT_STRIP_TRAILING_WHITESPACE)

ExternalProject_Add(
	llvm-translator-qemu-2.3

	SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/llvm-translator-qemu-2.3
	BUILD_IN_SOURCE 1

	DOWNLOAD_COMMAND ""
	UPDATE_COMMAND ""

	CONFIGURE_COMMAND
        ./configure
        --target-list=i386-softmmu,x86_64-softmmu
        --enable-llvm
        --enable-tcg-interpreter
        --disable-zlib-test
        --with-llvm=${LLVM_PATH}
        --cxx=clang++-3.4
        "--extra-ldflags=-L${CMAKE_BINARY_DIR}/bin -L${CMAKE_BINARY_DIR}/bin/boost"
        "--extra-cflags=-mno-sse3 -g -I${CMAKE_SOURCE_DIR}/lib/include -I${CMAKE_BINARY_DIR}/lib/boost/boost-prefix/src/boost_1_59_0 -DTCG_LLVM_OFFLINE"
        "--extra-cxxflags=-mno-sse3 -O2 -DTCG_LLVM_OFFLINE -DUSE_LLVM_3_4"

        BUILD_COMMAND make -j7

	INSTALL_COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/llvm-translator-qemu-2.3/crete_gen_links.sh ${CMAKE_BINARY_DIR}
	)

add_dependencies(llvm-translator-qemu-2.3 boost)

add_custom_target(llvm-translator-qemu-2.3-remake ALL
  COMMAND  make -j7
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/llvm-translator-qemu-2.3
  DEPENDS llvm-translator-qemu-2.3)
