#ifndef NODESIM_H
#define NODESIM_H

////////////////////////////////////////////
/// NODE_SIM
/// NodeSim is designed so that more than one of them can
/// be running at the same time. Start this in the command line:
/// >> ./NodeSim Sim0
/// ...and then start a second in another terminal:
/// >> /.NodeSim Sim1
/// They should now both be spinning, waiting for MATLAB's Node
/// instance to find them.
/////////////////////////////////////////////

#include <iostream>

#include "Node/Node.h"
#include "PbMsgs/BVP.pb.h"

class NodeSim
{
 public:
  NodeSim();
  ~NodeSim();

  /// FUNCTIONS
  void Init();
  std::string GetNumber(std::string name);

  //member variables
  std::string           sim_planner_name_;
  node::node            node_;
};

#endif // NODESIM_H
