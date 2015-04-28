cmake_minimum_required(VERSION 2.8)

# Typically, you'll just call find_package( NODE REQUIRED )
# but we've taken care of that from the TLD

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

include_directories( ${CMAKE_BINARY_DIR}/examples ${NODE_INCLUDE_DIRS} )
PROTOBUF_GENERATE_CPP( PROTO_SRCS PROTO_HDRS ExampleMessage.proto )

add_executable( NodeExample NodeExample.cpp ${PROTO_SRCS} )
add_executable( Node1 Node1.cpp ${PROTO_SRCS} )
add_executable( Node2 Node2.cpp ${PROTO_SRCS} )

if(${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
  target_link_libraries( NodeExample ${NODE_LIBRARIES} pthread )
  target_link_libraries( Node1 ${NODE_LIBRARIES} pthread )
  target_link_libraries( Node2 ${NODE_LIBRARIES} pthread )
else()
  target_link_libraries( NodeExample ${NODE_LIBRARIES} )
  target_link_libraries( Node1 ${NODE_LIBRARIES} )
  target_link_libraries( Node2 ${NODE_LIBRARIES} )
endif()
