include(def_executable)

find_package(Node)
find_package(Protobuf)
find_package(GLog)

include_directories(${Node_INCLUDE_DIRS} ${CMAKE_CURRENT_BINARY_DIR})

include_directories(${CMAKE_BINARY_DIR}/Examples)
PROTOBUF_GENERATE_CPP(PROTO_SRCS PROTO_HDRS ExampleMessage.proto)

def_executable(Node
  SOURCES Node.cpp ${PROTO_SRCS}
  DEPENDS node
  LINK_LIBS ${GLOG_LIBRARIES} ${PROTOBUF_LIBRARIES}
  )

def_executable(Node1
  SOURCES Node1.cpp ${PROTO_SRCS}
  DEPENDS node
  LINK_LIBS ${GLOG_LIBRARIES} ${PROTOBUF_LIBRARIES}
  )

def_executable(Node2
  SOURCES Node2.cpp ${PROTO_SRCS}
  DEPENDS node
  LINK_LIBS ${GLOG_LIBRARIES} ${PROTOBUF_LIBRARIES}
  )
