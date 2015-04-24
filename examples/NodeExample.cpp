/*

  Generic node example will get it's name from the command line, as well as
  the number of seconds to stay alive.  This can be used inside a script to
  test the effect of running many nodes at once.

*/

#include "ExampleMessage.pb.h"
#include <node/Node.h>

#include <stdio.h>
#include <chrono>
#include <thread>

using namespace std;

///////////////////////////////////////////////////////////
int main( int argc, char** argv )
{
  if( argc != 4 ){
    printf("USAGE: %s <name> <uptime> <verbosity 0-9>\n", argv[0] );
    return -1;
  }

  string sNodeName = argv[1];
  int nUpTime      = atoi(argv[2]);
  int nVerbosity   = atoi(argv[3]);

  // initialize node
  node::node n;
  n.set_verbosity(nVerbosity); // be a bit noisy
  if( n.init(sNodeName) == false ){
    return -1;
  }

  std::string sSpinner = "|/-\\";
  double dt = 0.2;
  int ii = 0;
  for( double t = 0.0; t < nUpTime; t+=dt ){
    cout << "\b\b" << sSpinner[ii++%4] << flush;
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  printf( "'%s' Exiting cleanly\n", sNodeName.c_str() );

  return 0;
}
