//
//  gl_frontEnd.h
//
//  Created by Jean-Yves Hervé on 2017-04-24.
//

#ifndef GL_FRONT_END_H
#define GL_FRONT_END_H

#include <vector>

//-----------------------------------------------------------------------------
//	Data types
//-----------------------------------------------------------------------------

//	Travel direction data type
//	Note that if you define a variable
//	TravelDirection dir = whatever;
//	you get the opposite directions from dir as (NUM_TRAVEL_DIRECTIONS - dir)
//	you get left turn from dir as (dir + 1) % NUM_TRAVEL_DIRECTIONS
enum TravelDirection {
						NORTH = 0,
						WEST,
						SOUTH,
						EAST,
						//
						NUM_TRAVEL_DIRECTIONS
};

//	The 
enum TravelerType {
								RED_TRAV = 0,
								GREEN_TRAV,
								BLUE_TRAV,
								//
								NUM_TRAV_TYPES
};

//	Example of Traveler thread info data type
struct TravelerInfo {
						TravelerType type;
						//	location of the traveler
						int row;
						int col;
						//	in which direction is the traveler going?
						TravelDirection dir;
						// initialized to true, set to false if terminates
						bool isLive;
						pthread_t threadId;
};


//-----------------------------------------------------------------------------
//	Function prototypes
//-----------------------------------------------------------------------------

void drawGrid(int**grid, int numRows, int numCols);
void drawGridAndTravelers(int**grid, int numRows, int numCols, std::vector<TravelerInfo>& travelerList);
void drawState(int numLiveThreads, int redLevel, int greenLevel, int blueLevel);
void initializeFrontEnd(int argc, char** argv, void (*gridCB)(void), void (*stateCB)(void));
void cleanupAndQuit();
void faster();
void slower();

void slowdownProducers();
void speedupProducers();
#endif // GL_FRONT_END_H

