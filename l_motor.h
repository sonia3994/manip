#ifndef __L_MOTOR_H
#define __L_MOTOR_H

#include "motor.h"

#define LOCKED_MODE 3

class LockedMotor:public Motor
{
private:

	Motor *master;

public:

	LockedMotor(char* name, AVR32 *avr, char *aChan, Motor* mot);
	void startLockedMode(int sourceChannel, long period);

	virtual void poll();		// our polling routine
	virtual void stop();		// override to handle cleanup if LOCKED_MODE
};

#endif	//ifndef __L_MOTOR_H
