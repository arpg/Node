#include "NodeSim.h"

#include <thread>

NodeSim::NodeSim(){
}

/////////////////////////////////////////////

/// DESTRUCTOR
NodeSim::~NodeSim(){
}

void NodeSim::Init(){
  node_.init(sim_planner_name_);
  std::cout<<"-------------------"<<GetNumber(sim_planner_name_)<<std::endl;
  while(!node_.subscribe("MATLAB/GetMsg"+GetNumber(sim_planner_name_))){
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  node_.advertise("SendMsg");
}

////////

std::string NodeSim::GetNumber(std::string name){
  std::size_t found = name.find("m");
  if(found!=std::string::npos){
    return name.substr(found+1);
  }
  return "NULL";
}

/***********************************
 * THE MAIN LOOP
 * Initializes NodeSim, Runs the optimizer, and returns the path.
 * Never stops (though it does have a sleep time); instead, if it's done with
 * one path, it destructs the NodeSim and creates a new one with the
 * new info it's fed.
 **********************************/

int main(int argc, char** argv){
  NodeSim* sim = new NodeSim();
  std::string name = argv[1];
  sim->sim_planner_name_ = name;
  std::string number = sim->GetNumber(sim->sim_planner_name_);
  std::cout<<"Sim planner name is "<<sim->sim_planner_name_<<std::endl;
  sim->Init();
  while(1){
    pb::BVP_check ineed_bvp;
    int rand_num = rand() % 2;
    if (rand_num == 1) {
      ineed_bvp.set_need(true);
    } else {
      ineed_bvp.set_need(false);
    }
    while(!sim->node_.publish("SendMsg", ineed_bvp)){
      std::cout << "Sending " << rand_num << std::endl;
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  }
}
