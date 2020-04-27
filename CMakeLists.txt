
cmake_minimum_required(VERSION 3.15)

project (vscode-unrealscript-debugger C CXX)

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/src/interface")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/src/adapter")

