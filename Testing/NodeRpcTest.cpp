#include <gtest/gtest.h>
#include <Node/Node.h>

TEST(Node, RPC) {
  node::node n1(false);
  node::node n2(false);

  n1.set_bind_port(8000);
  n2.set_bind_port(8001);

  ASSERT_TRUE(n1.init("n1"));
  ASSERT_TRUE(n2.init("n2"));

  bool was_called = false;
  auto callback = [&](google::protobuf::Message& a,
                      google::protobuf::Message& b,
                      void*) {
    was_called = true;
  };

  bool provide_ret = n1.provide_rpc<msg::String, msg::String>("rpc", callback);
  EXPECT_TRUE(provide_ret);

  msg::GetTableResponse rep;
  EXPECT_TRUE(n2.ConnectNode("localhost", 8000, &rep));
  EXPECT_EQ("n1", rep.sender_name());
  msg::String a, b;
  n2.call_rpc("n1/rpc", a, b);
  EXPECT_TRUE(was_called);
}
