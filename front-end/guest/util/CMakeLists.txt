cmake_minimum_required(VERSION 2.8.7)

project(util)

add_subdirectory(run)
add_subdirectory(tc-replay)

add_custom_target(sandbox-support ALL
  COMMAND  sudo setcap cap_sys_admin,cap_sys_chroot+ep ${CMAKE_BINARY_DIR}/bin/crete-run && sudo setcap cap_sys_admin,cap_sys_chroot+ep ${CMAKE_BINARY_DIR}/bin/crete-tc-replay && sudo cp ${CMAKE_BINARY_DIR}/bin/libcrete_replay_preload.so /lib && sudo cp ${CMAKE_BINARY_DIR}/bin/libcrete_run_preload.so /lib
  DEPENDS crete-run crete_run_preload crete-tc-replay crete_replay_preload)
