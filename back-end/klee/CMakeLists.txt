cmake_minimum_required(VERSION 2.8.7)

project(klee)

include(ExternalProject)

ExternalProject_Add(
  klee

  SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}
  BUILD_IN_SOURCE 1

  DOWNLOAD_COMMAND ""
  UPDATE_COMMAND ""

  CONFIGURE_COMMAND
  ./configure
  --with-llvm=${CMAKE_BINARY_DIR}/lib/llvm/llvm-3.2-prefix/src/llvm-3.2.src
  --with-stp=${CMAKE_BINARY_DIR}/bin/stp/stp-r940


  BUILD_COMMAND
  make  -j7 ENABLE_OPTIMIZED=1
  CXX=${CMAKE_BINARY_DIR}/lib/llvm/llvm-3.2-prefix/src/llvm-3.2.src/Release+Asserts/bin/clang++
  "CXXFLAGS=-I${CMAKE_SOURCE_DIR}/lib/include -I${CMAKE_BINARY_DIR}/lib/boost/boost-prefix/src/boost_1_59_0"
  "LDFLAGS=-L${CMAKE_BINARY_DIR}/bin -L${CMAKE_BINARY_DIR}/bin/boost"

  INSTALL_COMMAND ln -sf ${CMAKE_CURRENT_SOURCE_DIR}/Release+Asserts/bin/klee ${CMAKE_BINARY_DIR}/bin/crete-klee
  )

add_dependencies(klee llvm-3.2 stp-r940 crete_test_case boost)

add_custom_target(klee-remake ALL
  COMMAND
  make  -j7 ENABLE_OPTIMIZED=1
  CXX=${CMAKE_BINARY_DIR}/lib/llvm/llvm-3.2-prefix/src/llvm-3.2.src/Release+Asserts/bin/clang++
  "CXXFLAGS=-I${CMAKE_SOURCE_DIR}/lib/include -I${CMAKE_BINARY_DIR}/lib/boost/boost-prefix/src/boost_1_59_0"
  "LDFLAGS=-L${CMAKE_BINARY_DIR}/bin -L${CMAKE_BINARY_DIR}/bin/boost"

  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  DEPENDS klee)
