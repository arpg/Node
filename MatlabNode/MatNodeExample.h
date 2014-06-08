#ifndef _MATNODEEXAMPLE_H
#define _MATNODEEXAMPLE_H

/*
 * File: MatNodeExample.h
 * Author: bminortx
 * This file defines the class that we'll use in MatNodeExample.cpp
 * as an easy way to move things in and out of MATLAB.
 * IMPORTANT NOTES:
 * - All functions nust either return a double* or void. Since buffers
 *   can have multiple parts, it's recommended to place the lengths of
 *   each part as the first elements of the double* array, so that each
 *   part can be returned as its own array in MATLAB
 */

#include <unistd.h>
#include <cstdlib>
#include "Node.h"
// Our protobuf sources - I couldn't find a successful way to import these
// using the makefile, so #includes had to do.
#include <PbMsgs/BVP.pb.h>
#include "/Users/Trystan/Code/rslam/build/CoreDev/HAL/PbMsgs/BVP.pb.cc"

class MatNodeExample{
 public:

  /// CONSTRUCTOR
  MatNodeExample(){
    node_name_ = "MATLAB";
    node_ad_ = "GetMsg";
    node_.init(node_name_);
  }

  void StartConnections(int num_sims){
    for (int i = 0; i < num_sims; i++) {
      std::string sim_name = "Sim"+std::to_string(i);
      /// The above allows us to connect to multiple Nodes, as long as
      /// they start with "Sim"
      node_.advertise(node_ad_+std::to_string(i));
      std::cout << node_name_ << "/" << node_ad_ + std::to_string(i)
                << " successfully advertised." << std::endl;
      while (!node_.subscribe(sim_name+"/SendMsg")) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
    }
  }

  ////////////////////////
  /// CHECK STATUS OF SIMS
  ////////////////////////

  double* CheckSimStatus(int sim_number){
    // give and take should never both be true.
    double* status = new double[1];
    status[0] = 0;
    pb::BVP_check sim_needs_bvp;
    if (node_.receive("Sim"+std::to_string(sim_number)+"/SendMsg",
                     sim_needs_bvp)) {
      bool give = sim_needs_bvp.need();
      if (give == true) {
        status[0] = 1;
      }
    }
    return status;
  }

  ////////////////////////
  /// VECTOR MANIPULATORS
  /// Very handy for big calculations; MATLAB must take a double*, but
  /// your class functions shouldn't have to do that, too.
  ////////////////////////

  double* Vector2Double(const std::vector<double>& vect){
    double* new_doub = new double[vect.size()];
    for (int ii = 0; ii < vect.size(); ii++) {
      new_doub[ii] = vect.at(ii);
    }
    return new_doub;
  }

  ///////

  std::vector<double> Double2Vector(double* doub, int size){
    std::vector<double> new_vect;
    for (int ii = 0; ii < size; ii++) {
      new_vect.push_back(doub[ii]);
    }
    return new_vect;
  }

  //////////////
  /// MEMBER VARIABLES
  //////////////

  node::node node_;
  std::string node_name_;
  std::string node_ad_;

};

#endif // _MATNODEEXAMPLE_H
