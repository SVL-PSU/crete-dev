cmake_minimum_required(VERSION 2.8.7)

project(boost)

include(ExternalProject)

ExternalProject_Add(
	boost
	URL https://sourceforge.net/projects/boost/files/boost/1.59.0/boost_1_59_0.tar.bz2/download
	UPDATE_COMMAND ""
	BUILD_IN_SOURCE 1
	CONFIGURE_COMMAND ./bootstrap.sh --with-libraries=atomic,filesystem,log,program_options,random,serialization,system,test,thread
	BUILD_COMMAND ./b2 -j7
	INSTALL_COMMAND ln -sf ${CMAKE_BINARY_DIR}/lib/boost/boost-prefix/src/boost/stage/lib ${CMAKE_BINARY_DIR}/bin/boost
	)

