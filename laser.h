// LASER.H   Header file for the n2laser code
// Richard Ford  29th March 1997
//
// Revisions:    25/1/98 RF  Added commands to set filter wheels.
//
// Notes:
//
//
#ifndef __LASER_H
#define __LASER_H

#include "display.h"
#include "global.h"
#include "hwio.h"
#include "datain.h"
#include "loadcell.h"
//#include "IEEE_num.h"
#include "dye.h"
#include "filter.h"

#define LASER_COMMAND_NUMBER 10
#define HIGH 1
#define LOW 0

enum LaserStatus {		// monitor() routine must be updated if status is changed
	kUnknown, // Laser status is not known (indicates a problem)
	kOff,     // Laser is powered down
	kWarmup,	// Laser is warming up the thyratron
	kReady,   // Laser is ready but not triggered
	kWait,		// Laser is waiting before triggering (about 90 seconds)
	kRunning, // Laser is ready and firing
	kProblem  // Laser has a problem condition
};

class Laser:public Controller
{
private:
	LaserStatus	dbaseStatus;			// Status last time database updated
	LaserStatus laserStatus;      // Status of laser
	LaserStatus setLaserStatus(); // Get the laser status

	char substr1[30],substr2[30];
	char errorString[80];
	double lastTime;       // time of last trigger logic service
	double lastTime2;      // time of previous database update
	double waitTime;       // time left to wait before triggering laser

	float laserRate;       // Laser rep rate as entered in datafile
	int numFrequencies;    // number of dye cells + 1 (fundemental freq)
	float minGasFlow;      // minimum gas (agv) flow before laser shuts down
	float minGasPressure;  // Minimum gas (avg) pressure before laser shuts down
	float absMinGasFlow;      // minimum gas (abs) flow before laser shuts down
	float absMinGasPressure;  // Minimum gas (abs) pressure before laser shuts down
	float lastGasFlow;        // to calc average
	float lastGasPressure;    // to calc avg

	Dye *dye;
	Loadcell  *hiPressurecell,*lowPressurecell;
	Filter *filterA,*filterB;

	Pulser *pulser;

	HardwareIO* n2LaserPowerSwitch;
	HardwareIO* n2LaserTriggerSwitch;
//
	HardwareIO* n2LaserDebug;
//
	HardwareIO* n2LaserStatus;
	HardwareIO* n2LaserPowerStatus;
	HardwareIO* n2LaserLockStatus;
	HardwareIO* n2LaserTriggerStatus;
	HardwareIO* n2LaserWarmupStatus;
	HardwareIO* n2LaserReadyStatus;
	HardwareIO* n2LaserRunningStatus;
	HardwareIO* n2Laser_40V_Status;
	HardwareIO* n2Laser_12V_Status;
	HardwareIO* n2LaserDyeMotor1Status;
	HardwareIO* n2LaserDyeMotor2Status;
	HardwareIO* n2LaserDyeMotor3Status;
	HardwareIO* n2LaserDyeMotor4Status;

//
	unsigned int iN2LaserDebug;
//
	unsigned int iN2LaserStatus;
	unsigned int iN2LaserPowerStatus;
	unsigned int iN2LaserLockStatus;
	unsigned int iN2LaserTriggerStatus;
//	unsigned int iN2LaserWarmupStatus;
//	unsigned int iN2LaserReadyStatus;
//	unsigned int iN2LaserRunningStatus;
	unsigned int iN2Laser_40V_Status;
	unsigned int iN2Laser_12V_Status;
	unsigned int iN2LaserDyeMotor1Status;
	unsigned int iN2LaserDyeMotor2Status;
	unsigned int iN2LaserDyeMotor3Status;
	unsigned int iN2LaserDyeMotor4Status;

	unsigned int getMask(char *fileline);
	unsigned int controlMask,offState,warmupState,readyState,runningState;

	int triggerLaser(int trigger);  // toggle laser on/off
	int triggerRequest;             // set request to toggle laser on
	int triggerAttempt;             // num attempts to trigger laser
	int triggerSwitch;              // start/stop toggle switch

public:
	Laser(char* name);   // database constructor
	~Laser(void);

	virtual void monitor(char *outString);
	virtual void updateDatabase(void);
	virtual int  needUpdate() { return(getLaserStatus() != dbaseStatus);	}

	LaserStatus getLaserStatus()  {return laserStatus;}

	void poll(void); //read special control board and do maintenance on database

	void powerOn(void);
	void powerOff(void);
	void start(float pulserate);
	void stop(void);
	void hiPressure(void);
	void lowPressure(void);
	void gasFlow(void);
	void setND(float ndvalue);
	void rate(float pulserate);
	void block();
	int  isBlocked();

	float getLowPressure(void) {return lowPressurecell->getTension();}
	float getHiPressure(void) {return hiPressurecell->getTension();}

	float getFlow(void);

private:  // classes for accessing member functions via Command class

	class PowerOn: public Command
	{public:
		PowerOn(void):Command("poweron",kAccessCode1){;};
		char* subCommand(Hardware* hw, char* inString)
			{((Laser*) hw)->powerOn(); return "HW_ERR_OK";}
	};
	class PowerOff: public Command
	{public:
		PowerOff(void):Command("poweroff",kAccessCode1){;};
		char* subCommand(Hardware* hw, char* inString)
			{((Laser*) hw)->powerOff(); return "HW_ERR_OK";}
	};
	class Start: public Command
	{public:
		Start(void):Command("start",kAccessCode1){;};
		char* subCommand(Hardware* hw, char* inString)
		{
			float arg = atof(inString);
			((Laser*) hw)->start(arg);
			return "HW_ERR_OK";
		}
	};
	class Stop: public Command
	{public:
		Stop(void):Command("stop",kAccessAlways){;};
		char* subCommand(Hardware* hw, char* inString)
			{((Laser*) hw)->stop(); return "HW_ERR_OK";}
	};
	class HiPressure: public Command
	{public:
		HiPressure(void):Command("hipressure",kAccessCode1){;};
		char* subCommand(Hardware* hw, char* inString)
			{((Laser*) hw)->hiPressure(); return "HW_ERR_OK";}
	};
	class LowPressure: public Command
	{public:
		LowPressure(void):Command("lowpressure",kAccessCode1){;};
		char* subCommand(Hardware* hw, char* inString)
			{((Laser*) hw)->lowPressure(); return "HW_ERR_OK";}
	};
	class GasFlow: public Command
	{public:
		GasFlow(void):Command("gasflow",kAccessCode1){;};
		char* subCommand(Hardware* hw, char* inString)
			{((Laser*) hw)->gasFlow(); return "HW_ERR_OK";}
	};
	class SetND: public Command
	{public:
		SetND(void):Command("setnd",kAccessCode1){;};
		char* subCommand(Hardware* hw, char* inString)
		{
			float arg = atof(inString);
			((Laser*) hw)->setND(arg);
			return "HW_ERR_OK";
		}
	};
	class Rate: public Command
	{public:
		Rate(void):Command("rate",kAccessCode1){;};
		char* subCommand(Hardware* hw, char* inString)
		{
			float arg = atof(inString);
			((Laser*) hw)->rate(arg);
			return "HW_ERR_OK";
		}
	};
	class Block: public Command
	{public:
		Block(void):Command("block",kAccessCode1){;};
		char* subCommand(Hardware* hw, char* inString)
		{
			((Laser*) hw)->block();
			return "HW_ERR_OK";
		}
	};

	protected:
	virtual Command *getCommand(int num)	{ return commandList[num]; }
	virtual int 		getNumCommands() 			{ return LASER_COMMAND_NUMBER; }

private:
	static Command	*commandList[LASER_COMMAND_NUMBER];	// list of commands
};


#endif
