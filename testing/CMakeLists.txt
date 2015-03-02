include(def_test)

if(NOT TARGET gtest)
  add_subdirectory(gtest)
endif()

find_package(Node)
find_package(Protobuf)
find_package(MINIGLOG)

include_directories(
  gtest/include
  ${Node_INCLUDE_DIRS}
  )

set(EXTRA_LIBS ${PROTOBUF_LIBRARIES} ${MINIGLOG_LIBRARIES})
set(TEST_CONDITIONS PROTOBUF_FOUND MINIGLOG_FOUND)

def_test(NodeListenTest
  SOURCES NodeListenTest.cpp
  DEPENDS node
  LINK_LIBS ${EXTRA_LIBS}
  CONDITIONS ${TEST_CONDITIONS}
  )

def_test(NodePubSubTest
  SOURCES NodePubSubTest.cpp
  DEPENDS node
  LINK_LIBS ${EXTRA_LIBS}
  CONDITIONS ${TEST_CONDITIONS}
  )

def_test(NodeRpcTest
  SOURCES NodeRpcTest.cpp
  DEPENDS node
  LINK_LIBS ${EXTRA_LIBS}
  CONDITIONS ${TEST_CONDITIONS}
  )
