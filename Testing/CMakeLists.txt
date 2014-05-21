
include(def_test)

if(NOT TARGET gtest)
  add_subdirectory(gtest)
endif()

find_package(Node QUIET)

include_directories(
  gtest/include
  ${Node_INCLUDE_DIRS}
  )

def_test(NodeListenTest
  SOURCES NodeListenTest.cpp
  DEPENDS node
  )

def_test(NodePubSubTest
  SOURCES NodePubSubTest.cpp
  DEPENDS node
  )
