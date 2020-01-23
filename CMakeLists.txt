
cmake_minimum_required(VERSION 3.15)

project (vscode-unrealscript-debugger C CXX)

# Options
option(DebuggerInterface_INSTALL "Copy DebuggerInterface.dll to the installation directory on build" OFF)
set(DebuggerInterface_INSTALL_BINDIR "" CACHE STRING "Installation directory for the debugger interface")

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
set (cppdap_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/cppdap")
add_subdirectory(${cppdap_DIR})

include_directories("${cppdap_DIR}/include")

# Common files
set (DBGCOMMON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/common")

set (DBGCOMMON_HDRS
    ${DBGCOMMON_DIR}/commands.h
    ${DBGCOMMON_DIR}/events.h
    ${DBGCOMMON_DIR}/message.h
)

include_directories(${DBGCOMMON_DIR})

# The debugger interface project
set (DBGIFACE_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/interface")

set (DBGIFACE_SRCS
    ${DBGIFACE_SRC_DIR}/commands.cpp
    ${DBGIFACE_SRC_DIR}/debuggerinterface.cpp
    ${DBGIFACE_SRC_DIR}/events.cpp
    ${DBGIFACE_SRC_DIR}/service.cpp
)

set (DBGIFACE_HDRS
    ${DBGIFACE_SRC_DIR}/service.h
    ${DBGCOMMON_HDRS}
)

add_library(DebuggerInterface SHARED ${DBGIFACE_SRCS} ${DBGIFACE_HDRS})
target_compile_options(DebuggerInterface PRIVATE "-permissive-")

if (DebuggerInterface_INSTALL)
    install(TARGETS DebuggerInterface RUNTIME DESTINATION "${DebuggerInterface_INSTALL_BINDIR}")
endif (DebuggerInterface_INSTALL)

# The debug adapter project
set (DBGADAPTER_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/adapter")

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
target_compile_options(DebuggerAdapter PRIVATE "-permissive-")
target_link_libraries(DebuggerAdapter PRIVATE cppdap)

if (DebuggerAdapter_INSTALL)
    install(TARGETS DebuggerAdapter RUNTIME DESTINATION "${DebuggerAdapter_INSTALL_BINDIR}")
endif (DebuggerAdapter_INSTALL)
