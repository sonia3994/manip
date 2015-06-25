#ifndef __LOADCELL_H
#define __LOADCELL_H

#include "ioutilit.h" 	// database input stuff

#include "hwio.h"
#include "datain.h"

#include "llsqr.h"
//#include "sigma.h"

#include "hardware.h"
#include "controll.h"

#include "display.h"
extern Display *display;	// defined in motorcon.cpp

#define LOADCELL_COMMAND_NUMBER	5

class Loadcell:public Controller
{
friend class Axis;

private:
	HardwareIO* hwInput;

	int channel;          // encoder board channel

	int	failed_last_poll;	// number of times tension outside of limits
	int	last_i;

	float	maxLoad;		// max load for this loadcell

	int minreading;
	int maxreading;

	int	calibrateFlag;		// 0 if not in calibration mode, non-zero otherwise
	linear_least_square fit; // object for doing calibration

	int	loopFlag;					// 0 if not in read loop, non-zero otherwise
	short lastReading;		// uncalibrated value from last reading of transducer

public:
	float slope,intercept;  	// calibration  force(N)=slope*reading+intercept

	void calibrate(void);  		// toggle calibration mode
	void calibrationPoint(char* string);	// add calibration point

	void setLoopFlag(void) {loopFlag = 1;}
	void clearLoopFlag(void) {loopFlag = 0;}

	void poll(void); // poll analog channel
    int avrLive(int showMsg)         {return hwInput->avrLive(showMsg);}

//  Use defaults in hardware.h


//	void commandOff(void)           // turn command an polling modes on
//	{                               // and off for object and its sub-objs.
//		commandable = 0;
//		analogChannel->commandOff();
//	}

//	void commandOn(void)
//	{
//		commandable = 1;
//		analogChannel->commandOn();
//	}

//	void pollOff(void)
//	{
//		pollable = 0;
//		analogChannel->pollOff();
//	}

//	void pollOn(void)
//	{
//		pollable = 1;
//		analogChannel->pollOn();
//	}

	void set_tension_limits(float mass);

	float getTension(void)	// return "tension" in N
	{
//		return 0.f; // hack to cut out bad hardware stuff
		return lastReading*slope+intercept;
	}

	Loadcell(char* name);	// database constructor
	~Loadcell(void) { }

	virtual void monitor(char *outString);

//	sigma averages;
//	float last_ten_sigmas[10];
//	float last_ten_averages[10];

	private:	// classes for accessing member functions via Command class

	class Read: public Command        // read "tension" in kg
	{public:
		Read(void):Command("read"){;};
		char* subCommand(Hardware* hw, char* inString)
		{
			sprintf(display->outString,"%s Tension: %f          ",hw->getObjectName(),((Loadcell*) hw)->getTension());
			display->message();
			return "HW_ERR_OK";
		}
	};
	class ReadLoop: public Command		// enter a continuous read loop
	{public:
		ReadLoop(void):Command("readloop"){;};
		char* subCommand(Hardware* hw, char* inString) {
			((Loadcell*) hw)->setLoopFlag();
			return "HW_ERR_OK";
		}
	};
	class StopLoop: public Command
	{public:
		StopLoop(void):Command("stoploop"){;};
		char* subCommand(Hardware* hw, char* inString) {
			((Loadcell*) hw)->clearLoopFlag();
			return "HW_ERR_OK";
		}
	};
	class Calibrate: public Command  // toggle calibration mode
	{public:
		Calibrate(void):Command("calibrate"){;};
		char* subCommand(Hardware* hw, char* inString)
		{
			((Loadcell*) hw)->calibrate();
			return "HW_ERR_OK";
		}
	};
	class CalibrationPoint: public Command  // add calibration point
	{public:
		CalibrationPoint(void):Command("point"){;};
		char* subCommand(Hardware* hw, char* inString) {
			((Loadcell*) hw)->calibrationPoint(inString);
			return "HW_ERR_OK";
		}
	};

protected:
	virtual Command *getCommand(int num)	{ return commandList[num]; }
	virtual int 		getNumCommands() 			{ return LOADCELL_COMMAND_NUMBER; }

private:
	static Command	*commandList[LOADCELL_COMMAND_NUMBER];	// list of commands

};//  strain gauge tension reading

#endif
