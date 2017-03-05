cmake_minimum_required(VERSION 2.8.7)

project(util)

add_subdirectory(tc-replay)
add_subdirectory(debug)
# add_subdirectory(memcheck)
add_subdirectory(config-generator)

add_custom_target(tc-replay-sandbox-support
  COMMAND
  sudo setcap cap_sys_admin,cap_sys_chroot+ep ${CMAKE_BINARY_DIR}/bin/crete-tc-replay &&
  sudo cp ${CMAKE_BINARY_DIR}/bin/libcrete_replay_preload.so /lib
  DEPENDS crete-tc-replay crete_replay_preload)

add_custom_target(
  COMMAND
  sudo setcap cap_sys_admin,cap_sys_chroot+ep ${CMAKE_BINARY_DIR}/bin/crete-tc-replay &&
  sudo cp ${CMAKE_BINARY_DIR}/bin/libcrete_replay_preload.so /lib
  DEPENDS crete-tc-replay crete_replay_preload)
