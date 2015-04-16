
project(Node)
cmake_minimum_required( VERSION 2.8 )
enable_testing()

# Add to module path, so we can find our cmake modules
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules)

include(def_library)
include(export_package)

string( TOLOWER ${PROJECT_NAME} LIBRARY_NAME )

if(CMAKE_COMPILER_IS_GNUCXX)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DOS_OBJECT_USE_OBJC=0")
endif()

#SET(BUILD_SHARED_LIBS ON)

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wno-unused-parameter")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare")

find_package(ZeroMQ REQUIRED)
find_package(ZeroMQPP REQUIRED)
find_package(Protobuf REQUIRED)
find_package(GFlags REQUIRED)
find_package(GLog REQUIRED)

if(NOT APPLE)
  find_package(DNSSD REQUIRED)
  find_package(Avahi REQUIRED)
endif(NOT APPLE)

if(DNSSD_FOUND OR APPLE)
  set(HAVE_DNSSD ON)
endif()
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/NodeConfig.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/NodeConfig.h)

set(PROJ_INCLUDE_DIRS
  ${CMAKE_BINARY_DIR}
  ${ZeroMQ_INCLUDE_DIRS}
  ${ZeroMQPP_INCLUDE_DIRS}
  ${Avahi_INCLUDE_DIRS}
  ${PROTOBUF_INCLUDE_DIRS}
  ${Node_BINARY_DIR}
  ${Node_SOURCE_DIR}/include
  ${GFlags_INCLUDE_DIRS}
  ${GLOG_INCLUDE_DIRS}
  )

set(PROJ_LIBRARIES
  ${PROTOBUF_LIBRARY}
  ${ZeroMQ_LIBRARIES}
  ${ZeroMQPP_LIBRARIES}
  ${Avahi_LIBRARIES}
  ${GFlags_LIBRARIES}
  ${GLOG_LIBRARIES}
  dl
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

PROTOBUF_GENERATE_CPP(PROTO_SRCS PROTO_HDRS NodeMessages.proto)

set( NODE_HDRS include/Node/Node.h include/Node/ZeroConf.h include/Node/crc.h ${PROTO_HDRS} )
set( NODE_SRCS src/Node.cpp src/ZeroConf.cpp src/crc_16.c src/crc_16f.c src/crc_32.c ${PROTO_SRCS} )

if(ANDROID)
  list(APPEND NODE_HDRS include/Node/ifaddrs.h)
  list(APPEND NODE_SRCS src/ifaddrs.c)
endif()

include_directories(${PROJ_INCLUDE_DIRS})

set(EXPORT_NODE_INC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${Node_SOURCE_DIR}/include
  ${CMAKE_CURRENT_BINARY_DIR}
  ${PROTOBUF_INCLUDE_DIRS}
  ${ZeroMQ_INCLUDE_DIRS}
  ${ZeroMQPP_INCLUDE_DIRS}
  ${GFlags_INCLUDE_DIRS}
  ${GLOG_INCLUDE_DIRS}
  )

message(STATUS "ZEROMQPP_LIBRARIES ${ZeroMQPP_LIBRARIES}" )
message(STATUS "NODE_LIBRARIES ${PROJ_LIBRARIES}" )

def_library(${LIBRARY_NAME}
  SOURCES ${NODE_HDRS} ${NODE_SRCS}
  LINK_LIBS ${PROJ_LIBRARIES}
  PACKAGE Node
  )

export_package(Node
  TARGETS ${LIBRARY_NAME}
  LIBRARY ${LIBRARY_NAME}
  LIBRARIES ${LIBRARY_NAME} ${PROJ_LIBRARIES}
  INCLUDE_DIRS ${EXPORT_NODE_INC}
  )

# For the examples to find this and only this version of Node
set(Node_DIR ${CMAKE_CURRENT_BINARY_DIR})
add_subdirectory(examples)

# add_subdirectory(testing)

