project(zmqpp)
cmake_minimum_required(VERSION 2.8)

include(def_library)

set(VERSION_MAJOR 3)
set(VERSION_MINOR 2)
set(VERSION_REVISION 0)
set(APP_VERSION ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_REVISION})

add_definitions(-MMD -std=c++0x -pipe
  -DBUILD_VERSION_MAJOR=${VERSION_MAJOR}
  -DBUILD_VERSION_MINOR=${VERSION_MINOR}
  -DBUILD_VERSION_REVISION=${VERSION_REVISION}
  -DBUILD_VERSION=\"${APP_VERSION}\"
  )

set(SRCS
  src/zmqpp/compatibility.hpp
  src/zmqpp/context.cpp
  src/zmqpp/context.hpp
  src/zmqpp/context_options.hpp
  src/zmqpp/exception.hpp
  src/zmqpp/inet.hpp
  src/zmqpp/message.cpp
  src/zmqpp/message.hpp
  src/zmqpp/poller.cpp
  src/zmqpp/poller.hpp
  src/zmqpp/socket.cpp
  src/zmqpp/socket.hpp
  src/zmqpp/socket_options.hpp
  src/zmqpp/socket_types.hpp
  src/zmqpp/zmqpp.cpp
  src/zmqpp/zmqpp.hpp
  )

def_library(zmqpp
  SOURCES ${SRCS}
  LINK_LIBS zmq
  PACKAGE Node
  )