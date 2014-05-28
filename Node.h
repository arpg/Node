//
//   node::Node is a simple peer-to-peer networking tool providing
//   pub/sub and rpc functionality.  As ever, the goal for node it so
//   be simple, yet powerful.  As an example, here is how the API can
//   be used:
//
//   node::Node n1;
//   n1.init("NodeName1");
//   n1.advertise("LeftImage");
//   n1.publish("LeftImage", data); // data needs to be a zmq msg or google pb
//
//   ...
//
//   node::Node n2;
//   n2.init("NodeName2");
//   n2.subscribe("NodeName2/LeftImage"); // allow explicit subscription
//   n2.receive("NodeName1/LeftImage", data); // blocking call
//
//   ...
//
//   node::Node n3;
//   n3.init("name3");
//   n3.provide_rpc("MyFunc", MyFunc);
//
//   ...
//
//   n1.call_rpc("name3/MyFunc", arg, res);
//
#ifndef _NODE_NODE_H_
#define _NODE_NODE_H_

#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

#include <google/protobuf/message.h>
#include <miniglog/logging.h>
#include <NodeConfig.h>
#include <NodeMessages.pb.h>
#include <Node/zmq.hpp>
#include <Node/ZeroConf.h>
#include <zmqpp/zmqpp.hpp>

namespace node { class node; }

// global list of all allocated nodes -- we will call _BroadcastExit on shutdown
extern std::vector<node::node*> g_vNodes;

namespace node {

struct TimedNodeSocket;
typedef std::shared_ptr<zmqpp::socket> NodeSocket;

typedef void(*FuncPtr)(google::protobuf::Message&,
                       google::protobuf::Message&,
                       void *);

typedef std::function<void(google::protobuf::Message&,
                           google::protobuf::Message&,
                           void *)> RPCFunction;

typedef
std::function<void(std::shared_ptr<google::protobuf::Message>)> TopicCallback;

class node {
  static const std::string kTopicScheme, kRpcScheme, kListenScheme;

 public:
  /// node constructor you can call wo initialization.  user MUST call
  /// init at some point.
  ///
  /// @param use_autodiscovery Use Avahi to autodiscover other nodes
  ///        (if available)
  explicit node(bool use_auto_discovery = true);
  virtual ~node();
  void set_verbosity(int nLevel);

  ///
  /// Input: Node identifier
  bool init(std::string node_name);

  /// Specialization to register a remote procedure call for the
  // "int f(string)" signature.
  //< Input: Function name
  //< Input: Function pointer
  bool provide_rpc(const std::string& sName,
                   int (*pFunc)(const std::string&));

  /// Register a remote procedure call.
  //< Input: Function name
  //< Input: Function pointer
  //< Input: User data passed to the function
  template <class Req, class Rep>
  bool provide_rpc(const std::string& sName,
                   void (*pFunc)(Req&, Rep&, void*),
                   void* pUserData);

  template <class Req, class Rep>
  bool provide_rpc(const std::string& function_name,
                   const RPCFunction& func);

  /// Make a remote procedure call like "node->func()".
  /// This is a specialization for the "int f(string)" signature
  ///
  /// @param rpc_resource Remote node name and rpc method
  /// @param input to rpc method
  /// @param int result
  bool call_rpc(const std::string& rpc_resource,
                const std::string& input,
                int& result);

  /// Make a remote procedure call like "node->func()".
  //< Input:  node/rpc
  //< Input: Protobuf message request
  //< Output: Protobuf message reply
  //< Input: ms to wait for reply
  bool call_rpc(const std::string& rpc_resource,
                const google::protobuf::Message& msg_req,
                google::protobuf::Message& msg_rep,
                unsigned int time_out = 0);

  /// Make a remote procedure call like "node->func()".
  //< Input: Remote node name
  //< Input: Remote function to call
  //< Input: Protobuf message request
  //< Output: Protobuf message reply
  //< Input: ms to wait for reply
  bool call_rpc(const std::string& node_name,
                const std::string& function,
                const google::protobuf::Message& msg_req,
                google::protobuf::Message& msg_rep,
                unsigned int time_out = 0);

  ///  Tell all other nodes we publish this topic
  /// Input: Topic name
  bool advertise(const std::string& topic);

  /// Send data.
  /// Input: Topic to write to
  /// Input: Message to send
  bool publish(const std::string& topic,
               const google::protobuf::Message&  msg);
  bool publish(const std::string& topic, zmqpp::message& msg);
  bool publish(const std::string& topic, const std::string& msg);

  /// Subscribe to a topic being advertised by another node
  /// @param [in] Node resource: "NodeName/Topic"
  bool subscribe(const std::string& resource);

  /// Listen for a given message topic on a random port
  /// @param [in] Node resource: "Topic"
  bool listen(const std::string& topic);
  bool listen(const std::string& topic, uint16_t port);

  /// Send a message to a specific listener
  /// @param [in] listener: The "node/topic" that is listening
  /// @param [in] msg: The message to send
  bool send(const std::string& listener, const std::string& msg);
  bool send(const std::string& listener, const google::protobuf::Message& msg);
  bool send(const std::string& listener, zmqpp::message* msg);

  /// Consume data from publisher
  /// Input: Node resource: "NodeName/Topic"
  /// Output: Message read
  bool receive(const std::string& resource, google::protobuf::Message& msg);
  bool receive(const std::string& resource, std::string* msg);
  bool receive(const std::string& resource, zmqpp::message& zmq_msg);

  template<class Callbackmsg>
  bool RegisterCallback(const std::string &resource, TopicCallback func);

  /// Figure out the network name of this machine
  std::string _GetHostIP(const std::string& sPreferredInterface = "eth");

  // this function return the name of all connected client
  std::vector<std::string> GetSubscribeClientName();

  /// Connect to another node at a given hostname string and port
  ///
  /// The GetTableResponse will be filled out with the response from
  /// the connected node. This includes the Node name to be used for
  /// RPC nodes.
  ///
  /// Returns whether the connection was successful.
  bool ConnectNode(const std::string& host, uint16_t port,
                   msg::GetTableResponse* rep);

  /// Disconnect from the desired node
  void DisconnectNode(const std::string& node_name);

  bool using_auto_discovery() const {
    return use_auto_discovery_;
  }

  void set_using_auto_discovery(bool use_auto) {
    use_auto_discovery_ = use_auto;
  }

  // Set the port for this Node to use. Can only be called BEFORE init().
  void set_bind_port(uint16_t port) {
    CHECK(!initialized_) << "Only call set_bind_port before init().";
    port_ = port;
    use_fixed_port_ = true;
  }

  // Get the port this node is listening on.
  uint16_t bind_port() {
    return port_;
  }

 protected:
  void HeartbeatThread();
  void RPCThread();
  void TopicThread(const std::string& resource);


  /// Heartbeat, called by client _DoHeartbeat to check if he is up-to-date.
  static void _HeartbeatFunc(msg::HeartbeatRequest& req,
                             msg::HeartbeatResponse& rep,
                             void* pUserData);

  void HeartbeatFunc(msg::HeartbeatRequest& req,
                     msg::HeartbeatResponse& rep);

  /// here we are replying to a remote node who has asked for a copy
  /// of our node table
  static void _GetResourceTableFunc(msg::GetTableRequest& req,
                                    msg::GetTableResponse& rep,
                                    void* pUserData);

  void GetResourceTableFunc(msg::GetTableRequest& req,
                            msg::GetTableResponse& rep);

  /// being asked by other nodes to remove references to them from our
  /// resource table
  static void _DeleteFromResourceTableFunc(msg::DeleteFromTableRequest& req,
                                           msg::DeleteFromTableResponse& rep,
                                           void* pUserData);

  void DeleteFromResourceTableFunc(msg::DeleteFromTableRequest& req,
                                   msg::DeleteFromTableResponse& rep);

  /// here we are receiving an update to our node table from a remote node
  static void _SetResourceTableFunc(msg::SetTableRequest& req,
                                    msg::SetTableResponse& rep,
                                    void* pUserData);

  void SetResourceTableFunc(msg::SetTableRequest& req,
                            msg::SetTableResponse& rep);

  /// Make a remote procedure call like "node->func()" -- with out
  /// node name resolution.  this is the main API most calls boil down
  /// to.
  //< Input: Remote function to call
  //< Input: Protobuf message request
  //< Output: Protobuf message reply
  //< Input: ms to wait for reply
  bool call_rpc(NodeSocket socket,
                std::mutex* socket_mutex,
                const std::string& function,
                const google::protobuf::Message& msg_req,
                google::protobuf::Message& msg_rep,
                int timeout_ms = 0);

 private:
  /// Build a protobuf containing all the resources we know of, and the CRC.
  msg::ResourceTable _BuildResourceTableMessage(msg::ResourceTable& t);

  /// Checksum of the ResourceTable (used by nodes to check they are
  /// up-to-date).
  uint32_t _ResourceTableCRC();

  /// Here we loop through our list of known ndoes and call their
  // SetNodeTable functions, passing them our node table.
  void _PropagateResourceTable();

  /// Ask zeroconf for list of nodes on the network and ask each for
  //  their resource table.
  void _UpdateNodeRegistery();

  /// print the node table
  void _PrintResourceLocatorTable();

  /// print the rpc sockets
  void _PrintRpcSockets();

  // Bind the given socket on a certain port
  bool _BindPort(uint16_t port, const NodeSocket& socket);

  // Find an available port number and bind to it.
  int _BindRandomPort(const NodeSocket& socket);

  std::string _GetAddress() const;
  std::string _GetAddress(const std::string& sHostIP, const int nPort) const;

  std::string _ZmqAddress() const;
  std::string _ZmqAddress(const std::string& sHostIP, const int nPort) const;

  double _Tic();
  double _Toc(double dSec);
  double _TicMS();
  double _TocMS(double dMS);
  ///
  std::string _ParseNodeName(const std::string& resource);
  ///
  std::string _ParseRpcName(const std::string& resource);

  // ensure we have a connection
  void _ConnectRpcSocket(const std::string& node_name_name,
                         const std::string& node_nameAddr);

  static void NodeSignalHandler(int nSig);

  void _BroadcastExit();

  // signature for simplified int f(void) style functions
  static void _IntStringFunc(msg::String& sStr,
                             msg::Int& nInt, void* pUserData);

  void BuildDeleteFromTableRequest(msg::DeleteFromTableRequest* msg) const;

 private:
  // ZMQ context. Must be initialized first and destroyed last.
  std::unique_ptr<zmqpp::context> context_;

  // NB a "resource" is a nodename, node/rpc or node/topic URL
  // resource to host:port map
  std::map<std::string, std::string> resource_table_;

  struct RPC {
    RPCFunction RpcFunc;
    std::shared_ptr<google::protobuf::Message> ReqMsg;
    std::shared_ptr<google::protobuf::Message> RepMsg;
    void* UserData;
  };

  struct TopicCallbackData {
    TopicCallback callback_;
    std::shared_ptr<google::protobuf::Message> callback_msg_;
  };

  struct TopicData {
    std::mutex mutex;
    std::thread thread;
    std::shared_ptr<TopicCallbackData> callback;
    NodeSocket socket;
  };

  struct RpcData {
    std::mutex mutex;
    std::shared_ptr<TimedNodeSocket> socket;
    std::shared_ptr<RPC> rpc;
  };

  struct ListenData {
    std::mutex mutex;
    NodeSocket socket;
  };

  struct SendData {
    std::mutex mutex;
    NodeSocket socket;
  };

  std::map<std::string, std::shared_ptr<TopicData> > topics_;
  std::map<std::string, std::shared_ptr<RpcData> > rpc_;
  std::map<std::string, std::shared_ptr<ListenData> > listen_data_;
  std::map<std::string, std::shared_ptr<SendData> > send_data_;

  // for automatic server discovery
  ZeroConf zero_conf_;

  // global socket, (RPC too)
  NodeSocket socket_;

  // node's machine IP
  std::string host_ip_;

  // node's RPC port with a default.
  uint16_t port_ = 1776;

  // node unique name
  std::string node_name_;

  // initialized?
  bool init_done_;

  // Thread for handling rpc
  std::thread rpc_thread_;

  // Thread for handling heartbeats
  std::thread heartbeat_thread_;

  // Max timeout wait
  double get_resource_table_max_wait_;
  double heartbeat_wait_thresh_;

  // Timeout w/o heartbeat before death is declared in seconds.
  double heartbeat_death_timeout_ = 10.0;
  unsigned int resource_table_version_;
  mutable std::mutex mutex_;

  // send and receive message max wait
  int send_recv_max_wait_ = 3000;

  // Should we use autodiscovery to find other nodes
  bool use_auto_discovery_ = true;

  // Has a port number been set for this Node to use
  bool use_fixed_port_ = false;

  // Has this Node been initialized yet?
  bool initialized_ = false;

  int debug_level_ = 0;

  bool exiting_ = false;
};

template <class Req, class Rep>
bool node::provide_rpc(const std::string& function_name,
                       void (*pFunc)(Req&, Rep&, void*),
                       void* pUserData) {
  return provide_rpc<Req, Rep>(function_name,
                               std::bind((FuncPtr)pFunc,
                                         std::placeholders::_1,
                                         std::placeholders::_2, pUserData));
}

template <class Req, class Rep>
bool node::provide_rpc(const std::string& function_name,
                       const RPCFunction& func) {
  auto it = rpc_.find(function_name);
  if (it != rpc_.end()) return false;

  auto data = std::make_shared<RpcData>();
  auto rpc = std::make_shared<RPC>();
  rpc->RpcFunc = func;
  rpc->ReqMsg.reset(new Req);
  rpc->RepMsg.reset(new Rep);
  data->rpc = rpc;
  rpc_[function_name] = data;

  std::string rpc_resource = "rpc://" + node_name_ + "/" + function_name;
  resource_table_[rpc_resource] = _GetAddress();
  return true;
}


template<class Callbackmsg>
bool node::RegisterCallback(const std::string &resource, TopicCallback func) {
  std::string topicResource = kTopicScheme + resource;

  auto it = topics_.find(topicResource);
  if (it == topics_.end()) {
    LOG(WARNING) << "Topic '" << resource << "' hasn't been subscribed to.";
    return false;
  }

  static_assert(
      std::is_base_of<google::protobuf::Message, Callbackmsg>::value,
      "Value provided for template is not a protobuf message.");
  std::shared_ptr<google::protobuf::Message> msg =
      std::make_shared<Callbackmsg>();
  std::shared_ptr<TopicCallbackData> tcd =
      std::make_shared<TopicCallbackData>();
  tcd->callback_ = func;
  tcd->callback_msg_ = msg;

  it->second->thread = std::thread(
      std::bind(&node::TopicThread, this, resource));
  it->second->callback = tcd;

  LOG(INFO) << "Registered callback for '" << topicResource << "'";
  return true;
}
}  // end namespace node
#endif  // _NODE_NODE_H_
