
include(def_test)

if(NOT TARGET gtest)
  add_subdirectory(gtest)
endif()

include_directories(gtest/include)

def_test(NodeListenTest
  SOURCES NodeListenTest.cpp
  DEPENDS node
  )
