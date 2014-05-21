#include <gtest/gtest.h>
#include <node/Node.h>

TEST(Node, PubSub) {
  node::node n1(false);
  node::node n2(false);

  n1.set_bind_port(1779);
  n2.set_bind_port(1780);

  ASSERT_TRUE(n1.init("n1"));
  ASSERT_TRUE(n2.init("n2"));

  msg::GetTableResponse rep;
  ASSERT_TRUE(n2.ConnectNode("localhost", 1779, &rep));
  EXPECT_EQ("n1", rep.sender_name());

  ASSERT_TRUE(n1.advertise("def"));
  ASSERT_TRUE(n2.subscribe("n1/def"));

  static const std::string msg = "Hello, World!";

  bool received_yet = false;
  std::string received;
  // Even this is not precise: With ZMQ, you do not know when you have
  // actually finished connecting to a subscriber, so we must just try
  // and send and send until we know it's gotten there.
  /** @todo Add intelligence to node to avoid this */
  for (int i = 0; i < 100 && !received_yet; ++i) {
    EXPECT_TRUE(n1.publish("def", msg));
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    received_yet = n2.receive("n1/def", &received);
  }
  ASSERT_TRUE(received_yet);
  EXPECT_EQ(msg, received);
}
