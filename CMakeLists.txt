project(Node)
cmake_minimum_required( VERSION 2.8 )
enable_testing()

# Add to module path, so we can find our cmake modules
list( APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules )
include( install_library )

if(CMAKE_COMPILER_IS_GNUCXX)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DOS_OBJECT_USE_OBJC=0")
endif()

#SET(BUILD_SHARED_LIBS ON)

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wno-unused-parameter")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare")

find_package( ZeroMQ REQUIRED )
find_package( ZeroMQPP REQUIRED )
find_package( Protobuf REQUIRED )
find_package( GFlags REQUIRED )
find_package( GLog REQUIRED )

if(NOT APPLE)
  find_package(DNSSD REQUIRED)
  find_package(Avahi REQUIRED)
endif(NOT APPLE)

if(DNSSD_FOUND OR APPLE)
  set(HAVE_DNSSD ON)
endif()
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/include/Node/NodeConfig.h.in ${CMAKE_CURRENT_BINARY_DIR}/NodeConfig.h)

# NB all "REQUIRED" items are for external dependencies -- so make sure not to
# include non-standard paths (like home directories or build directories).
set(REQUIRED_LIBRARIES ${PROTOBUF_LIBRARY} ${ZeroMQ_LIBRARIES}
    ${ZeroMQPP_LIBRARIES} ${Avahi_LIBRARIES} ${GFlags_LIBRARIES}
    ${GLOG_LIBRARIES} dl)
set(REQUIRED_INCLUDE_DIRS ${ZeroMQ_INCLUDE_DIRS} ${ZeroMQPP_INCLUDE_DIRS}
    ${Avahi_INCLUDE_DIRS} ${PROTOBUF_INCLUDE_DIRS} ${GFlags_INCLUDE_DIRS}
    ${GLOG_INCLUDE_DIRS})


if(DNSSD_FOUND)
  list(APPEND REQUIRED_INCLUDE_DIRS ${DNSSD_INCLUDE_DIRS})
  list(APPEND REQUIRED_LIBRARIES ${DNSSD_LIBRARIES})
endif()

if(ANDROID)
    # Override to use hosts protoc compiler
    unset(PROTOBUF_PROTOC_EXECUTABLE CACHE)
    find_host_package( Protobuf REQUIRED)
endif()

set( NODE_HDRS include/Node/Node.h include/Node/ZeroConf.h include/Node/Messages.h )
set( NODE_SRCS src/Node.cpp src/ZeroConf.cpp src/crc_16.c src/crc_16f.c
    src/crc_32.c src/Messages.cpp )

if(ANDROID)
  list(APPEND NODE_HDRS include/Node/ifaddrs.h)
  list(APPEND NODE_SRCS src/ifaddrs.c)
endif()

include_directories(${REQUIRED_INCLUDE_DIRS})
include_directories(${CMAKE_SOURCE_DIR}/include ${CMAKE_BINARY_DIR} )
add_library( node ${NODE_SRCS} )
target_link_libraries( node ${REQUIRED_LIBRARIES} )
 
# Install library and create a pkg-config package.
install_library(
  LIBRARY node
  VERSION 2.1
  DESCRIPTION "Simple library for RPC and pub/sub."
  HEADERS ${NODE_HDRS}
  HEADER_DESTINATION ${CMAKE_INSTALL_PREFIX}/include/Node
  INCLUDE_DIRS ${REQUIRED_INCLUDE_DIRS}
  LIB_DEPENDS ${REQUIRED_LIBRARIES}
  LINK_DIRS /usr/local/lib
  )

# For the examples to find this version of node, but still have a generic
# CMakeLists.txt that can be cut-n-paste used as an example for other projects. 
set( NODE_LIBRARIES ${REQUIRED_LIBRARIES} node ) 
set( NODE_INCLUDE_DIRS ${REQUIRED_INCLUDE_DIRS} ) 
set( NODE_DIR ${CMAKE_CURRENT_BINARY_DIR} )
link_libraries( ${REQUIRED_LIBRARIES} node ) 
add_subdirectory( examples )

###########################
## A simple uninstall script.
## Alternatively UNIX users can run/sudo `xargs rm < install_manifest.txt` in the build directory.

set(unfile ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)
file(WRITE ${unfile} "IF(NOT EXISTS \"install_manifest.txt\")\n")
file(APPEND ${unfile} "MESSAGE(\"FATAL_ERROR Cannot find \\\"install manifest\\\": install_manifest.txt\")\n")
file(APPEND ${unfile} "ENDIF(NOT EXISTS \"install_manifest.txt\")\n")
file(APPEND ${unfile} "FILE(READ \"install_manifest.txt\" files)\n")
file(APPEND ${unfile} "STRING(REGEX REPLACE \"\\n\" \";\" files \"\${files}\")\n")
file(APPEND ${unfile} "FOREACH(file \${files})\n")
file(APPEND ${unfile} "  MESSAGE(STATUS \"Uninstalling \\\"\${file}\\\"\")\n")
file(APPEND ${unfile} "  IF(EXISTS \"\${file}\")\n")
file(APPEND ${unfile} "    EXEC_PROGRAM(\n")
file(APPEND ${unfile} "      \"\${CMAKE_COMMAND}\" ARGS \"-E remove \\\"\${file}\\\"\"\n")
file(APPEND ${unfile} "    OUTPUT_VARIABLE rm_out\n")
file(APPEND ${unfile} "      RETURN_VALUE rm_retval\n")
file(APPEND ${unfile} "      )\n")
file(APPEND ${unfile} "    IF(\"\${rm_retval}\" STREQUAL 0\)\n")
file(APPEND ${unfile} "    ELSE(\"\${rm_retval}\" STREQUAL 0\)\n")
file(APPEND ${unfile} "    MESSAGE(FATAL_ERROR \"Problem when removing \\\"\${file}\\\"\")\n")
file(APPEND ${unfile} "    ENDIF(\"\${rm_retval}\" STREQUAL 0)\n")
file(APPEND ${unfile} "  ELSE(EXISTS \"\${file}\")\n")
file(APPEND ${unfile} "    MESSAGE(STATUS \"File \\\"\${file}\\\" does not exist.  \")\n")
file(APPEND ${unfile} "  ENDIF(EXISTS \"\${file}\")\n")
file(APPEND ${unfile} "ENDFOREACH(file)\n")

add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)
###########################
