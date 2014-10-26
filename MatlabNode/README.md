MATLAB connection for Node library
====================================

This directory serves as an example of how to build a connection between a C++
class and the MATLAB computation library. It runs on the MEX C++ interface
designed by MATLAB. If you design the code correctly, the interface works
remarkably well, though it can be tricky. I recommend that this interface be
used for personal testing only, until a more distributed design can be solved. 

BUILDING AND RUNNING THE DEMO
===================================

This demo serves as a basic outline for any MATLAB-Node interface. It
instantiates n nodes on the C++ side, connects them all to a node on the MATLAB
side, and sends random integers back and forth. As noted, there are two main
parts: The MATLAB side and the C++ side.

Build
------
MATLAB side: Run

	make

in the top directory to create the .mex file that MATLAB uses to communicate
with the class found in NodeSim.h

C++ side:

	mkdir build
	cd build
	cmake ..
	make -j
	
This creates the program NodeSim, which is merely a wrapper for a protobuf.

Run
------
1. Modify the Makefile to fit your libraries and MATLAB version
2. Medify MatNodeExample.h's #include for Protobufs to fit your directory
3. Initialize a NodeSim instance. This starts a node thread running. Create as
   many as you'd like (in different windows), but make sure to name them in
   cardinal order:

	./NodeSim Sim0
	./NodeSim Sim1
	....
	
4. Start a MATLAB thread. In MATLAB, run

    node = MatNode(<num_sims>)

where <num_sims> is the number of NodeSims that you have currently spinning.
This starts the node instance for MATLAB and connects it (both advertising and
subscribing) to each NodeSim. Next, run

	node.Run

This will start checking each instance for an integer, 0 or 1, and printing
what it received.

MODIFYING THE CODE
==================

The code distribution is as follows:

- MATLAB side:
  - MatNode.m: Holds the top-level MATLAB class. When started, it initializes an instance of the class held in MatNodeExample.
  - MatNodeExample.cpp: Holds the MEX interface between MatNode.m and MatNodeExample.h. Communicates between the two
  - MatNodeExample.h: Describes our class functions, and iniializes a node thread when created.
  - class_handle.hpp: DO NOT MODIFY. Helps MEX interact with C++ classes.
  - Makefile: Compiles our MEX file for use.

If changing MatNodeExample.h, be mindful that you must change
MatNodeExample.cpp as well. Only call functions from within the MATLAB class
you create in MatNode; don't call MatNodeExample directly. If you have
compilation errors after compiling, make sure to read the comments within
Makefile. They probably have the answer to your question.

- C++ side:
  - NodeSim.h and NodeSim.cpp: Creates the program NodeSim. A regular class structure and nothing special, except that it calls the MATLAB node instance
  - CMakeLists.txt: builds NodeSim.

This is just like a regular C++ program, and can be modified accordingly.

NOTES AND CAVEATS
=================

1. DO NOT USE BOOST. MATLAB comes with its own version of the Boost libraries. Sadly, they're a bit outdated, but MATLAB will go there before it goes to your library. THERE IS NO WAY TO CHANGE THIS BEHAVIOR, other than renaming every one of MATLAB's Boost libraries in its source code, which will invariably break something else. So, just use the std libraries if you can afford to.
2. Since MATLAB will not print out anything in the C++ code, a handy trick is just to start MATLAB without the GUI attached. Add the following to your .bashrc file:

	alias matlabnogui='/Applications/MATLAB_R2013a.app/bin/matlab -nodesktop -nosplash'

Restart terminal and type 'matlabnogui' to start a command-line instance of MATLAB. Now, when you construct and run the MATLAB class, you can see all C++ communication.
3. Protobufs must be added within the class file that they are used in. If you find a way to add them directly in the Makefile, please let me know; otherwise, this is the only way that I have found to go around a major compilation issue.

****************************

AUTHOR: Brandon Minor


