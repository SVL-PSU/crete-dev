cmake_minimum_required(VERSION 2.8.7)

project(boost)

include(ExternalProject)

ExternalProject_Add(
	boost
	DOWNLOAD_DIR ${CMAKE_CURRENT_BINARY_DIR}
	URL ${CMAKE_CURRENT_SOURCE_DIR}/boost.src.tar.bz2
	UPDATE_COMMAND ""
	BUILD_IN_SOURCE 1
	CONFIGURE_COMMAND ./bootstrap.sh --includedir=${CMAKE_SOURCE_DIR}/lib/include --libdir=${CMAKE_BINARY_DIR}/bin
	BUILD_COMMAND ./b2 -j7 install
	INSTALL_COMMAND ""
	)


### enforce compile 32 bits on 64 bits OS
# BUILD_COMMAND ./b2 address-model=32 -j7 install
