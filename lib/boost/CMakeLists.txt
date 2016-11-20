cmake_minimum_required(VERSION 2.8.7)

project(boost)

include(ExternalProject)

ExternalProject_Add(
	boost

        SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/boost-prefix/src/boost_1_59_0
	BUILD_IN_SOURCE 1

	DOWNLOAD_COMMAND
        wget https://sourceforge.net/projects/boost/files/boost/1.59.0/boost_1_59_0.tar.bz2 &&
        tar -xf boost_1_59_0.tar.bz2

	UPDATE_COMMAND ""

	CONFIGURE_COMMAND
        ./bootstrap.sh --with-libraries=atomic,filesystem,log,program_options,random,serialization,system,test,thread

	BUILD_COMMAND ./b2 -j7

	INSTALL_COMMAND ln -sf ${CMAKE_CURRENT_BINARY_DIR}/boost-prefix/src/boost_1_59_0/stage/lib ${CMAKE_BINARY_DIR}/bin/boost
	)
