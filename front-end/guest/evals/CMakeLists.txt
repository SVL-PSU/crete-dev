cmake_minimum_required(VERSION 2.8.7)

project(evals)

add_custom_target(evals
  COMMAND  ./build-evals.sh ${CMAKE_CURRENT_SOURCE_DIR}
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
