#include <node/Node.h>
#include <arpa/inet.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>

#include <boost/crc.hpp>  // for boost::crc_32_
#include "TicToc.h"
#include "ZeroConf.h"

std::vector<node::node*> g_vNodes;

namespace node {

const std::string node::kListenScheme = "listen://";
const std::string node::kTopicScheme = "topic://";
const std::string node::kRpcScheme = "rpc://";

/// used to time socket communications
struct TimedNodeSocket {
  TimedNodeSocket() {}
  TimedNodeSocket(const NodeSocket& socket) : socket(socket),
                                              last_heartbeat_time(0.0) {}

  TimedNodeSocket& operator=(const TimedNodeSocket& RHS) {
    if (this == &RHS) {
      return *this;
    }
    socket = RHS.socket;
    last_heartbeat_time = RHS.last_heartbeat_time;
    return *this;
  }

  void Tic() {
    last_heartbeat_time = node_time::Tic();
  }

  NodeSocket  socket;
  double      last_heartbeat_time;
};

zmq::context_t* _InitSingleton() {
  // not ideal! we should apparently port away from avahi-compat... ug
  setenv("AVAHI_COMPAT_NOWARN", "1", 1);
  static zmq::context_t* pContext = new zmq::context_t(1);
  return pContext;
}

node::node(bool use_auto_discovery) : context_(nullptr),
                                      use_auto_discovery_(use_auto_discovery) {
  heartbeat_wait_thresh_ = 10;

  // maximum of timeout. Default value is 0.1.
  get_resource_table_max_wait_ = 0.8;
  resource_table_version_ = 1;
  init_done_ = false;
}

static void WaitThread(std::thread& t) {
  if (t.joinable()) {
    t.join();
  }
}

node::~node() {
  _BroadcastExit();
  exiting_ = true;

  WaitThread(rpc_thread_);
  WaitThread(heartbeat_thread_);
  for (auto it = topics_.begin(); it != topics_.end(); ++it) {
    WaitThread(it->second->thread);
  }
}

void node::set_verbosity(int level) {
  debug_level_ = level;
}

///
/// Input: Node identifier
bool node::init(std::string node_name) {
  node_name_ = node_name;
  host_ip_ = _GetHostIP();
  context_ = _InitSingleton();

  // install signal handlers so we can exit and tell other nodes about it..
  signal(SIGHUP       , NodeSignalHandler);
  signal(SIGINT       , NodeSignalHandler);
  signal(SIGQUIT      , NodeSignalHandler);
  signal(SIGILL       , NodeSignalHandler);
  signal(SIGTRAP      , NodeSignalHandler);
  signal(SIGABRT      , NodeSignalHandler);
  signal(SIGFPE       , NodeSignalHandler);
  signal(SIGKILL      , NodeSignalHandler);
  signal(SIGBUS       , NodeSignalHandler);
  signal(SIGSEGV      , NodeSignalHandler);
  signal(SIGSYS       , NodeSignalHandler);
  signal(SIGPIPE      , NodeSignalHandler);
  signal(SIGALRM      , NodeSignalHandler);
  signal(SIGTERM      , NodeSignalHandler);
  signal(SIGURG       , NodeSignalHandler);
  signal(SIGSTOP      , NodeSignalHandler);
  signal(SIGTSTP      , NodeSignalHandler);
  signal(SIGCONT      , NodeSignalHandler);
  signal(SIGCHLD      , NodeSignalHandler);
  signal(SIGTTIN      , NodeSignalHandler);
  signal(SIGTTOU      , NodeSignalHandler);
  signal(SIGIO        , NodeSignalHandler);
  signal(SIGXCPU      , NodeSignalHandler);
  signal(SIGXFSZ      , NodeSignalHandler);
  signal(SIGVTALRM    , NodeSignalHandler);
  signal(SIGPROF      , NodeSignalHandler);
  signal(SIGUSR1      , NodeSignalHandler);
  signal(SIGUSR2      , NodeSignalHandler);

  g_vNodes.push_back(this);

  init_done_ = true;
  LOG(debug_level_) << "Finished registering signals";

  // RPC server socket
  socket_.reset(new zmq::socket_t(*context_, ZMQ_REP));

  // If we're not using ZeroConf then we should bind a known port and
  // fail if we can't bind that port.
  if (use_fixed_port_ || !use_auto_discovery_) {
    if (!_BindPort(port_, socket_)) {
      return false;
    }
  } else {
    port_ = _BindRandomPort(socket_);
  }

  // register with zeroconf
  if (use_auto_discovery_ && zero_conf_.IsValid()) {
    if (!zero_conf_.RegisterService("hermes_" + node_name_,
                                    "_hermes._tcp", port_)) {
      LOG(ERROR) << "Registering node '" << node_name_
                 << "' with ZeroConf -- make sure the name is unique";
      return false;
    }
  }

  // register special calls for distributing the node-table
  this->provide_rpc("Heartbeat", &_HeartbeatFunc, this);
  this->provide_rpc("GetResourceTable", &_GetResourceTableFunc, this);
  this->provide_rpc("SetResourceTable", &_SetResourceTableFunc, this);
  this->provide_rpc("DeleteFromResourceTable",
                    &_DeleteFromResourceTableFunc, this);

  rpc_thread_ = std::thread(std::bind(&node::RPCThread, this));
  usleep(100);  // Sleep to split log messages
  heartbeat_thread_ = std::thread(std::bind(&node::HeartbeatThread, this));

  // ask avahi who's around, and get their resource tables, also build our table
  _UpdateNodeRegistery();
  LOG(debug_level_) << "Finished updating node registry";

  LOG(debug_level_) << "Node '" << node_name_ << "' started at " << host_ip_;

  // propagate changes
  _PropagateResourceTable();
  LOG(debug_level_) << "Finished propagating resource table";

  _PrintResourceLocatorTable();

  initialized_ = true;
  return true;
}

bool node::provide_rpc(const std::string& name,
                       int (*pFunc)(const std::string&)) {
  return provide_rpc(name, _IntStringFunc, (void*)pFunc);
}

bool node::call_rpc(const std::string& rpc_resource,
                    const std::string& input,
                    int& nResult) {
  std::string node_name = _ParseNodeName(rpc_resource);
  std::string function_name = _ParseRpcName(rpc_resource);
  msg::String req;
  msg::Int rep;
  req.set_value(input);
  bool bRes = call_rpc(node_name, function_name, req, rep);
  nResult = rep.value();
  return bRes;
}

bool node::call_rpc(const std::string& rpc_resource,
                    const google::protobuf::Message& msg_req,
                    google::protobuf::Message& msg_rep,
                    unsigned int time_out) {
  std::string node_name = _ParseNodeName(rpc_resource);
  std::string function_name = _ParseRpcName(rpc_resource);
  return call_rpc(node_name, function_name, msg_req, msg_rep, time_out);
}

bool node::call_rpc(const std::string& node_name,
                    const std::string& function_name,
                    const google::protobuf::Message&  msg_req,
                    google::protobuf::Message& msg_rep,
                    unsigned int time_out) {
  CHECK(init_done_);
  std::shared_ptr<TimedNodeSocket> socket;

  std::unique_lock<std::mutex> lock(mutex_);

  // make sure we know about this method
  std::string rpc_resource =  kRpcScheme + node_name + "/" + function_name;
  auto it = resource_table_.find(rpc_resource);
  if (it == resource_table_.end()) {
    // unknown method, fail
    return false;
  }
  std::string host_and_port = resource_table_[rpc_resource];

  // check if socket is already open for this host
  auto rpcit = rpc_.find(node_name);
  CHECK(rpcit != rpc_.end());

  const std::shared_ptr<RpcData>& data = rpcit->second;
  if (data->socket) {
    // socket is already open, lets use it
    socket = data->socket;
  } else {
    data->socket = std::make_shared<TimedNodeSocket>(
        NodeSocket(new zmq::socket_t(*context_, ZMQ_REQ)));

    // lets connect using the socket
    try {
      data->socket->socket->connect(("tcp://" + host_and_port).c_str());
    } catch(const zmq::error_t& error) {
      LOG(ERROR) << "Error connecting to " << host_and_port;
      return false;
    }
  }

  if (!data->socket->socket->connected()) {
    return false;
  }

  // once we have socket, we can let the resource table change. yay
  // smart pointers
  lock.unlock();

  bool success = call_rpc(socket->socket, &data->mutex,
                          function_name, msg_req, msg_rep, time_out);
  if (success) {
    socket->Tic();
  }
  return success;
}

bool node::call_rpc(NodeSocket socket,
                    std::mutex* socket_mutex,
                    const std::string& function_name,
                    const google::protobuf::Message& msg_req,
                    google::protobuf::Message& msg_rep,
                    unsigned int nTimeoutMS) {
  CHECK(init_done_);
  CHECK(socket->connected());

  // prepare to append function information (clip function name size)
  std::string sFName = function_name;
  if (sFName.size() > 254) {
    sFName.resize(254);
  }
  unsigned char n = sFName.size();

  // prepare message
  zmq::message_t ZmqReq(sizeof(n) + n + msg_req.ByteSize());
  std::memcpy(ZmqReq.data(), &n, sizeof(n));
  std::memcpy((char*)ZmqReq.data() + sizeof(n), sFName.c_str(), n);
  if (!msg_req.SerializeToArray((char*)ZmqReq.data() + sizeof(n) + n,
                                msg_req.ByteSize())) {
    // error serializing protobuf to ZMQ message
    return false;
  }

  // send request

  // We can only use this socket one thread at a time
  /** @todo Sockets should only be used by 1 thread ever... */
  std::lock_guard<std::mutex> lock(*socket_mutex);
  socket->setsockopt(ZMQ_SNDTIMEO,
                     &send_recv_max_wait_,
                     sizeof(send_recv_max_wait_));

  try {
    if (!socket->send(ZmqReq)) {
      LOG(ERROR) << "zmq::send return: " << strerror(errno);
      return false;
    }
  } catch(const zmq::error_t& error) {
    std::string sErr = error.what();
    LOG(ERROR) << " zmq->send() -- " << sErr;
    return false;
  }

  zmq::message_t ZmqRep;
  double dStartTime = _TicMS();

  socket->setsockopt(ZMQ_RCVTIMEO, &nTimeoutMS, sizeof(nTimeoutMS));

  try{
    bool bStatus = false;
    while(!bStatus) {
      if (nTimeoutMS == 0) { // block
        bStatus = socket->recv(&ZmqRep);
      }
      else{
        bStatus = socket->recv(&ZmqRep);
        double dTimeTaken = _TocMS(dStartTime);
        if (dTimeTaken >= nTimeoutMS) {
          // timeout... error receiving
          LOG(debug_level_) << "Warning: Call timed out waiting for reply ("
                            << dTimeTaken << " ms > " << nTimeoutMS << " ms).";
          return false;
        }
        usleep(100); // wait a bit
      }
    }
  } catch(const zmq::error_t& error) {
    std::string sErr = error.what();
    LOG(ERROR) << " zmq->recv() -- " << sErr;
    return false;
  }

  if (!msg_rep.ParseFromArray(ZmqRep.data(), ZmqRep.size())) {
    // bad protobuf format
    return false;
  }

  return true;
}

bool node::advertise(const std::string& sTopic) {
  CHECK(init_done_);
  std::string topic_resource = kTopicScheme + node_name_ + "/" + sTopic;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    // check if socket is already open for this topic
    auto it = topics_.find(topic_resource);
    if (it != topics_.end()) {
      LOG(ERROR) << "resource socket is already open, return false";
      return false;
    }
  }

  // no socket open.. lets open a new one
  // check if port is already in use
  NodeSocket socket(new zmq::socket_t(*context_, ZMQ_PUB));
  int port = _BindRandomPort(socket);
  std::string sAddr = _GetAddress(host_ip_, port);
  LOG(debug_level_) << "Publishing topic '" << sTopic << "' on " << sAddr;

  // update node table and socket table
  {
    auto data = std::make_shared<TopicData>();
    data->socket = socket;

    std::lock_guard<std::mutex> lock(mutex_);
    resource_table_[topic_resource] = sAddr;
    topics_[topic_resource] = data;
  }
  _PropagateResourceTable();
  return true;
}

bool node::publish(const std::string& sTopic,
                   const google::protobuf::Message& Msg) {
    zmq::message_t zmq_msg(Msg.ByteSize());
    if (!Msg.SerializeToArray(zmq_msg.data(), Msg.ByteSize())) {
      LOG(WARNING) << "Conversion of protobuf message to ZMQ message for topic "
                   << sTopic <<  " was not successfull";
      return false;
    }
    return publish(sTopic, zmq_msg);
}

bool node::publish(const std::string& sTopic, zmq::message_t& Msg) {
  CHECK(init_done_);
  std::string topic_resource = kTopicScheme + node_name_ + "/" + sTopic;

  // check if socket is already open for this topic
  auto it = topics_.find(topic_resource);
  if (it == topics_.end()) return false;

  const NodeSocket& socket = it->second->socket;

  std::lock_guard<std::mutex> lock(it->second->mutex);
  socket->setsockopt(ZMQ_SNDTIMEO,
                     &send_recv_max_wait_,
                     sizeof(send_recv_max_wait_));
  try {
    return socket->send(Msg);
  } catch(const zmq::error_t& error) {
    LOG(WARNING) << "Error sending the message for topic " << sTopic;
    return false;
  }
}

bool node::publish(const std::string& topic, const std::string& msg) {
  // No null termination on the wire, get's added when read out.
  zmq::message_t zmq_msg(msg.size());
  memcpy(zmq_msg.data(), msg.data(), msg.size());
  return publish(topic, zmq_msg);
}

bool node::subscribe(const std::string& resource) {
  CHECK(init_done_);
  std::string topic_resource = kTopicScheme + resource;
  std::string resource_ip;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    // check if socket is already open for this topic
    auto it = topics_.find(topic_resource);
    if (it != topics_.end()) {
      LOG(ERROR) << "subscription for that topic already exists";
      return false;
    }

    // lets find this node's IP
    auto its = resource_table_.find(topic_resource);
    if (its == resource_table_.end()) {
      LOG(debug_level_) << "Resource '" << topic_resource
                        << "' not found on cache table.";
      return false;
    }
    resource_ip = "tcp://" + its->second;
  }

  NodeSocket socket(new zmq::socket_t(*context_, ZMQ_SUB));
  // lets connect using the socket
  try {
    socket->setsockopt(ZMQ_SUBSCRIBE, NULL, 0);
    socket->connect(resource_ip.c_str());
  } catch(const zmq::error_t& error) {
    return false;
  }

  auto data = std::make_shared<TopicData>();
  data->socket = socket;

  std::lock_guard<std::mutex> lock(mutex_);
  topics_[topic_resource] = data;
  return true;
}

bool node::listen(const std::string& topic) {
  std::string endpoint = kListenScheme + node_name_ + "/" + topic;
  if (resource_table_.count(endpoint)) {
    LOG(ERROR) << "Already listening for " << endpoint;
    return false;
  }

  auto data = std::make_shared<ListenData>();
  data->socket.reset(new zmq::socket_t(*context_, ZMQ_SUB));
  data->socket->setsockopt(ZMQ_SUBSCRIBE, NULL, 0);
  int port = _BindRandomPort(data->socket);
  resource_table_[endpoint] = _GetAddress(host_ip_, port);
  listen_data_[topic] = data;
  _PropagateResourceTable();

  return true;
}

bool node::listen(const std::string& topic, uint16_t port) {
  std::string endpoint = kListenScheme + node_name_ + "/" + topic;
  if (resource_table_.count(endpoint)) {
    LOG(ERROR) << "Already listening for " << endpoint;
    return false;
  }

  auto data = std::make_shared<ListenData>();
  data->socket.reset(new zmq::socket_t(*context_, ZMQ_SUB));
  data->socket->setsockopt(ZMQ_SUBSCRIBE, NULL, 0);
  bool success = _BindPort(port, data->socket);
  if (success) {
    resource_table_[endpoint] = _GetAddress(host_ip_, port);
    listen_data_[topic] = data;
  }
  _PropagateResourceTable();
  return success;
}

bool node::send(const std::string& listener, const std::string& msg) {
  zmq::message_t zmq_msg(msg.size());
  memcpy(zmq_msg.data(), msg.data(), msg.size());
  return send(listener, &zmq_msg);
}

bool node::send(const std::string& listener,
                const google::protobuf::Message& msg) {
  int msg_size = msg.ByteSize();
  zmq::message_t zmq_msg(msg_size);
  if (!msg.SerializeToArray(zmq_msg.data(), msg_size)) {
    LOG(debug_level_) << "Conversion of protobuf message to ZMQ message for "
                      << listener <<  " was not successful.";
    return false;
  }
  return send(listener, &zmq_msg);
}

bool node::send(const std::string& listener, zmq::message_t* zmq_msg) {
  CHECK(init_done_);
  CHECK_NOTNULL(zmq_msg);
  std::string listening_resource = kListenScheme + listener;
  std::string dest_addr;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto res_it = resource_table_.find(listening_resource);
    if (res_it == resource_table_.end()) {
      LOG(ERROR) << "Attempting to send to non-existent listener " << listener;
      return false;
    }
    dest_addr = res_it->second;
  }

  auto it = send_data_.find(listening_resource);
  std::shared_ptr<SendData> send;
  if (it == send_data_.end()) {
    NodeSocket socket(new zmq::socket_t(*context_, ZMQ_PUB));

    std::string to_connect = "tcp://" + dest_addr;
    auto data = std::make_shared<SendData>();
    data->socket = socket;
    try {
      data->socket->connect(to_connect.c_str());
    } catch(const zmq::error_t& err) {
      LOG(ERROR) << "Failed to connect to listener @ " << to_connect;
      return false;
    }

    {
      std::lock_guard<std::mutex> lock(mutex_);
      send_data_[listening_resource] = data;
    }
    _PropagateResourceTable();
    return true;
  } else {
    send = it->second;
  }

  std::lock_guard<std::mutex> lock(send->mutex);
  const NodeSocket& socket = send->socket;
  socket->setsockopt(ZMQ_SNDTIMEO,
                     &send_recv_max_wait_,
                     sizeof(send_recv_max_wait_));
  try {
    return socket->send(*zmq_msg);
  } catch(const zmq::error_t& error) {
    LOG(WARNING) << "Error sending a message for listener " << listener;
    return false;
  }
}

bool node::receive(const std::string& resource,
                   google::protobuf::Message& Msg) {
    zmq::message_t zmq_msg;

    return receive(resource, zmq_msg) &&
        Msg.ParseFromArray(zmq_msg.data(), zmq_msg.size());
}

bool node::receive(const std::string& resource, std::string* msg) {
  CHECK_NOTNULL(msg);
  zmq::message_t zmq_msg;
  bool ret = receive(resource, zmq_msg);
  msg->assign(reinterpret_cast<const char*>(zmq_msg.data()), zmq_msg.size());
  return ret;
}

bool node::receive(const std::string& resource, zmq::message_t& zmq_msg) {
  CHECK(init_done_);
  std::string topic_resource = kTopicScheme + resource;

  // check if socket is already open for this topic
  auto topic_it = topics_.find(topic_resource);
  auto listen_it = listen_data_.find(resource);
  NodeSocket socket;
  std::shared_ptr<ListenData> listen_data;
  std::shared_ptr<TopicData> topic_data;

  std::unique_lock<std::mutex> lock;
  if (topic_it != topics_.end()) {
    topic_data = topic_it->second;
    socket = topic_data->socket;
    lock = std::unique_lock<std::mutex>(topic_data->mutex);
  } else if (listen_it != listen_data_.end()) {
    listen_data = listen_it->second;
    socket = listen_data->socket;
    lock = std::unique_lock<std::mutex>(listen_data->mutex);
  } else {
    LOG(debug_level_) << "Can't receive on topic '" << resource
                      << "'. Did you subscribe to it?";
    // no socket found
    return false;
  }

  socket->setsockopt(
      ZMQ_RCVTIMEO, &send_recv_max_wait_, sizeof(send_recv_max_wait_));
  try {
    return socket->recv(&zmq_msg);
  } catch(const zmq::error_t& error) {
    LOG(WARNING) << "Error receiving zmq packet: " << error.what();
    return false;
  }
  return true;
}

std::string node::_GetHostIP(const std::string& sPreferredInterface) {
  // orderd list of interfaces we perfer... all so
  // the interface matches what Zeroconf says.. this
  // is a hack. should instead re-map what zeroconf
  // says to some standard...
  std::vector<std::string> ifs;
  ifs.push_back(sPreferredInterface);
  ifs.push_back("eth");
  ifs.push_back("en");
  ifs.push_back("wlan");

  struct ifaddrs *ifaddr, *ifa;
  char host[NI_MAXHOST];

  std::vector<std::pair<std::string, std::string> > if_ips;

  if (getifaddrs(&ifaddr) == -1) {
    perror("getifaddrs");
  }

  for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
    if (ifa->ifa_addr == NULL) {
      continue;
    }
    int s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), host,
                        NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
    if (s == 0 && ifa->ifa_addr->sa_family == AF_INET) {
      if_ips.push_back(std::pair<std::string, std::string>(ifa->ifa_name,
                                                           host));
    }
  }
  freeifaddrs(ifaddr);

  std::string sIP = "127.0.0.1";
  for (const std::string& preferred_if : ifs) {
    for (const std::pair<std::string, std::string> ifip : if_ips) {
      // found a prefered interface
      if (ifip.first.find(preferred_if) != std::string::npos) {
        sIP = ifip.second;
      }
    }
  }
  return sIP;
}

void node::_HeartbeatFunc(
    msg::HeartbeatRequest& req,
    msg::HeartbeatResponse& rep, void* pUserData) {
  static_cast<node*>(pUserData)->HeartbeatFunc(req, rep);
}

void node::HeartbeatFunc(
    msg::HeartbeatRequest& req,
    msg::HeartbeatResponse& rep) {
  rep.set_checksum(_ResourceTableCRC());
  rep.set_version(resource_table_version_);

  // 1) if we are behind the remote client, do nothing
  // -- the next RPC call from the client will give
  // us an updated table.
  // 2) if client is behind, send him our table (TODO: send a diff instead)
  if (req.version() < resource_table_version_) {
    _BuildResourceTableMessage(*rep.mutable_resource_table());
  }
  // 3) if version match, do nothing -- this is the default case
}

void node::_GetResourceTableFunc(
    msg::GetTableRequest& req,
    msg::GetTableResponse& rep, void* pUserData) {
  static_cast<node*>(pUserData)->GetResourceTableFunc(req, rep);
}

void node::GetResourceTableFunc(
    msg::GetTableRequest& req,
    msg::GetTableResponse& rep) {
  msg::ResourceTable* pTable = rep.mutable_resource_table(); // add the resource table to the message
  pTable->set_version(resource_table_version_);
  pTable->set_checksum(_ResourceTableCRC());
  rep.set_sender_name(node_name_);
  for (auto it = resource_table_.begin(); it != resource_table_.end(); ++it) {
    msg::ResourceLocator* pMsg = pTable->add_urls();// add new url to end of table
    pMsg->set_resource(it->first);
    pMsg->set_address(it->second);
  }
  LOG(debug_level_) << "GetResourceTableFunc() called -- will send "
                    << rep.resource_table().urls_size() << " resouces back";
  _PrintResourceLocatorTable();
  // ok at this point the NodeTable protobuf is ready to send back to the caller
}

void node::_DeleteFromResourceTableFunc(msg::DeleteFromTableRequest& req,
                                        msg::DeleteFromTableResponse& rep,
                                        void* pUserData) {
  static_cast<node*>(pUserData)->DeleteFromResourceTableFunc(req, rep);
}

void node::DeleteFromResourceTableFunc(msg::DeleteFromTableRequest& req,
                                       msg::DeleteFromTableResponse& rep) {
  std::lock_guard<std::mutex> lock(mutex_); // careful

  LOG(debug_level_) << "DeleteFromResourceTableFunc() called by '"
                    << req.requesting_node_name() << "' to delete "
                    << req.urls_to_delete_size() << " resources";
  // _PrintResourceLocatorTable();
  for (int ii = 0; ii < req.urls_to_delete_size(); ++ii) {
    const msg::ResourceLocator& m = req.urls_to_delete(ii);
    auto it = resource_table_.find(m.resource());
    if (it != resource_table_.end()) {
      resource_table_.erase(it);
    }
  }

  // also remove from RPC socket map.
  auto rpcit = rpc_.find(req.requesting_node_name());
  if (rpcit != rpc_.end()) {
    // this is ok, because no one else can touch this stuff right now...
    rpc_.erase(rpcit);
  }

  // also remove from topics_,
  for (auto topicit = topics_.begin(); topicit != topics_.end(); ) {
    if (topicit->first.find(req.requesting_node_name()) != std::string::npos) {
      topics_.erase(topicit++);
    } else {
      ++topicit;
    }
  }

  // ok at this point we have updated our node table
  _PrintResourceLocatorTable();
  _PrintRpcSockets();
}

void node::_SetResourceTableFunc(msg::SetTableRequest& req,
                                 msg::SetTableResponse& rep, void* pUserData) {
  static_cast<node*>(pUserData)->SetResourceTableFunc(req, rep);
}

void node::SetResourceTableFunc(msg::SetTableRequest& req,
                                msg::SetTableResponse& rep) {
  LOG(debug_level_) << "SetResourceTableFunc() called by '"
                    << req.requesting_node_name() << "' to share "
                    << req.resource_table().urls_size() << " resources";

  // open RPC socket if necessary -- e.g. if the client is new:
  _ConnectRpcSocket(req.requesting_node_name(), req.requesting_node_addr());

  // verify that the new clients resource table is newer than ours
  std::lock_guard<std::mutex> lock(mutex_);
  if (req.resource_table().version() <= resource_table_version_) {
    LOG(WARNING) << "'" << req.requesting_node_name()
                 << "' sent an outdated resource table with version "
                 << req.resource_table().version();
  }

  // else all good, just adopt the remote resource table version
  resource_table_version_ = req.resource_table().version();

  //  update the rest of the table:
  for (int ii = 0; ii < req.resource_table().urls_size(); ++ii) {
    const msg::ResourceLocator& m = req.resource_table().urls(ii);
    resource_table_[m.resource()] = m.address();
  }

  // set appropraite reply to the remote calling client
  rep.set_version(resource_table_version_);
  rep.set_checksum(_ResourceTableCRC());

  // ok at this point we have updated our node table
  _PrintResourceLocatorTable();
  LOG(debug_level_) << "SetResourceTable complete";
}

void node::HeartbeatThread() {
  std::string sAddr = _GetAddress(host_ip_, port_);
  LOG(debug_level_) << "Starting Heartbeat Thread at " << sAddr;
  while(1) {
    bool should_sleep = true;


    // Copy it so we don't have to lock while heartbeating
    decltype(rpc_) sockets_copy;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      sockets_copy = rpc_;
    }

    // ping all nodes we haven't heard from recently
    for (auto it = sockets_copy.begin() ; it != sockets_copy.end(); ++it) {
      const std::shared_ptr<RpcData>& rpc_data = it->second;
      if (!rpc_data->socket) continue;

      double dElapsedTime = _Toc(rpc_data->socket->last_heartbeat_time);

      // haven't heard from this guy in a while
      if (dElapsedTime > heartbeat_wait_thresh_) {
        LOG(debug_level_) << "sending heartbeat to '" << it->first << "'";
        double timeout = get_resource_table_max_wait_ * 1e3;

        msg::HeartbeatRequest hbreq;
        msg::HeartbeatResponse hbrep;
        hbreq.set_checksum(_ResourceTableCRC());
        hbreq.set_version(resource_table_version_);
        if (!call_rpc(rpc_data->socket->socket, &rpc_data->mutex,
                      "Heartbeat", hbreq, hbrep, timeout)) {
          if (dElapsedTime > heartbeat_death_timeout_) {
            LOG(WARNING) << "Haven't heard from '" << it->first
                         << "' for " << dElapsedTime
                         << " ms seconds. Disconnecting.";
            DisconnectNode(it->first);
            should_sleep = false;
            break;  // From inner loop, the iterator is now invalid.
          }
          continue;
        }

        rpc_data->socket->Tic();

        // if remote resource table is newer than ours...
        if (hbrep.version() > resource_table_version_) {
          // ...update resource table with data from remote node:
          for (int ii = 0; ii < hbrep.resource_table().urls_size(); ++ii) {
            msg::ResourceLocator r = hbrep.resource_table().urls(ii);
            resource_table_[r.resource()] = r.address();
          }
          resource_table_version_ = hbrep.version();
        }

        // if our resource table is more current, send it out:
        if (hbrep.version() < resource_table_version_) {
          msg::SetTableRequest streq;
          msg::SetTableResponse strep;
          _BuildResourceTableMessage(*streq.mutable_resource_table());
          streq.set_requesting_node_name(node_name_);
          streq.set_requesting_node_addr(_GetAddress());
          if (!call_rpc(rpc_data->socket->socket, &rpc_data->mutex,
                        "SetResourceTable", streq, strep)) {
            LOG(ERROR) << "Sending resource table failed";
          }
        }

        /// serious error!
        if (_ResourceTableCRC() != hbrep.checksum()) {
          LOG(ERROR) << "Node '" << it->first << "' checksum error ( "
                     << _ResourceTableCRC() << " != "
                     << hbrep.checksum() << ")";
        }
      }
    }
    if (exiting_) {
      return;
    } else if (should_sleep) {
      usleep(10000);
    }
    if (exiting_) return;
  }
}

void node::RPCThread() {
  std::string sAddr = _GetAddress(host_ip_, port_);
  LOG(debug_level_) << "Starting RPC server at " << sAddr;

  while(1) {
    // wait for request
    zmq::message_t ZmqReq;

    try {
      static const int rpc_recv_max_wait_ = 3000;
      socket_->setsockopt(ZMQ_RCVTIMEO, &rpc_recv_max_wait_,
                          sizeof(rpc_recv_max_wait_));
      while (!socket_->recv(&ZmqReq)) {
        if (exiting_) return;
      }
    } catch(const zmq::error_t& error) {
      std::string sErr = error.what();
      LOG(ERROR) << "zmq->recv() -- " << sErr;
    }

    // obtain "header" which contains function name
    unsigned char FuncNameSize;
    memcpy(&FuncNameSize, ZmqReq.data(), sizeof(FuncNameSize));
    std::string FuncName(
        static_cast<char*>(ZmqReq.data()) + sizeof(FuncNameSize), FuncNameSize);

    LOG(debug_level_) << "Responding to '" << FuncName << "' RPC call";

    // prepare reply message
    unsigned int PbOffset = sizeof(FuncNameSize) + FuncNameSize;
    unsigned int PbByteSize = ZmqReq.size() - PbOffset;

    // look-up function
    auto rpc_it = rpc_.find(FuncName);
    if (rpc_it != rpc_.end() && rpc_it->second->rpc) {
      auto rpc = rpc_it->second->rpc;
      RPCFunction& Func = rpc->RpcFunc;
      const auto& Req = rpc->ReqMsg;
      const auto& Rep = rpc->RepMsg;

      Rep->Clear();

      if (!Req->ParseFromArray((char*)(ZmqReq.data()) + PbOffset, PbByteSize)) {
        continue;
      }

      // call function
      Func(*(Req), *(Rep), rpc->UserData);

      // send reply
      zmq::message_t ZmqRep(Rep->ByteSize());
      if (!Rep->SerializeToArray(ZmqRep.data(), Rep->ByteSize())) {
        // error serializing protobuf to ZMQ message
      }
      socket_->send(ZmqRep);
    } else {
      // send empty reply
      zmq::message_t ZmqRep(0);
      socket_->send(ZmqRep);
    }
  }
}

void node::TopicThread(const std::string& resource)
{
  //this resource is without TopicScheme.
  CHECK(init_done_);
  LOG(debug_level_) << "Thread for '" << resource << "' started.";

  std::string topic_resource = kTopicScheme + resource;

  const std::shared_ptr<TopicCallbackData>& callback_data =
      topics_[topic_resource]->callback;
  std::shared_ptr<google::protobuf::Message> msg = callback_data->callback_msg_;

  while(!exiting_) {
    if (receive(resource, *msg.get())) {
      callback_data->callback_(msg);
    }
    msg->Clear();
  }
  LOG(debug_level_) << "Thread for '" << resource << "' stopped.";
}

std::vector<std::string> node::GetSubscribeClientName() {
  std::lock_guard<std::mutex> lock(mutex_); // careful

  std::vector<std::string> vClientNames;
  for (auto it = topics_.begin(); it != topics_.end(); ++it) {
    if (it->first.find("StateKeeper") == std::string::npos) {
      std::string sSubString = it->first.substr(8, it->first.size());
      vClientNames.push_back(sSubString.substr(0, sSubString.find("/")));
    }
  }
  return vClientNames;
}

msg::ResourceTable node::_BuildResourceTableMessage(msg::ResourceTable& t) {
  for (auto it = resource_table_.begin(); it != resource_table_.end(); ++it) {
    msg::ResourceLocator* pMsg = t.add_urls();// add new url to end of table
    pMsg->set_resource(it->first);
    pMsg->set_address(it->second);
  }
  t.set_checksum(_ResourceTableCRC());
  t.set_version(resource_table_version_);
  return t;
}

uint32_t node::_ResourceTableCRC() {
  boost::crc_32_type crc;
  std::string vNames;
  for (auto it = resource_table_.begin() ; it != resource_table_.end(); ++it) {
    vNames += it->first;
  }
  crc.process_bytes(vNames.c_str(), vNames.size());
  return crc.checksum();
}

void node::_PropagateResourceTable() {
  LOG(debug_level_) << "Propagating resource table";
  msg::SetTableRequest req;
  msg::SetTableResponse rep;

  decltype(rpc_) tmp;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    tmp = rpc_;
  }

  _BuildResourceTableMessage(*req.mutable_resource_table());
  req.set_requesting_node_name(node_name_);
  req.set_requesting_node_addr(_GetAddress());

  for (auto it = tmp.begin(); it != tmp.end(); ++it) {
    if (!it->second->socket ||
        it->second->socket->socket == socket_) {
      continue; // don't send to self
    }

    LOG(debug_level_) << "Calling node '" << it->first
                      << "' SetResourceTable rpc method";
    if (!call_rpc(it->second->socket->socket, &it->second->mutex,
                  "SetResourceTable", req, rep)) {
      LOG(ERROR) << "sending resource table";
    }
  }
}

void node::_UpdateNodeRegistery() {
  if (!use_auto_discovery_ || !zero_conf_.IsValid()) return;

  std::vector<ZeroConfRecord> records;
  records = zero_conf_.BrowseForServiceType("_hermes._tcp");
  LOG(debug_level_) << "Looking for hermes.tcp ";
  while(records.empty()) {
    LOG(debug_level_)
        << "Waiting for _hermes._tcp to appear in ZeroConf registery";
    records = zero_conf_.BrowseForServiceType("_hermes._tcp");
    usleep(1000);
  }
  LOG(debug_level_) << "Found " << records.size() << " _hermes._tcp "
                    << "records in ZeroConf";
  // Report all the nodes registered with Avahi:
  std::vector<ZeroConfURL> urls;
  for (const ZeroConfRecord& r : records) {
    LOG(debug_level_) << "Resolving ZeroConf record: " << r;
    std::vector<ZeroConfURL> new_urls =
        zero_conf_.ResolveService(r.service_name, r.reg_type);
    urls.insert(urls.end(), new_urls.begin(), new_urls.end());
  }

  LOG(debug_level_) <<"Found " << urls.size() << " URLs for nodes";
  std::string sMyAddr = _GetAddress();
  for (ZeroConfURL& url : urls) {
    std::string sAddr = _GetAddress(url.host, url.port);
    if (sAddr == sMyAddr) { // don't send a message to ourselves...
      LOG(debug_level_) << "node at " << url << " (my URL)";
    } else{
      LOG(debug_level_) << "node at " << url;
    }
  }

  unsigned int uLatestResourceTableVersion = 0;
  msg::GetTableResponse rep;
  for (const ZeroConfURL& url : urls) {
    if (url.host == host_ip_ && url.port == port_) continue;

    // connect to the service, if we can
    if (!ConnectNode(url.host, url.port, &rep)) continue;

    uint32_t table_version = rep.resource_table().version();

    // keep track of incoming resource table versions
    if (uLatestResourceTableVersion == 0) { // catch the initial case
      uLatestResourceTableVersion = table_version;
    } else if (uLatestResourceTableVersion != table_version) {
      // error, in this case, we have received different resource table
      // versions from at least two different nodes... there is a conflict.
      LOG(WARNING) << "Resource table conflict";
    }
  }
  resource_table_version_ = uLatestResourceTableVersion + 1;
}

bool node::ConnectNode(const std::string& host, uint16_t port,
                       msg::GetTableResponse* rep) {
  LOG(debug_level_) << "Connecting to " << host << ":" << port;

  std::string zmq_addr = _ZmqAddress(host, port);
  NodeSocket socket(new zmq::socket_t(*context_, ZMQ_REQ));
  try {
    socket->connect(zmq_addr.c_str());
  } catch(const zmq::error_t& error) {
    LOG(ERROR) << "zmq->connect() -- " << error.what();
  }
  LOG(debug_level_) << "'" << node_name_ << "' connected to remote node: "
                    << host << ":" << port;

  /// update our registery
  msg::GetTableRequest req;
  req.set_requesting_node_name(node_name_);
  req.set_requesting_node_addr(_GetAddress());
  double timeout = get_resource_table_max_wait_ * 1e3;

  std::mutex mutex;
  if (!call_rpc(socket, &mutex,
                "GetResourceTable", req, *rep, timeout)) {
    LOG(ERROR) << "Failed when asking for remote node name";
    return false;
  }
  if (rep->sender_name() == node_name_) {
    LOG(WARNING) << "Tried to communicate with myself, node: '" << node_name_
                 << "'. Not connecting.";
    return false;
  }

  LOG(debug_level_) << "\tHeard back from '" << rep->sender_name()
                    << "' about " << rep->resource_table().urls_size()
                    << " resources";

  // ok, now we have the nodes name to record his socket
  auto rpc_data = std::make_shared<RpcData>();
  rpc_data->socket = std::make_shared<TimedNodeSocket>(socket);
  rpc_[rep->sender_name()] = rpc_data;

  // push these into our resource table:
  for (const msg::ResourceLocator& r : rep->resource_table().urls()) {
    resource_table_[r.resource()] = r.address();
  }
  _PropagateResourceTable();
  return true;
}

void node::DisconnectNode(const std::string& node_name) {
  msg::DeleteFromTableRequest req;
  BuildDeleteFromTableRequest(&req);

  std::string topic_res = kTopicScheme + node_name;
  std::string rpc_res = kRpcScheme + node_name;

  std::shared_ptr<TimedNodeSocket> socket;
  std::mutex* socket_mutex;

  {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto it = resource_table_.begin(); it != resource_table_.end(); ) {
      if (it->first.find(topic_res) != std::string::npos ||
          it->first.find(rpc_res) != std::string::npos) {
        resource_table_.erase(it++);
      } else {
        ++it;
      }
    }

    auto it = rpc_.find(node_name);
    if (it == rpc_.end()) return;

    socket = it->second->socket;
    socket_mutex = &it->second->mutex;
  }

  msg::DeleteFromTableResponse rep;
  call_rpc(socket->socket, socket_mutex,
           "DeleteFromResourceTable", req, rep);

  std::lock_guard<std::mutex> lock(mutex_);
  rpc_.erase(node_name);
}

void node::_PrintResourceLocatorTable() {
  LOG(debug_level_) << "--------------- RESOURCE TABLE (ver "
                    << resource_table_version_
                    << ", crc " << _ResourceTableCRC() << ") --------------";
  LOG(debug_level_) << "URL\t\tRESOURCE";
  for (auto it = resource_table_.begin() ; it != resource_table_.end(); ++it) {
    LOG(debug_level_) << it->first << "\t\t" << it->second;
  }
}

void node::_PrintRpcSockets() {
  LOG(debug_level_) << "----------- RPC SOCKETS OPEN ----------------";
  for (auto it = rpc_.begin() ; it != rpc_.end(); ++it) {
    LOG(debug_level_) << it->first;
  }
}

bool node::_BindPort(uint16_t port, const NodeSocket& socket) {
  try {
    std::ostringstream address;
    address << "tcp://*:" << port;
    socket->bind(address.str().c_str());
    return true;
  } catch(const zmq::error_t& error) {
    LOG(debug_level_) << "Failed to bind to port " << port << ": "
                      << error.what();
    return false;
  }
}

int node::_BindRandomPort(const NodeSocket& socket) {
  uint16_t port = 5555;
  while (!_BindPort(port, socket)) {
    ++port;
  }
  return port;
}

std::string node::_GetAddress() const {
  return _GetAddress(host_ip_, port_);
}

std::string node::_GetAddress(const std::string& host_ip,
                              const int port) const {
  std::ostringstream address;
  address << host_ip << ":" << port;
  return address.str();
}

std::string node::_ZmqAddress() const {
  return _ZmqAddress(host_ip_, port_);
}

std::string node::_ZmqAddress(const std::string& host_ip,
                              const int port) const {
  std::ostringstream address;
  address << "tcp://" << host_ip << ":" << port;
  return address.str();
}

double node::_Tic() {
  return node_time::Tic();
}

double node::_Toc(double dSec) {
  return node_time::Toc(dSec);
}

double node::_TicMS() {
  return node_time::Tic() * 1e3;
}

double node::_TocMS(double dMS) {
  return _TicMS() - dMS;
}

std::string node::_ParseNodeName(const std::string& resource) {
  std::string node_name;
  size_t found = resource.find_first_of('/');
  if (found != std::string::npos) {
    node_name = resource.substr(0, found);
  }
  return node_name;
}

std::string node::_ParseRpcName(const std::string& resource) {
  std::string rpc_name;
  size_t found = resource.find_first_of('/');
  if (found != std::string::npos) {
    rpc_name = resource.substr(found + 1);
  }
  return rpc_name;
}

void node::_ConnectRpcSocket(const std::string& node_name,
                             const std::string& node_addr) {
  // open RPC socket if necessary:
  auto rpcit = rpc_.find(node_name);
  if (rpcit == rpc_.end()) {
    std::string sZmqAddr = "tcp://" + node_addr;
    NodeSocket socket = NodeSocket(new zmq::socket_t(*context_, ZMQ_REQ));
    try {
      socket->connect(sZmqAddr.c_str());
    } catch(const zmq::error_t& error) {
      std::string sErr = error.what();
      LOG(ERROR) << "Error zmq->connect() -- " << sErr;
    }
    LOG(debug_level_) << "Connected to remote node: " << sZmqAddr;

    // record the new connection
    auto rpc_data = std::make_shared<RpcData>();
    rpc_data->socket = std::make_shared<TimedNodeSocket>(socket);;
    rpc_[node_name] = rpc_data;
  }
}

void node::NodeSignalHandler(int nSig) {
  for (size_t ii = 0; ii < g_vNodes.size(); ++ii) {
    g_vNodes[ii]->_BroadcastExit();
  }
  switch(nSig) {
    case SIGINT : LOG(ERROR) << "Caught SIGINT"; break;
    case SIGTERM: LOG(ERROR) << "Caught SIGTERM"; break;
    case SIGSTOP: LOG(ERROR) << "Caught SIGSTOP"; break;
    case SIGSEGV: LOG(ERROR) << "Caught SIGSEGV"; break;
    default:      LOG(ERROR) << "NodeSignalHandler caught "<< strsignal(nSig);
  }
  exit(-1);
}

/// collect all resources we provide
void node::BuildDeleteFromTableRequest(msg::DeleteFromTableRequest* msg) const {
  std::lock_guard<std::mutex> lock(mutex_);
  msg->set_requesting_node_name(node_name_);
  msg->set_requesting_node_addr(_GetAddress());
  for (const std::pair<std::string, std::string> res : resource_table_) {
    if (res.first.find(kRpcScheme + node_name_) != std::string::npos ||
        res.first.find(kTopicScheme + node_name_) != std::string::npos) {
      msg::ResourceLocator* pMsg = msg->add_urls_to_delete();
      pMsg->set_resource(res.first);
      pMsg->set_address(res.second);
    }
  }
}

void node::_BroadcastExit() {
  msg::DeleteFromTableRequest req;
  BuildDeleteFromTableRequest(&req);

  msg::DeleteFromTableResponse rep;

  // ask all known nodes to remove us:
  decltype(rpc_) tmp;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    tmp = rpc_;
  }
  for (const auto& pair : tmp) {
    const std::shared_ptr<RpcData>& rpc_data = pair.second;
    if (!rpc_data->socket ||
        !rpc_data->socket->socket->connected() ||
        rpc_data->socket->socket == socket_) {
      continue;
    }
    LOG(debug_level_) << "[Node '" << node_name_
                      << "']  Calling DeleteFromResource to remove "
                      << req.urls_to_delete_size()
                      <<" resources";
    if (!call_rpc(rpc_data->socket->socket, &rpc_data->mutex,
                  "DeleteFromResourceTable", req, rep)) {
      LOG(ERROR) << "Calling remote DeleteFromResourceTable";
    }
  }
}

void node::_IntStringFunc(msg::String& sStr,
                          msg::Int& nInt, void* pUserData) {
  int (*pFunc)(const std::string&) = (int (*)(const std::string&))pUserData;
  nInt.set_value((*pFunc)(sStr.value()));
}
}  // end namespace node
