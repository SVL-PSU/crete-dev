cmake_minimum_required(VERSION 2.8.7)

project(llvm-translator)

include(ExternalProject)

ExternalProject_Add(
	llvm-translator-qemu-2.3
	DOWNLOAD_COMMAND ""
	UPDATE_COMMAND ""
	SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/llvm-translator-qemu-2.3
	BUILD_IN_SOURCE 1
	CONFIGURE_COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/llvm-translator-qemu-2.3/crete_configure.sh ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR}
	BUILD_COMMAND make -j7
	INSTALL_COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/llvm-translator-qemu-2.3/crete_gen_links.sh ${CMAKE_BINARY_DIR}
	)

add_dependencies(llvm-translator-qemu-2.3 llvm-3.2)

add_custom_target(llvm-translator-qemu-2.3-remake ALL
  COMMAND  make -j7
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/llvm-translator-qemu-2.3
  DEPENDS llvm-translator-qemu-2.3)
