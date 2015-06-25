//
// File:			R_SOURCE.CPP
//
// Revisions:	02/20/97 PH - simplifed to utilize new motor flexiblity
//						02/21/97 PH - derived class from PolyAxis instead of Controller
//
#include "r_source.h"
#include "l_motor.h"
#include "hardware.h"
#include "controll.h"
#include "ioutilit.h"
#include "datain.h"
#include "display.h"

extern DataInput *dataInput;		// defined in motorcon.cpp
extern Display *display;

// static member declarations
R_Source::SetOrientation	R_Source::cmd_setorientation;
R_Source::AimAt						R_Source::cmd_aimat;
R_Source::Rotate					R_Source::cmd_rotate;

Command* R_Source::commandList[R_SOURCE_COMMAND_NUMBER] = {
	&cmd_setorientation,
	&cmd_aimat,
	&cmd_rotate,
};


R_Source::R_Source(char* objectName,// name of this object
									 AV *av,					// acrylic vessel object (for co-ordinates)
									 FILE *fp)				// optional file
					// keep database file open, and obtain file pointer in tempFile
				 :PolyAxis(objectName, av, fp, &tempFile)
{
	char Phi_name[513];
	char Theta_name[513];

// Get the names of the motors and the initial orientation from
// the database.

	// open input file and find entry
	InputFile in("POLYAXIS", objectName, 1.00, NL_FAIL, NULL, tempFile);

	// initialize variables from database (re-written PH 02/20/97)
	strcpy(Theta_name,noWS(in.nextLine("THETA_NAME:")));
	strcpy(Phi_name, noWS(in.nextLine("PHI_NAME:")));

// Now initialize the motors.

	theta = (Motor *)signOut(Theta_name);
	if (theta==NULL) quit("R_SOURCE Theta axis motor not found");

	phi = (LockedMotor*)signOut(Phi_name);
	if (phi==NULL) quit("R_SOURCE Phi axis motor not found");

	// get current position from motors
	current_theta = theta->getPosition();
	last_theta = current_theta;

	current_phi = phi->getPosition();
	last_phi = current_phi;

	stepsPerRevolution = (long)(theta->getStepsPerUnit() * 360.0 + 0.5);

	// We must close our database file manually if polyaxis opened it.
	// InputFile won't because it didn't open the file.
	// PolyAxis left it open specifically because we requested the file
	// pointer be returned.  I apologize for this sneaky code, but you
	// work with what you're given... - PH
	if (!fp) fclose(tempFile);

//	It may be necessary or desireable to inform the DAQ of the intial
//	source orientation at this point.  On the other hand, it may not be
//	practical.  Comment out the call to send_message if the DAQ should
//	not be informed of the initial orientation immediately.

//		send_message();

}


R_Source::~R_Source()
{
//	Nothing is dynamically allocated in the contructor except the commands.
//	They are destroyed elsewhere.  So not much needs to be done in the
//	destructor.

	stopRotation();

	signIn(theta);	// sign motors back in
	signIn(phi);
}

void R_Source::monitor(char *outStr)	// monitor function - PH 02/20/97
{
	PolyAxis::monitor(outStr);	// first show polyaxis information
	outStr += strlen(outStr);

	outStr += sprintf(outStr,"Source Orientation: %7.2f %7.2f\n",
										current_theta, current_phi);
}

Command *R_Source::getCommand(int num)	// add our command list to the base class
{
	int	numInherited = PolyAxis::getNumCommands();

	if (num >= numInherited) {
		return(commandList[num - numInherited]);
	} else {
		return(PolyAxis::getCommand(num));	// allow all inherited commands
	}
}
int R_Source::getNumCommands()	// we have an expanded command set
{
	return(R_SOURCE_COMMAND_NUMBER + PolyAxis::getNumCommands());
}

char* R_Source::getOrientation()
{
	sprintf(display->outString,"Source Orientation: %7.2f %7.2f",
					current_theta, current_phi);
	display->message();

	return "HW_ERR_OK";
}

int R_Source::checkPhi(float test_phi)	// return non-zero if phi out of range
{
	// let phi go one degree beyond [0,360] to allow overshoot
	if (test_phi<=-1 || test_phi>=361) {
		sprintf(display->outString,"R_SOURCE: Phi out of range");
		display->message();
		return(1);
	} else {
		return(0);
	}
}

char* R_Source::setOrientation(char* inString)
{
	float	temp_theta, temp_phi;

	if((sscanf(inString,"%f %f",&temp_theta,&temp_phi))==2) {
		// limit phi to the range [0,360]
		if (checkPhi(temp_phi)) return "HW_ERR_BAD_ARGS";

		// initialize motors and member variables
		theta->initPosition(temp_theta);
		phi->initPosition(temp_phi);
		// read back actual positions
		// (will not be exactly as we set due to step resolution)
		current_theta = theta->getPosition();
		current_phi = phi->getPosition();
		return getOrientation();
	}
	else {
		sprintf(display->outString,
			"USE: %s setOrientation <theta> <phi> (in degrees)",
			getObjectName());
		display->message();
		return "HW_ERR_BAD_ARGS";
	}
}


void R_Source::aimAt(float dest_theta, float dest_phi)
{
	// calculate the nearest theta angle and go directly there
	// (theta can spin round-and-round, but phi can't)
	float theta_diff = dest_theta - current_theta;
	theta_diff = fmod(theta_diff, 360);	// change to range [-360,360]
	// confine difference change to the range [-180,180]
	if (theta_diff < -180) theta_diff += 360;
	else if (theta_diff > 180) theta_diff -= 360;
	dest_theta = current_theta + theta_diff;	// our new destination

	// make sure phi is in the range [0,360]
	if (checkPhi(dest_phi)) return;

	theta->setPosition(dest_theta);
	phi->setPosition(dest_phi);

	commandOff(kAccessCode2);		// turn off access to code 2 commands
	pollOn();		// make sure we are polling
}

char* R_Source::aimAt(char* inString)
{
//	This member is an intermediary between the character based
//	COMMAND derrivative and the numerical member.

	float theta;
	float phi;

	if (sscanf(inString,"%f %f",&theta,&phi)==2) {
		this->aimAt(theta,phi);
		return "HW_ERR_OK";
	}
	else {
		sprintf(display->outString,"USE: %s aimAt <theta> <phi> (in degrees)",
			getObjectName());
		display->message();
		return "HW_ERR_BAD_ARGS";
	}
}

char* R_Source::rotate(char* inString)
{
	float rate;

	if (sscanf(inString,"%f",&rate)!=1) {
		rate = theta->getCruiseSpeed();
	}
	this->rotate(rate);
	return "HW_ERR_OK";
}

void R_Source::rotate(float speed)
{
	stopRotation();

	if (speed < theta->getMinSpeed()) {
		speed = theta->getMinSpeed();
		sprintf(display->outString,"Running at minimum speed of %g deg/s",speed);
	} else if (speed > theta->getCruiseSpeed()) {
		sprintf(display->outString,"Running at maximum speed of %g deg/s",theta->getCruiseSpeed());
	} else {
		sprintf(display->outString,"Running at %g deg/s",speed);
	}
	display->message();

	theta->setControlMode(VELOCITY_MODE);
	theta->setDesiredVelocity(speed);
	phi->setDesiredVelocity(speed);

	phi->startLockedMode(0,stepsPerRevolution);	// phi steps once every
																// revolution in theta
	theta->start();	// phi is locked, so it will start automagically

	commandOff(kAccessCode2);		// turn off access to code 2 commands
	pollOn();			// make sure we are polling
}

int R_Source::stop(int stopPattern)
{
	int rtnVal = PolyAxis::stop(stopPattern);		// stop the polyaxis first
	if (stopPattern) {
		stopRotation();								// stop our motors too if this is a global stop
	}
	return(rtnVal);
}

void R_Source::stopRotation()
{
	// stop our motors
	theta->stop();	// should stop the master first to avoid lost steps in the slave
	phi->stop();

	commandOn(kAccessCode2);				// restore access to code 2 commands
	if (!getNumAxes()) pollOff();		// stop polling if not necessary
}

void R_Source::rampDown()
{
	PolyAxis::rampDown();	// ramp down base class first

	if (theta->isRunning()) {
		theta->setControlMode(VELOCITY_MODE);
		theta->setDesiredVelocity(0);
	}
	if (phi->getControlMode() == LOCKED_MODE) {
		phi->stop();
	} else if (phi->isRunning()) {
		phi->setControlMode(VELOCITY_MODE);
		phi->setDesiredVelocity(0);
	}
}

void R_Source::poll()
{
	PolyAxis::poll();		// poll the base class first

//	This member needs to update the current orientation appropriately
//  and pass update information to the message builder that will ship
//  the orientation updates to the DAQ.

//	Update current_theta and current_phi

	// shut off everything and re-allow commands if motors not running
	if ((getCommandFlag() & kAccessCode2) &&
			!theta->isRunning() && !phi->isRunning()) {
		stopRotation();
	}

	// restrict theta to [0,360]
	theta->modulus(stepsPerRevolution);

	// get current angles from motors
	current_theta = theta->getPosition();
	current_phi = phi->getPosition();

	//  Send a message if orientation has changed.
	if (current_theta!=last_theta || current_phi!=last_phi) {
		if (phi->getControlMode()==LOCKED_MODE && checkPhi(current_phi)) {
			stopRotation();	// stop if we have run outside phi range
		}
		positionUpdate();
		last_theta = current_theta;
		last_phi = current_phi;
	}
}

void R_Source::positionUpdate()
{
//
//	This member needs to do whatever has to be done to communicate an
//	orientation update to the DAQ.  This probably has to be done through
//	an intermediary that builds a calibration data record of some sort.
//	This whole process is undefined at the moment, so this routine doesn't
//	do anything.

//	static int count=0;

	getOrientation();		// updates the screen display

}

