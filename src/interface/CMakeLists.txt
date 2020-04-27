
cmake_minimum_required(VERSION 3.15)

project (vscode-unrealscript-interface C CXX)

# Options
option(DebuggerInterface_INSTALL "Copy DebuggerInterface.dll to the installation directory on build" OFF)
set(DebuggerInterface_INSTALL_BINDIR "" CACHE STRING "Installation directory for the debugger interface")

set (CMAKE_CXX_STANDARD 17)
string(REGEX REPLACE " /W[0-4]" "" CMAKE_C_FLAGS ${CMAKE_C_FLAGS})
string(REGEX REPLACE " /W[0-4]" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Dependencies

set (Boost_USE_STATIC_LIBS ON)
find_package(Boost REQUIRED)
include_directories(${Boost_INCLUDE_DIR})

# Common files
set (DBGCOMMON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../common")

set (DBGCOMMON_HDRS
    ${DBGCOMMON_DIR}/commands.h
    ${DBGCOMMON_DIR}/events.h
    ${DBGCOMMON_DIR}/message.h
)

include_directories(${DBGCOMMON_DIR})

# The debugger interface project
set (DBGIFACE_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

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
target_compile_options(DebuggerInterface PRIVATE "-permissive-" "-W3")
target_compile_definitions(DebuggerInterface PRIVATE "_WIN32_WINNT=0x0601")

if (DebuggerInterface_INSTALL)
    install(TARGETS DebuggerInterface RUNTIME DESTINATION "${DebuggerInterface_INSTALL_BINDIR}")
endif (DebuggerInterface_INSTALL)
