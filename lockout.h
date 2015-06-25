/********************************************************************/
/* The lockout class is used to output signals to disable the       */
/* function of glove box valves.            												*/
/* 09/18/98 - PH																										*/
/********************************************************************/
#ifndef __LOCKOUT_H__
#define __LOCKOUT_H__

#include "llist.h"
#include "controll.h"

class Pulser;
class Axis;

#define LOCKOUT_COMMAND_NUMBER	0

class Lockout : public Controller {
public:
	Lockout(char *name, char *pulsername, int channel, float max_len);

	virtual void poll();            // poll the lockout class

	virtual void monitor(char *outString);

	virtual int  getNumCommands() 			{ return LOCKOUT_COMMAND_NUMBER; }
	virtual Command *getCommand(int num){ return 0; }

private:
	void				createAxisList();

	LList<Axis>	mAxisList;
	Pulser		* mPulser;
	int					mChannel;
	int					mOutput;
	float				mMaxLength;
};

#endif
