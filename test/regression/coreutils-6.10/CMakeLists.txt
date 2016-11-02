cmake_minimum_required(VERSION 2.8.7)

project(coreutils-6.10)

include(ExternalProject)

ExternalProject_Add(
	test-coreutils-6.10
	URL http://ftp.gnu.org/gnu/coreutils/coreutils-6.10.tar.gz
	SOURCE_DIR coreutils-6.10
	BUILD_IN_SOURCE 1
	CONFIGURE_COMMAND ${CMAKE_CURRENT_BINARY_DIR}/coreutils-6.10/configure CFLAGS=-coverage
	BUILD_COMMAND make -j7
	INSTALL_COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/prepare-test.sh ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}
)

