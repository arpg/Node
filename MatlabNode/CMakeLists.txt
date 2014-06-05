project( NodeSim )

cmake_minimum_required(VERSION 2.8)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -Wextra")
if(${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
endif()

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/CMakeModules/")

find_package( Node REQUIRED )
find_package( MINIGLOG REQUIRED )
find_package( PbMsgs REQUIRED )

include_directories( ${Node_INCLUDE_DIRS}
  ${MINIGLOG_INCLUDE_DIRS}
  ${PbMsgs_INCLUDE_DIRS})

add_executable( NodeSim NodeSim.cpp NodeSim.h)

target_link_libraries( NodeSim ${Node_LIBRARIES}
  ${PbMsgs_LIBRARIES})

		     