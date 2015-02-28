/*
  Exampel calling rpc methods in Node1, and subscribing to Node1 topic

  Run Node1 then run this guy.

*/

#include <Node/Node.h>
#include "ExampleMessage.pb.h"

#include <stdio.h>
#include <chrono> // for std::chrono::sleep
#include <thread>

using namespace std;
using namespace google;

void Node1Topic2Callback( shared_ptr<protobuf::Message> mMsg )
{
  shared_ptr<Msg> msg;
  msg = dynamic_pointer_cast<Msg>(mMsg);
  printf("In Callback Got '%s'\n", msg->value().c_str() );
}

int main()
{

  node::node n;
  n.set_verbosity( 2 ); // make some noise on errors
  n.init("Node2");

  // Example subscribing to a topic published by Node1:
  if( n.subscribe( "Node1/Node1Topic" ) == false ) {
    printf("Error subscribing to Node1Topic.\n");
  }

  // Example of failure case subscribing to a non existant topic:
  if( n.subscribe( "Node1/Node1Topic2" ) == false ) {
    printf("Error subscribing to Node1Topic2.\n");
  }

  // Example to register a generic callback
  function<void(shared_ptr<protobuf::Message>)> func_ptr = Node1Topic2Callback;
  if( !n.register_callback<Msg>("Node1/Node1Topic2", Node1Topic2Callback)){
    printf("Topic 'Node1/Node1Topic2' not found; register callback failed\n");
  }
  printf("Registered callback for message 'Node1/Node1Topic2'\n");

  // Example demonstrating how to adverstize a topic:
  if( n.advertise( "Node2Topic" ) == false ) {
    printf("Error subscribing to topic.\n");
  }

  unsigned int nCount = 0;

  // Example using the "easy" api -- call Node1->SimpleRpcMethod("test")
  int res;
  n.call_rpc( "Node1/SimpleRpcMethod", "test", res );
  printf("Got %d back from 'Node1/SimpleRpcMethod'\n", res);

  // Example: make a mistake
  n.call_rpc( "Node1/SimpleRpcMethod2", "test", res );

  while(1) {
    Msg mMsg;
    n.receive( "Node1/Node1Topic", mMsg ); // blocking call
    printf("Got '%s'.\n", mMsg.value().c_str());

    nCount++;
    if( nCount == 3 ) {
      printf("--- Sending RPC message! ---\n");
      mMsg.set_value( "Bye!" );
      n.call_rpc( "Node1/RpcMethod", mMsg, mMsg );
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  return 0;
}
