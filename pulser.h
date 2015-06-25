#ifndef __PULSER_H
#define __PULSER_H

#include "ioutilit.h" // database input stuff
#include "hardware.h"
#include "controll.h"
#include "avrchannel.h"
#include "global.h"

#define PULSER_COMMAND_NUMBER		3

class AVR32;

class Pulser : public Controller, public AVRChannel
{
private:
	int		source;		// source oscillator channel
	int		running;	// non-zero while running
	int		channel0Mask;
	int		channel1Mask;
	float	minRate;
	float	maxRate;
	float	curRate;
	float	clock_freq;

public:

	Pulser(char* name, AVR32 *avr,  char *aChan);

	virtual void poll() { }	// our polling routine
	virtual void stop();		// override to do simple stop
	virtual void run();		// override to do simple start
	virtual void setRate(float newrate);	// set pulse rate (Hz)
	virtual void monitor(char *outString);

	float getRate(void) {return curRate;}
	int   isRunning(void) {return running;}

	void	setDigitalOutput(int channel, int value);

protected:
	virtual Command *getCommand(int num)	{ return commandList[num]; }
	virtual int 		getNumCommands() 			{ return PULSER_COMMAND_NUMBER; }

private:
	static Command *commandList[PULSER_COMMAND_NUMBER];	// list of commands

	static class Stop: public Command {
		public:
			Stop(void):Command("stop") { }
			char* subCommand(Hardware *hw, char* inString) {
				((Pulser*) hw)->stop();
				return "HW_ERR_OK";
			}
	} cmd_stop;

	static class Run: public Command {
		public:
			Run(void):Command("run") { }
			char* subCommand(Hardware *hw, char* inString) {
				((Pulser*) hw)->run();
				return "HW_ERR_OK";
			}
	} cmd_run;

	static class Rate: public Command {
		public:
			Rate(void):Command("rate") { }
			char* subCommand(Hardware *hw, char* inString) {
				float arg = atof(inString);
				((Pulser*) hw)->setRate(arg);
				return "HW_ERR_OK";
			}
	} cmd_rate;

};

#endif	//ifndef __PULSER_H
