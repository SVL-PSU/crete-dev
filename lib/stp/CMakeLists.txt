cmake_minimum_required(VERSION 2.8.7)

project(stp)

include(ExternalProject)

ExternalProject_Add(
	stp-2.1.2

        GIT_REPOSITORY https://github.com/stp/stp.git
        GIT_TAG "2.1.2"

        CMAKE_ARGS -DBUILD_SHARED_LIBS:BOOL=OFF -DENABLE_PYTHON_INTERFACE:BOOL=OFF

	UPDATE_COMMAND ""
	INSTALL_COMMAND ""
	)
