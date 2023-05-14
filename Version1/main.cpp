//
//  main.cpp
//  GL travelers
//

// g++ -Wall -std=c++20 main.cpp gl_frontEnd.cpp -framework OpenGL -framework GLUT -o travel

 /*-------------------------------------------------------------------------+
 |	A graphic front end for a grid+state simulation.						|
 |																			|
 |	This application simply creates a glut window with a pane to display	|
 |	a colored grid and the other to display some state information.			|
 |	Sets up callback functions to handle menu, mouse and keyboard events.	|
 |	Normally, you shouldn't have to touch much if anything at all in this 	|
 |	code, unless you want to change some of the things displayed, add 		|
 |	menus, etc.																|
 |	Only mess with this after everything else works and making a backup		|
 |	copy of your project.  OpenGL & glut are tricky and it's really easy	|
 |	to break everything with a single line of code.							|
 |																			|
 |	Current GUI:															|
 |		- 'ESC' --> exit the application									|
 |		- 'r' --> add red ink												|
 |		- 'g' --> add green ink												|
 |		- 'b' --> add blue ink												|
 +-------------------------------------------------------------------------*/

#include <thread>
#include <random>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <unistd.h>
#include <iostream>
#include <tuple>
#include <fstream>
#include <mutex>
//
#include "glPlatform.h"
#include "gl_frontEnd.h"

using namespace std;

//==================================================================================
//	Function prototypes
//==================================================================================
void displayGridPane(void);
void displayStatePane(void);
void initializeApplication(void);

//==================================================================================
//	Application-level global variables
//==================================================================================
void makeTravelers();
void travelerThreadFunc(TravelerInfo *traveler);
void colorTrailUp(TravelerInfo *traveler);
void colorTrailDown(TravelerInfo *traveler);
void colorTrailLeft(TravelerInfo *traveler);
void colorTrailRight(TravelerInfo *traveler);

void faster();
void slower();

void speedupProducers(void);
void slowdownProducers(void);

void producerRedThreadFunc();
void producerGreenThreadFunc();
void producerBlueThreadFunc();

void readPipe(std::string pipePath);

std::tuple<int, int> getTargetCordinate(TravelerInfo* traveler, TravelDirection newDir, int newRow, int newCol);

//	Don't touch
extern int	GRID_PANE, STATE_PANE;
extern int	gMainWindow, gSubwindow[2];
bool DRAW_COLORED_TRAVELER_HEADS = true;

//	The state grid and its dimensions
int** grid;
int num_rows = 20, num_cols = 20;

//	the number of live threads (that haven't terminated yet)
int num_threads = 10;
int numLiveThreads = 0;

//	the ink levels
int MAX_LEVEL = 50;
int MAX_ADD_INK = 10;
int redLevel = 20, greenLevel = 30, blueLevel = 40;

//	ink producer sleep time (in microseconds)
//	[min sleep time is arbitrary]
const int MIN_SLEEP_TIME = 30000;
int producerSleepTime = 100000;

// Define the color increment
int colorIncrement = 32;

vector<TravelerInfo> travelerList;
std::vector<std::thread> travelerThreads;

std::vector<std::thread> producerRedThreads;
std::vector<std::thread> producerGreenThreads;
std::vector<std::thread> producerBlueThreads;

std::mutex gridLock, redInkLock, blueInkLock, greenInkLock, refillRedLock, refillBlueLock, refillGreenLock;

const int CORNER_DISTANCE = 1;
unsigned int stime = 500000;

random_device myRandDev;
default_random_engine myEngine(myRandDev());

std::string pipePath = "/tmp/travpipe";
//==================================================================================
//	These are the functions that tie the simulation with the rendering.
//	Some parts are "don't touch."  Other parts need your intervention
//	to make sure that access to critical section is properly synchronized
//==================================================================================


void displayGridPane(void)
{
	//	This is OpenGL/glut magic.  Don't touch
	glutSetWindow(gSubwindow[GRID_PANE]);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	//---------------------------------------------------------
	//	This is the call that makes OpenGL render the grid.
	//
	//	You *must* synchronize this call.
	//---------------------------------------------------------
	//	Use this drawing call instead
	gridLock.lock();
	drawGridAndTravelers(grid, num_rows, num_cols, travelerList);
	gridLock.unlock();

	//	This is OpenGL/glut magic.  Don't touch
	glutSwapBuffers();	
	glutSetWindow(gMainWindow);
}

void displayStatePane(void)
{
	//	This is OpenGL/glut magic.  Don't touch
	glutSetWindow(gSubwindow[STATE_PANE]);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	//---------------------------------------------------------
	//	This is the call that makes OpenGL render information
	//	about the state of the simulation.
	//
	//	You *must* synchronize this call.
	//
	//---------------------------------------------------------
	drawState(numLiveThreads, redLevel, greenLevel, blueLevel);
		
	//	This is OpenGL/glut magic.  Don't touch
	glutSwapBuffers();	
	glutSetWindow(gMainWindow);
}

//------------------------------------------------------------------------
//	These are the functions that would be called by a traveler thread in
//	order to acquire red/green/blue ink to trace its trail.
//	You *must* synchronize access to the ink levels
//------------------------------------------------------------------------
//
bool acquireRedInk(int theRed)
{
	bool ok = false;
	redInkLock.lock();
	if (redLevel >= theRed)
	{
		redLevel -= theRed;
		ok = true;
	}
	redInkLock.unlock();
	return ok;
}

bool acquireGreenInk(int theGreen)
{
	bool ok = false;
	greenInkLock.lock();
	if (greenLevel >= theGreen)
	{
		greenLevel -= theGreen;
		ok = true;
	}
	greenInkLock.unlock();
	return ok;
}

bool acquireBlueInk(int theBlue)
{
	bool ok = false;
	blueInkLock.lock();
	if (blueLevel >= theBlue)
	{
		blueLevel -= theBlue;
		ok = true;
	}
	blueInkLock.unlock();
	return ok;
}

//------------------------------------------------------------------------
//	These are the functions that would be called by a producer thread in
//	order to refill the red/green/blue ink tanks.
//	You *must* synchronize access to the ink levels
//------------------------------------------------------------------------
//
bool refillRedInk(int theRed)
{
	bool ok = false;
	refillRedLock.lock();
	if (redLevel + theRed <= MAX_LEVEL)
	{
		redLevel += theRed;
		ok = true;
	}
	refillRedLock.unlock();
	return ok;
}

bool refillGreenInk(int theGreen)
{
	bool ok = false;
	refillGreenLock.lock();
	if (greenLevel + theGreen <= MAX_LEVEL)
	{
		greenLevel += theGreen;
		ok = true;
	}
	refillGreenLock.unlock();
	return ok;
}

bool refillBlueInk(int theBlue)
{
	bool ok = false;
	refillBlueLock.lock();
	if (blueLevel + theBlue <= MAX_LEVEL)
	{
		blueLevel += theBlue;
		ok = true;
	}
	refillBlueLock.unlock();
	return ok;
}

void faster() {
	if (stime > 11) stime = 9 * stime / 10;
}

void slower() {
	stime = 11 * stime / 10;
}
//------------------------------------------------------------------------
//	You shouldn't have to touch this one.  Definitely if you don't
//	add the "producer" threads, and probably not even if you do.
//------------------------------------------------------------------------
void speedupProducers(void)
{
	//	decrease sleep time by 20%, but don't get too small
	int newSleepTime = (8 * producerSleepTime) / 10;
	
	if (newSleepTime > MIN_SLEEP_TIME)
	{
		producerSleepTime = newSleepTime;
	}
}

void slowdownProducers(void)
{
	//	increase sleep time by 20%
	producerSleepTime = (12 * producerSleepTime) / 10;
}

void readPipe(std::string pipe_path) 
{
	while (true) 
	{
		std::ifstream pipeStream(pipe_path);
		if (!pipeStream.is_open()) 
		{
			std::cerr << "Failed to open named pipe" << std::endl;
			return;
		}

		std::string command;
		std::getline(pipeStream, command);

		// TODO: Handle command
		if (command == "r")
		{
			while(!refillRedInk(3 * MAX_ADD_INK));
		}
		else if (command == "g")
		{
			while(!refillGreenInk(3 * MAX_ADD_INK));
		}
		else if (command == "b")
		{
			while(!refillBlueInk(3 * MAX_ADD_INK));
		}
		else if (command == "end") 
		{
			exit(0);
		}
    }
    // pipeStream.close();
}

//------------------------------------------------------------------------
//	You shouldn't have to change anything in the main function
//------------------------------------------------------------------------
int main(int argc, char** argv)
{
    // Verify that three arguments were passed
    if (argc != 4) 
	{
        std::cerr << "Usage: " << argv[0] << " <num_cols> <num_rows> <num_threads>\n";
        return 1;
    }

    // Parse arguments and check for validity
    num_cols = std::atoi(argv[1]);
    num_rows = std::atoi(argv[2]);
    num_threads = std::atoi(argv[3]);

    if (num_cols <= 1 || num_rows <= 1 || num_threads <= 0) 
	{
        std::cerr << "Invalid arguments. num_cols and num_rows must be larger than 5.\n";
        return 1;
    }

	initializeFrontEnd(argc, argv, displayGridPane, displayStatePane);

	//	Now we can do application-level
	initializeApplication();

    std::thread readerThread(readPipe, pipePath);
    // readerThread.join();

	//	Now we enter the main loop of the program and to a large extend
	//	"lose control" over its execution.  The callback functions that 
	//	we set up earlier will be called when the corresponding event
	//	occurs
	glutMainLoop();
	
	// in fact never called.
	cleanupAndQuit();
		
	//	This will never be executed (the exit point will be in one of the
	//	call back functions).
	return 0;
}


//==================================================================================
//
//	This is a part that you have to edit and add to.
//
//==================================================================================

void cleanupAndQuit()
{
	//	You would want to join all the threads before you free the grid and other
	//	allocated data structures.  You may run into seg-fault and other ugly termination
	//	issues otherwise.

	//	just nicer.  Also, if you crash there, you know something is wrong
	//	in your code.
	for (int i=0; i< num_rows; i++)
		delete []grid[i];
	delete []grid;
	
	exit(0);
}

void initializeApplication(void)
{
	//	Allocate the grid
	grid = new int*[num_rows];
	for (int i=0; i<num_rows; i++)
		grid[i] = new int[num_cols];
	
	//---------------------------------------------------------------
	//	The code block below to be replaced/removed
	//	I initialize the grid's pixels to have something to look at
	//---------------------------------------------------------------

	//	I don't want my squares to be too dark
	const unsigned char minVal = (unsigned char) 40;
	//	I create my random distribution local to this function because 
	//		1. It's used only once in the program's lifetime, here.
	//		2. This code is meant to go away.
	uniform_int_distribution<unsigned char> colorDist(minVal, 255);
	
	//	create RGB values (and alpha  = 255) for each pixel
	for (int i=0; i<num_rows; i++)
	{
		for (int j=0; j<num_cols; j++)
		{
			grid[i][j] = 0xFF000000;
		}	
	}

	//---------------------------------------------------------------
	//	You're going to have to properly initialize your travelers at random locations
	//	on the grid:
	//		- not ata  corner
	//		- not at the same location as an existing traveler
	//---------------------------------------------------------------
	makeTravelers();

    for (int k = 0; k < num_threads; k++)
	{
        travelerThreads.push_back(std::thread(travelerThreadFunc, &travelerList[k]));
		numLiveThreads ++;
    }

    for (int k = 0; k < 3; k++)
	{
        producerRedThreads.push_back(std::thread(producerRedThreadFunc));
		producerGreenThreads.push_back(std::thread(producerGreenThreadFunc));
		producerBlueThreads.push_back(std::thread(producerBlueThreadFunc));
    }
}

// add red ink to its tank
void producerRedThreadFunc()
{
	while (true)
	{
		usleep(producerSleepTime);
		refillRedInk(MAX_ADD_INK);
	}
}

// add green ink to its tank
void producerGreenThreadFunc()
{
	while (true)
	{
		usleep(producerSleepTime);
		refillGreenInk(MAX_ADD_INK);
	}
}

// add blue ink to its tank
void producerBlueThreadFunc()
{
	while (true)
	{
		usleep(producerSleepTime);
		refillBlueInk(MAX_ADD_INK);
	}
}

// function executed by each traveler thread
void travelerThreadFunc(TravelerInfo *traveler) 
{
	
	while (traveler->isLive)
	{
		// random perpendicular direction
		int currDir  = static_cast<int>(traveler->dir);
		TravelDirection newDir;

		std::uniform_int_distribution<int> dirss(0, NUM_TRAVEL_DIRECTIONS - 1);
		do
		{
			newDir = static_cast<TravelDirection>(dirss(myEngine));
		}
		while (newDir == currDir || abs(newDir - currDir) == 2 || (newDir == NORTH && traveler->row == 0) || (newDir == SOUTH && traveler->row == num_rows - 1) || ((newDir == WEST && traveler->col == 0)) || ((newDir == EAST && traveler->col == num_cols - 1)) );

		auto myTuple = getTargetCordinate(traveler, newDir, traveler->row, traveler->col);
		int newRow = std::get<0>(myTuple);
		int newCol = std::get<1>(myTuple);
		
		while (traveler->row != newRow) 
		{
			traveler->dir = newDir;
			if (traveler->row < newRow)
				colorTrailUp(traveler);

			else if (traveler->row > newRow)
				colorTrailDown(traveler);

			usleep(stime);
		}

		while (traveler->col != newCol)
		{
			traveler->dir = newDir;
			if (traveler->col < newCol)
				colorTrailLeft(traveler);

			else if (traveler->col > newCol)
				colorTrailRight(traveler);

			usleep(stime);
		}
		
		if ((traveler->row == 0 && traveler->col == 0) || (traveler->row == 0 && traveler->col == num_cols - 1) || (traveler->row == num_rows - 1 && traveler->col == 0) || (traveler->row == num_rows - 1 && traveler->col == num_cols - 1))
			traveler->isLive = false; 
	}
}

// make travelers and push them into our list of travelers
void makeTravelers() 
{

	for (int k=0; k< num_threads; k++)
	{
		TravelerInfo traveler;
		uniform_int_distribution<int> ttypes(0, NUM_TRAV_TYPES - 1);

		traveler.type = (TravelerType) ttypes(myEngine);
		do 
		{
			uniform_int_distribution<int> rowDist(CORNER_DISTANCE, num_rows-CORNER_DISTANCE);
			traveler.row = rowDist(myEngine);
			uniform_int_distribution<int> colDist(CORNER_DISTANCE, num_cols-CORNER_DISTANCE);
			traveler.col = colDist(myEngine);
		} 
		while (std::find_if(travelerList.begin(), travelerList.end(), [&](const TravelerInfo& other) { return other.row == traveler.row && other.col == traveler.col; }) != travelerList.end());

		uniform_int_distribution<int> dirDist(0, NUM_TRAVEL_DIRECTIONS-1);
		traveler.dir = static_cast<TravelDirection>(dirDist(myEngine));
	
		traveler.isLive = true;
		travelerList.push_back(traveler);
	}
}

// get the target position of the traveler coordinates based on its direction.
std::tuple<int, int> getTargetCordinate(TravelerInfo* traveler, TravelDirection newDir, int newRow, int newCol) 
{
	int lengthr, lengthc;
	
	// random displacement length
	std::uniform_int_distribution<int> lengthDistn(1, traveler->row);
	std::uniform_int_distribution<int> lengthDistw(1, traveler->col);
	std::uniform_int_distribution<int> lengthDists(1, (num_rows - traveler->row - 1));
	std::uniform_int_distribution<int> lengthDiste(1, (num_cols - traveler->col - 1));

	switch (newDir)
	{
	case NORTH:
		lengthr = lengthDistn(myEngine);
		newRow -= lengthr;
		break;
	case WEST:
		lengthc = lengthDistw(myEngine);
		newCol -= lengthc;
		break;
	case SOUTH:
		lengthr = lengthDists(myEngine);
		newRow += lengthr;
		break;
	case EAST:
		lengthc = lengthDiste(myEngine);
		newCol += lengthc;
		break;
	default:
		break;
	}
	return std::make_tuple(newRow, newCol);
}

// updates the traveler left and leave a color trail right
void colorTrailRight(TravelerInfo *traveler) 
{
	int new_color;
	switch (traveler->type)
	{
	case RED_TRAV:
		while (!acquireRedInk(1)) usleep(1000);
		gridLock.lock();
		traveler->col--;

		new_color = (grid[traveler->row][traveler->col + 1] & 0xFF) + colorIncrement;
		if (new_color > 255) new_color = 255;
			
		grid[traveler->row][traveler->col + 1] = grid[traveler->row][traveler->col + 1] | new_color;

		gridLock.unlock();
		break;
	case GREEN_TRAV:
		while (!acquireGreenInk(1)) usleep(1000);
		gridLock.lock();
		traveler->col--;

		new_color = (grid[traveler->row][traveler->col + 1] >> 8 & 0xFF) + colorIncrement;
		if (new_color > 255) new_color = 255;

		grid[traveler->row][traveler->col + 1] = grid[traveler->row][traveler->col + 1] | (new_color << 8);

		gridLock.unlock();
		break;
	case BLUE_TRAV:
		while (!acquireBlueInk(1)) usleep(1000);
		gridLock.lock();
		traveler->col--;

		new_color = (grid[traveler->row][traveler->col + 1] >> 16 & 0xFF) + colorIncrement;
		if (new_color > 255) new_color = 255;
	
		grid[traveler->row][traveler->col + 1] = grid[traveler->row][traveler->col + 1] | (new_color << 16);

		gridLock.unlock();
		break;
	default:
		break;
	}
}

// updates the traveler right and leave a color trail left
void colorTrailLeft(TravelerInfo *traveler) 
{
	int new_color;
	switch (traveler->type)
	{
	case RED_TRAV:
		while (!acquireRedInk(1)) usleep(1000);
		gridLock.lock();
		traveler->col++;

		new_color = (grid[traveler->row][traveler->col - 1] & 0xFF) + colorIncrement;
		if (new_color > 255) new_color = 255;

		grid[traveler->row][traveler->col - 1] = grid[traveler->row][traveler->col - 1] | new_color;

		gridLock.unlock();
		break;
	case GREEN_TRAV:
		while (!acquireGreenInk(1)) usleep(1000);
		gridLock.lock();
		traveler->col++;

		new_color = (grid[traveler->row][traveler->col - 1] >> 8 & 0xFF) + colorIncrement;
		if (new_color > 255) new_color = 255;

		grid[traveler->row][traveler->col - 1] = grid[traveler->row][traveler->col - 1] | (new_color << 8);

		gridLock.unlock();
		break;
	case BLUE_TRAV:
		while (!acquireBlueInk(1)) usleep(1000);
		gridLock.lock();
		traveler->col++;

		new_color = (grid[traveler->row][traveler->col - 1] >> 16 & 0xFF) + colorIncrement;
		if (new_color > 255) new_color = 255;
	
		grid[traveler->row][traveler->col - 1] = grid[traveler->row][traveler->col - 1] | (new_color << 16);

		gridLock.unlock();
		break;
	default:
		break;
	}
}

// updates the traveler down and leave a color trail up
void colorTrailUp(TravelerInfo *traveler) 
{
	int new_color;
	switch (traveler->type)
	{
	case RED_TRAV:
		while (!acquireRedInk(1)) usleep(1000);
		gridLock.lock();
		traveler->row++;

		new_color = (grid[traveler->row - 1][traveler->col] & 0xFF) + colorIncrement;
		if (new_color > 255) new_color = 255;
		
		grid[traveler->row - 1][traveler->col] = grid[traveler->row - 1][traveler->col] | new_color;

		gridLock.unlock();
		break;
	case GREEN_TRAV:
		while (!acquireGreenInk(1)) usleep(1000);
		gridLock.lock();
		traveler->row++;

		new_color = (grid[traveler->row - 1][traveler->col] >> 8 & 0xFF) + colorIncrement;
		if (new_color > 255) new_color = 255;

		grid[traveler->row - 1][traveler->col] = grid[traveler->row - 1][traveler->col] | (new_color << 8);

		gridLock.unlock();
		break;
	case BLUE_TRAV:
		while (!acquireBlueInk(1)) usleep(1000);
		gridLock.lock();
		traveler->row++;

		new_color = (grid[traveler->row - 1][traveler->col] >> 16 & 0xFF) + colorIncrement;
		if (new_color > 255) new_color = 255;
	
		grid[traveler->row - 1][traveler->col] = grid[traveler->row - 1][traveler->col] | (new_color << 16);

		gridLock.unlock();
		break;
	default:
		break;
	}
}

// updates the traveler up and leave a color trail down
void colorTrailDown(TravelerInfo *traveler) 
{
	int new_color;
	switch (traveler->type)
	{
	case RED_TRAV:
		while (!acquireRedInk(1)) usleep(1000);
		gridLock.lock();
		traveler->row--;

		new_color = (grid[traveler->row + 1][traveler->col] & 0xFF) + colorIncrement;
		if (new_color > 255) new_color = 255;
		
		grid[traveler->row + 1][traveler->col] = grid[traveler->row + 1][traveler->col] | new_color;

		gridLock.unlock();
		break;
	case GREEN_TRAV:
		while (!acquireGreenInk(1)) usleep(1000);
		gridLock.lock();
		traveler->row--;

		new_color = (grid[traveler->row + 1][traveler->col] >> 8 & 0xFF) + colorIncrement;
		if (new_color > 255) new_color = 255;

		grid[traveler->row + 1][traveler->col] = grid[traveler->row + 1][traveler->col] | (new_color << 8);

		gridLock.unlock();
		break;
	case BLUE_TRAV:
		while (!acquireBlueInk(1)) usleep(1000);
		gridLock.lock();
		traveler->row--;

		new_color = (grid[traveler->row + 1][traveler->col] >> 16 & 0xFF) + colorIncrement;
		if (new_color > 255) new_color = 255;
	
		grid[traveler->row + 1][traveler->col] = grid[traveler->row + 1][traveler->col] | (new_color << 16);

		gridLock.unlock();
		break;
	default:
		break;
	}
}
