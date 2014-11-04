cmake_policy( SET CMP0048 NEW ) # for project versioning
project(Node VERSION 0.3 )
cmake_minimum_required( VERSION 3.0 )
enable_testing()

# Add to module path, so we can find our cmake modules
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/CMakeModules)

include(def_library)
include(export_package)

string( TOLOWER ${PROJECT_NAME} LIBRARY_NAME )

if(CMAKE_COMPILER_IS_GNUCXX)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DOS_OBJECT_USE_OBJC=0")
endif()

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wno-unused-parameter")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -Wextra -Wno-unused-parameter")

find_package(ZeroMQ REQUIRED)
find_package(Protobuf REQUIRED)
find_package(MINIGLOG REQUIRED)

if(NOT APPLE)
  find_package(DNSSD)
  find_package(Avahi)
endif(NOT APPLE)

if(DNSSD_FOUND OR APPLE)
  set(HAVE_DNSSD ON)
endif()
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/NodeConfig.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/NodeConfig.h)

set(PROJ_INCLUDE_DIRS
  ${CMAKE_BINARY_DIR}
  ${ZeroMQ_INCLUDE_DIRS}
  ${Avahi_INCLUDE_DIRS}
  ${PROTOBUF_INCLUDE_DIRS}
  ${Node_BINARY_DIR}
  ${Node_SOURCE_DIR}
  ${Node_SOURCE_DIR}/include
  ${MINIGLOG_INCLUDE_DIRS}
  ${CMAKE_CURRENT_SOURCE_DIR}/zmqpp/src
  )

set(PROJ_LIBRARIES
  ${PROTOBUF_LIBRARY}
  ${ZeroMQ_LIBRARIES}
  ${AVAHI_LIBRARIES}
  )

if(DNSSD_FOUND)
  list(APPEND PROJ_INCLUDE_DIRS ${DNSSD_INCLUDE_DIRS})
  list(APPEND PROJ_LIBRARIES ${DNSSD_LIBRARIES})
endif()

if(ANDROID)
    # Override to use hosts protoc compiler
    unset(PROTOBUF_PROTOC_EXECUTABLE CACHE)
    find_host_package( Protobuf REQUIRED)
endif()

PROTOBUF_GENERATE_CPP(PROTO_HDRS PROTO_SRCS NodeMessages.proto)

set(NODE_HDRS include/Node/Node.h include/Node/ZeroConf.h include/Node/crc.h ${PROTO_HDRS})
set(NODE_SRCS src/Node.cpp src/ZeroConf.cpp src/crc_16.c src/crc_16f.c src/crc_32.c ${PROTO_SRCS})

if(ANDROID)
  list(APPEND NODE_HDRS android-ifaddrs/ifaddrs.h)
  list(APPEND NODE_SRCS android-ifaddrs/ifaddrs.c)
  include_directories(SYSTEM android-ifaddrs)
  list(APPEND PROJ_INCLUDE_DIRS android-ifaddrs)
endif()

include_directories(${PROJ_INCLUDE_DIRS})
add_subdirectory(zmqpp)

# Create the NODEConfig.cmake file for the build tree.
set(EXPORT_NODE_INC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${Node_SOURCE_DIR}/include
  ${CMAKE_CURRENT_BINARY_DIR}
  ${PROTOBUF_INCLUDE_DIRS}
  ${ZeroMQ_INCLUDE_DIRS}
  ${CMAKE_CURRENT_SOURCE_DIR}/zmqpp/src
  )

def_library(${LIBRARY_NAME}
  SOURCES ${NODE_HDRS} ${NODE_SRCS}
  LINK_LIBS ${PROJ_LIBRARIES} miniglog
  DEPENDS zmqpp
  PACKAGE Node
  )

export_package(Node
  TARGETS ${LIBRARY_NAME}
  LIBRARY ${LIBRARY_NAME}
  LIBRARIES ${LIBRARY_NAME}
  INCLUDE_DIRS ${EXPORT_NODE_INC}
  )

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/NodeConfig.h"
  DESTINATION ${CMAKE_INSTALL_PREFIX}/include/${LIBRARY_NAME}
  )

# For the examples to find this and only this version of Node
set(Node_DIR ${CMAKE_CURRENT_BINARY_DIR})
add_subdirectory(Examples)
add_subdirectory(Testing)

