cmake_minimum_required(VERSION 2.8.7)

project(stp)

include(ExternalProject)

ExternalProject_Add(
	stp-r940
	DOWNLOAD_DIR ${CMAKE_CURRENT_BINARY_DIR}
	URL ${CMAKE_CURRENT_SOURCE_DIR}/stp-r940.tgz
	UPDATE_COMMAND ""
	SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/stp-r940
	BUILD_IN_SOURCE 1
	CONFIGURE_COMMAND ${CMAKE_CURRENT_BINARY_DIR}/stp-r940/scripts/configure --with-prefix=${CMAKE_BINARY_DIR}/bin/stp/stp-r940 --with-cryptominisat2
	BUILD_COMMAND make OPTIMIZE=-O2 CFLAGS_M32= install -j7
	INSTALL_COMMAND ""
	)
