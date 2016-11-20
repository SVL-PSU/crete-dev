cmake_minimum_required(VERSION 2.8.7)

project(llvm)

include(ExternalProject)

ExternalProject_Add(
	llvm-3.2

        SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/llvm-3.2-prefix/src/llvm-3.2.src
        BUILD_IN_SOURCE 1

        DOWNLOAD_COMMAND
        wget http://llvm.org/releases/3.2/llvm-3.2.src.tar.gz && tar -xf llvm-3.2.src.tar.gz &&
        wget http://llvm.org/releases/3.2/clang-3.2.src.tar.gz && tar -xf clang-3.2.src.tar.gz  &&
        mv clang-3.2.src llvm-3.2.src/tools/clang

	UPDATE_COMMAND ""
        CONFIGURE_COMMAND ./configure --enable-jit --enable-optimized --enable-assertions
        BUILD_COMMAND make -j7 ENABLE_OPTIMIZED=1 REQUIRES_RTTI=1
        INSTALL_COMMAND ""
	)
