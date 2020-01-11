
cmake_minimum_required(VERSION 3.15)

project (vscode-unrealscript-debugger C CXX)

# Options
option(DebuggerInterface_INSTALL "Copy DebuggerInterface.dll to the installation directory on buld" OFF)
set(DebuggerInterface_INSTALL_BINDIR "" CACHE STRING "Installation directory for the debugger interface")

set (CMAKE_CXX_STANDARD 17)
string(REGEX REPLACE " /W[0-4]" "" CMAKE_C_FLAGS ${CMAKE_C_FLAGS})
string(REGEX REPLACE " /W[0-4]" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Dependencies

set (Boost_USE_STATIC_LIBS ON)
find_package(Boost REQUIRED COMPONENTS regex)
include_directories(${Boost_include_dirs})

find_package(ZeroMQ CONFIG REQUIRED)

# protobuf is also needed
set (Protobuf_USE_STATIC_LIBS ON)
find_package(Protobuf REQUIRED)
include_directories(${Protobuf_INCLUDE_DIRS})

# Third party utilities included in build
set (cppdap_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/cppdap")
add_subdirectory(${cppdap_DIR})

# Unreal Debugger Interface RPC definition
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS src/protos/commands.proto src/protos/events.proto)

include_directories("${CMAKE_CURRENT_BINARY_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/third_party/azmq" "${cppdap_DIR}/include")

set (DBGIFACE_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/interface")

set (DBGIFACE_SRCS
    ${DBGIFACE_SRC_DIR}/debuggerinterface.cpp
    ${DBGIFACE_SRC_DIR}/service.cpp
    ${PROTO_SRCS}
)

set (DBGIFACE_HDRS
    ${DBGIFACE_SRC_DIR}/service.h
    ${PROTO_HDRS}
)

add_library(DebuggerInterface SHARED ${DBGIFACE_SRCS} ${DBGIFACE_HDRS})
target_link_libraries(DebuggerInterface PRIVATE protobuf::libprotobuf libzmq-static ${Boost_LIBRARIES} cppdap)

if (DebuggerInterface_INSTALL)
    install(TARGETS DebuggerInterface RUNTIME DESTINATION "${DebuggerInterface_INSTALL_BINDIR}")
endif (DebuggerInterface_INSTALL)