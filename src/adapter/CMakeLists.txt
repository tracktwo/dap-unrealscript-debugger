
cmake_minimum_required(VERSION 3.15)

project (vscode-unrealscript-adapter C CXX)

option(DebuggerAdapter_INSTALL "Copy the Debugger adapter to the installation directory on build" OFF)
set(DebuggerAdapter_INSTALL_BINDIR "" CACHE STRING "Installation directory for the debugger adapter")

set (CMAKE_CXX_STANDARD 17)
string(REGEX REPLACE " /W[0-4]" "" CMAKE_C_FLAGS ${CMAKE_C_FLAGS})
string(REGEX REPLACE " /W[0-4]" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Dependencies

set (Boost_USE_STATIC_LIBS ON)
find_package(Boost REQUIRED)
include_directories(${Boost_INCLUDE_DIR})

# Third party utilities included in build
set (debugger_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)
set (cppdap_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/cppdap")
set (cppdap_BINARIES "${CMAKE_BINARY_DIR}/cppdap")
add_subdirectory(${cppdap_DIR} ${cppdap_BINARIES})

include_directories("${cppdap_DIR}/include")

# Common files
set (DBGCOMMON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../common")

set (DBGCOMMON_HDRS
    ${DBGCOMMON_DIR}/commands.h
    ${DBGCOMMON_DIR}/events.h
    ${DBGCOMMON_DIR}/message.h
)

include_directories(${DBGCOMMON_DIR})

# The debug adapter project
set (DBGADAPTER_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

set (DBGADAPTER_SRCS
    ${DBGADAPTER_SRC_DIR}/adapter.cpp
    ${DBGADAPTER_SRC_DIR}/client.cpp
    ${DBGADAPTER_SRC_DIR}/commands.cpp
    ${DBGADAPTER_SRC_DIR}/debugger.cpp
    ${DBGADAPTER_SRC_DIR}/events.cpp
)

set (DBGADAPTER_HDRS
    ${DBGADAPTER_SRC_DIR}/adapter.h
    ${DBGADAPTER_SRC_DIR}/client.h
    ${DBGADAPTER_SRC_DIR}/debugger.h
    ${DBGCOMMON_HDRS}
)

add_executable(DebuggerAdapter ${DBGADAPTER_SRCS} ${DBGADAPTER_HDRS})
target_compile_options(DebuggerAdapter PRIVATE "-permissive-" "-W3")
target_compile_definitions(DebuggerAdapter PRIVATE "_WIN32_WINNT=0x0601")
target_link_libraries(DebuggerAdapter PRIVATE cppdap)

# Copy the debug adapter build to the VS installer project.
add_custom_command(TARGET DebuggerAdapter POST_BUILD 
  COMMAND "${CMAKE_COMMAND}" -E copy 
     "$<TARGET_FILE:DebuggerAdapter>"
     "${debugger_ROOT}/extensions/visual_studio/UnrealScriptDebuggerAdapter/adapter/DebuggerAdapter.exe" 
  COMMENT "Copying to VS adapter directory")

  add_custom_command(TARGET DebuggerAdapter POST_BUILD 
  COMMAND "${CMAKE_COMMAND}" -E copy 
     "$<TARGET_FILE:DebuggerAdapter>"
     "${debugger_ROOT}/extensions/vscode/DebuggerAdapter.exe" 
  COMMENT "Copying to VS Code extension directory")

if (DebuggerAdapter_INSTALL)
    install(TARGETS DebuggerAdapter RUNTIME DESTINATION "${DebuggerAdapter_INSTALL_BINDIR}")
endif (DebuggerAdapter_INSTALL)
