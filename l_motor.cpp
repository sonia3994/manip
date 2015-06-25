#include "l_motor.h"
#include "datain.h"
#include "display.h"

extern DataInput *dataInput;
extern Display *display;


LockedMotor::LockedMotor(char* name,       // motor name
						 AVR32 *avr,                     // timer card for source clock
						 char *aChan,  // tio10 channel number. Already changed
																		// from Alvin's 1-8 or 9-16 to the
																		// hardware's 1-10(skippping 4 and 5)
						 Motor *mot):						// pointer to Master motor
							Motor(name,avr,aChan)
{


	master=mot;
	if (master==NULL) {
		sprintf(display->outString,"WARNING: No master motor.  Bad Medicine.\n");
		display->message(1,21);
	}
}

void LockedMotor::startLockedMode(int sourceChannel, long period)
{
	controlMode=LOCKED_MODE;
	master->setCurrentHsteps(0);
	startHsteps = 0;	// save our current absolute location
	start();
}

void LockedMotor::stop()
{
	//TESTlong curSteps = currentHsteps;	// save current steps

	Motor::stop();		// let base class do the work

	if (controlMode == LOCKED_MODE) {
	//TEST	currentHsteps = curSteps;		// restore current steps (messed up by stop())
		poll();		// do one last poll to update totalHsteps
		controlMode = STEP_MODE;	// get out of LOCKED_MODE
	}
}

void LockedMotor::poll(void)
{
	if (controlMode == LOCKED_MODE) {
		// must calculate total steps taken based on master
//TEST		totalHsteps = currentHsteps + master->getCurrentHsteps(); //????  / (curPeriod*2);
	} else {
		Motor::poll();	// let the base class handle the regular polling
	}
}
