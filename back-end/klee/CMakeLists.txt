cmake_minimum_required(VERSION 2.8.7)

project(klee)

include(ExternalProject)

ExternalProject_Add(
  klee
  DOWNLOAD_COMMAND ""
  UPDATE_COMMAND ""
  SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}
  BUILD_IN_SOURCE 1
  CONFIGURE_COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/configure --with-llvm=${CMAKE_BINARY_DIR}/lib/llvm/llvm-3.2-prefix/src/llvm-3.2 --with-stp=${CMAKE_BINARY_DIR}/bin/stp/stp-r940
  BUILD_COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/crete_make_klee.sh ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR}
  INSTALL_COMMAND ln -sf ${CMAKE_CURRENT_SOURCE_DIR}/Release+Asserts/bin/klee ${CMAKE_BINARY_DIR}/bin/crete-klee
  )

add_dependencies(klee llvm-3.2 stp-r940 crete_test_case)

add_custom_target(klee-remake ALL
  COMMAND  ./crete_make_klee.sh ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR}
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  DEPENDS klee)
