cmake_minimum_required(VERSION 2.8.7)

project(crete)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include_directories("${PROJECT_SOURCE_DIR}/lib/include" "${CMAKE_BINARY_DIR}/lib/boost/boost-prefix/src/boost_1_59_0")
link_directories("${CMAKE_BINARY_DIR}/lib/boost/boost-prefix/src/boost_1_59_0/stage/lib")

add_subdirectory(lib)
add_subdirectory(back-end)
add_subdirectory(front-end)
add_subdirectory(misc)
add_subdirectory(test EXCLUDE_FROM_ALL)

install (PROGRAMS "${PROJECT_SOURCE_DIR}/back-end/klee/Release+Asserts/bin/klee"
         DESTINATION bin
         PERMISSIONS WORLD_EXECUTE WORLD_READ
         OWNER_READ OWNER_WRITE OWNER_EXECUTE
         RENAME crete-klee)

install (DIRECTORY "${PROJECT_SOURCE_DIR}/back-end/klee/Release+Asserts/lib/"
         DESTINATION lib
         PATTERN "*.bc"
         PATTERN ".svn" EXCLUDE
         PERMISSIONS WORLD_READ OWNER_READ OWNER_WRITE)

install (PROGRAMS "${PROJECT_SOURCE_DIR}/front-end/qemu-2.3/x86_64-softmmu/crete-qemu-2.3-system-x86_64"
         DESTINATION bin
         PERMISSIONS WORLD_EXECUTE WORLD_READ
         OWNER_READ OWNER_WRITE OWNER_EXECUTE)

install (PROGRAMS "${PROJECT_SOURCE_DIR}/back-end/llvm-translator/llvm-translator-qemu-2.3/x86_64-softmmu/crete-llvm-translator-x86_64"
         DESTINATION bin
         PERMISSIONS WORLD_EXECUTE WORLD_READ
         OWNER_READ OWNER_WRITE OWNER_EXECUTE
         RENAME crete-llvm-translator-qemu-2.3-x86_64)

install (FILES "${PROJECT_SOURCE_DIR}/back-end/llvm-translator/llvm-translator-qemu-2.3/x86_64-softmmu/bc_target_x86_64_helpers.bc"
         DESTINATION bin
         PERMISSIONS WORLD_READ OWNER_READ OWNER_WRITE
         RENAME crete-qemu-2.3-op-helper-x86_64.bc)

install (DIRECTORY "/home/chenbo/MSoftware/libs/boost_1_59_0/stage/lib/"
         DESTINATION lib
         PATTERN ".*.1.59.0"
         PERMISSIONS WORLD_READ OWNER_READ OWNER_WRITE)

## Pack libicu48 so that the deb package can be used on ubuntu 12.04 and 14.04
install (FILES "/usr/lib/libicuuc.so.48"
         "/usr/lib/libicui18n.so.48"
         "/usr/lib/libicuuc.so.48.1.1"
         "/usr/lib/libicui18n.so.48.1.1"
         "/usr/lib/libicudata.so.48"
         "/usr/lib/libicudata.so.48.1.1"
         DESTINATION lib
         PERMISSIONS WORLD_READ OWNER_READ OWNER_WRITE)

# cpack
SET(CPACK_PACKAGE_DESCRIPTION_SUMMARY "CRETE")
SET(CPACK_PACKAGE_VENDOR "System Validation Laboratory, Portland State University")
# SET(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/ReadMe.txt")
SET(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.TXT")
SET(CPACK_PACKAGE_VERSION_MAJOR "0")
SET(CPACK_PACKAGE_VERSION_MINOR "1")
SET(CPACK_PACKAGE_VERSION_PATCH "1")
SET(CPACK_SET_DESTDIR "ON")

SET(CPACK_GENERATOR "DEB")
SET(CPACK_DEBIAN_PACKAGE_MAINTAINER "Bo Chen (chenbo@pdx.edu)") #required
INCLUDE(CPack)
