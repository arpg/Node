#include "mex.h"
#include "class_handle.hpp"
#include "iostream"
// Our class' header file
#include "MatNodeExample.h"

/*********************************************************************
 *CONSTRUCTION AND DESTRUCTION OF OUR CLASS POINTERS
 **********************************************************************/

void mexFunction(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[])
{
  // Get the command string
  char cmd[64];
  if (nrhs < 1 || mxGetString(prhs[0], cmd, sizeof(cmd)))
    mexErrMsgTxt("First input should be a tring less than 64 characters long.");

  // New
  if (!strcmp("new", cmd)) {
    // Check parameters
    if (nlhs != 1)
      mexErrMsgTxt("New: One output expected.");
    // Define the class
    MatNodeExample* node_wrap = new MatNodeExample;
    plhs[0] = convertPtr2Mat<MatNodeExample>(node_wrap);
    void mexUnlock(void);
    return;
  }

  // Check there is a second input, which should be the class instance handle
  if (nrhs < 2)
    mexErrMsgTxt("Second input should be a class instance handle.");

  // Delete
  if (!strcmp("delete", cmd)) {
    destroyObject<MatNodeExample>(prhs[1]);
    if (nlhs != 0 || nrhs != 2)
      mexWarnMsgTxt("Delete: Unexpected arguments ignored.");
    return;
  }

  // Get the class instance pointer from the second input
  MatNodeExample *Node_instance = convertMat2Ptr<MatNodeExample>(prhs[1]);

  /*********************************************************************
   * USER-DEFINED FUNCTIONS
   * The first argument is always the function name, so start passing
   * arguments from prhs[2]
   **********************************************************************/

  if (!strcmp("StartConnections", cmd)) {
    std::cout<<"[node.mex] Starting connections."<<std::endl;
    double* num_sims = mxGetPr(prhs[2]);
    // Call the class pointer, and pass our argument.
    // Remember to cast!
    Node_instance->StartConnections(int(*num_sims));
    return;
  }

  if (!strcmp("CheckSimStatus", cmd)) {
    std::cout<<"[node.mex] Checking Sim Status..."<<std::endl;
    double* sim_num = mxGetPr(prhs[2]);
    double* status = Node_instance->CheckSimStatus(int(*sim_num));
    // Returning numbers
    // This matrix is 1x1, but modify it to take other sizes as you like.
    plhs[0] = mxCreateDoubleMatrix(1, 1, mxREAL);
    double* problem = mxGetPr(plhs[0]);
    // Modify the data, not the pointer to the data.
    *problem = status[0];
    return;
  }

  /**************************/

  // Got here, so command not recognized
  mexErrMsgTxt("Command not recognized.");

}
