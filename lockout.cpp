/********************************************************************/
/* The lockout class is used to output signals to disable the       */
/* function of glove box valves.            												*/
/* 09/18/98 - PH																										*/
/********************************************************************/

#include "lockout.h"
#include "pulser.h"
#include "axis.h"
#include "manrope.h"
#include "display.h"

enum {
	OUTPUT_UNLOCKED = 0,
	OUTPUT_LOCKED		= 1,
	OUTPUT_UNKNOWN	= 2
};


Lockout::Lockout(char *name,char *pulsername,int channel,float max_len)
				:Controller("Lockout",name)
{
	mPulser = (Pulser *)Hardware::findObject(pulsername);
	if (!mPulser) {
		display->message("NO LOCKOUT OBJECT -- pulser not found!");
	}
	createAxisList();
	mChannel = channel;
	mOutput = OUTPUT_UNKNOWN;
	mMaxLength = max_len;
}

void Lockout::monitor(char *outString)
{
	if (!mPulser) {
		outString += sprintf(outString,"NO PULSER!!! - not active\n");
	}
	outString += sprintf(outString,"OUTPUT: %s\n",
											 mOutput==OUTPUT_LOCKED ? "LOCKED" : "FREE");
	outString += sprintf(outString,"MAX_LENGTH: %.1f\n",mMaxLength);
	for (LListIterator<Axis> li(mAxisList); li<LIST_END; ++li) {
		outString += sprintf(outString,"%s: ",li.obj()->getObjectName());
		float len = li.obj()->getLength();
		outString += sprintf(outString,"length=%.1f %s\n",
										len, len>mMaxLength ? "LOCKOUT" : "OK");
	}
}

void Lockout::createAxisList()
{
	mAxisList.clear();	// clear all old entries

	// create axis list
	for (LListIterator<Hardware> hw(Hardware::getHardwareList()); hw<LIST_END; ++hw) {
		if (!strcasecmp(hw.obj()->getClassName(),"AXIS")) {
			Axis *axis = (Axis *)hw.obj();
			// add only central ropes to the lockout axis list
			if (axis->getRope()->ropeType == ManipulatorRope::centralRope) {
				mAxisList += (Axis *)hw.obj();
			}
		}
	}
}

void Lockout::poll()
{
	if (!mPulser) return;

	if (!mAxisList.count()) {
		createAxisList();
		if (!mAxisList.count()) return;
	}

	int output = OUTPUT_UNLOCKED;

	for (LListIterator<Axis> li(mAxisList); li<LIST_END; ++li) {
		if (li.obj()->getLength() > mMaxLength) {
			output = OUTPUT_LOCKED;
			break;
		}
	}
	if (mOutput != output) {
		mOutput = output;
		mPulser->setDigitalOutput(mChannel, mOutput);
	}
}

