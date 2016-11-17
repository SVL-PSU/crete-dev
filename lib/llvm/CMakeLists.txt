cmake_minimum_required(VERSION 2.8.7)

project(llvm)

include(ExternalProject)

ExternalProject_Add(
	llvm-3.2
	SVN_REPOSITORY http://llvm.org/svn/llvm-project/llvm/branches/release_32
	UPDATE_COMMAND wget http://llvm.org/releases/3.2/clang-3.2.src.tar.gz && tar -xf clang-3.2.src.tar.gz && mv clang-3.2.src tools/clang
	BUILD_IN_SOURCE 1
	CONFIGURE_COMMAND ${CMAKE_CURRENT_BINARY_DIR}/llvm-3.2-prefix/src/llvm-3.2/configure --enable-jit --enable-optimized --enable-assertions
	BUILD_COMMAND make -j7 ENABLE_OPTIMIZED=1 REQUIRES_RTTI=1
	INSTALL_COMMAND ""
	)

