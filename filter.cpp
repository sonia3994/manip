// FILTER.CPP   Code to run the laser ND filter wheels.
// Richard Ford  29th March 1997
//
// Revisions:    25/1/98 RF  Added commands to set ND filter wheels.
//
//               10/2/98 RF  Changed position command to only move in cw direction.
//                           Added reSynchPos function.
// Notes:
//
//
#include "filter.h"

extern DataInput *dataInput;

const int		kOpenStopNum			= 7;		// stop number for zero attenuation
const float	kMotorCorrScale		= 0.4;  // relative speed to run motor for fine corrections
const float	kMinMotorScale		= 0.1;  // minimum motor speed scaling factor
const float kMaxMotorScale    = 2.0;  // maximun motor speed scaling factor
const float	kMaxEncoderError	= 5.0;	// maximum encoder error
const float kTabSlowDownDist  = 30.0; // slow down motor when within this dist of limit switch
const float updateDatabasePeriod = 10.0;
const float checkProblemPeriod= 0.5;  // check for big encoder error every 0.5s
const float maxRunningPeriod  = 20.0; // Motor is lost if it runs for longer than this time
const float kStopTolerance    = 3.0;  // Reported at a stop if within this tol
const float maxReSynchPos     = 5000.0; // Max angle before resynch of wheel position

Command* Filter::commandList[FILTER_COMMAND_NUMBER] = {
	new CWD,
	new CCWD,
	new CWS,
	new CCWS,
	new Position,
	new ToTab,
	new CalTab,
	new ND,
	new Stop,
	new CalPosition,
	new FindTab,
	new Init
};


Filter::Filter(char* filterName):  // constructor from database
		 Controller("Filter",filterName)
{
			const int kMaxLen = 128;
			char componentName[kMaxLen];  // name of a subcomponent of filterwheel
			char hwInStr[kMaxLen];
			char *namePtr;
			char* key;            		// pointer to character *AFTER* keyword

			// open file and find object
			InputFile in("FILTER", filterName, 1.00);

			key = in.nextLine("MOTOR:", "<motor name>");	 // get motor name
			strcpy(componentName,filterName);
			strcat(componentName,"Motor");
			namePtr = noWS(componentName);
			motor = (Motor*)signOut(namePtr);
			if (!motor) quit("motor","Filter");

			key = in.nextLine("ENCODER:","<encoder name>"); // get encoder name
			strcpy(componentName,filterName);
			strcat(componentName,"Encoder");
			namePtr = noWS(componentName);
			encoder = (Encoder *)signOut(new Encoder(namePtr));
			if (!encoder) quit("encoder","Filter");
			encoderStep = fabs(encoder->getStepSize());// get cm per step

			strcpy(hwInStr,filterName);
			strcat(hwInStr,"SWITCH");
			strcat(hwInStr,"DIGITALHARDWAREIO");
			filterTabSwitch = dataInput->getHardwareIO(hwInStr);
			if (!filterTabSwitch) quit("hwInput","filterTabSwitch");

			tabPosition = in.nextFloat("TAB_POSITION:", "<tab position>");
			presentPosition = in.nextFloat("PRESENT_POSITION:","<present position>");

			for(i=0;i<NUM_STOPS;i++)
			{
				 sprintf(hwInStr,"STOP%d:",i);
				 stopPosition[i] = in.nextFloat(hwInStr, "<stop position>");
				 sprintf(hwInStr,"ND%d:",i);
				 neutralDensity[i] = in.nextFloat(hwInStr, "<nd value>");
			}

			time = RTC->time();
			lastDatabaseTime = time;
			lastCheckProblemTime = time;

			desiredPosition = presentPosition;
			resetEncoderError();
			initAtTabFlag = 0;

			// change to quiet error return
			in.setErrorFlag(NL_QUIET);
} // end of Dye-from-database constructor

Filter::~Filter()
{
	signIn(motor); //sign motor back in
	delete encoder;
}

void Filter::monitor(char *outStr)
{
	int stopNum;
	static char *modeStr[] = { "IDLE","STEP_MODE","STOP_SELECT","TO_TAB_SWITCH","PROBLEM"};
	stopNum = getStopNumber();
	outStr += sprintf(outStr,"Status:  %s\n", modeStr[getFilterMode()]);
	outStr += sprintf(outStr,"Position: %d\n", stopNum);
	outStr += sprintf(outStr,"Degrees: %6.2f\n", getPosition());
	outStr += sprintf(outStr,"ND: %5.2f\n", getNDvalue(stopNum));
	outStr += sprintf(outStr,"Encoder error:   %6.2f\n", getEncoderError());
}

void Filter::poll(void)
{
	time = RTC->time();                // get real time

	presentPosition = getPosition();
	float	positionError = desiredPosition - presentPosition;
	unsigned int atTab = filterTabSwitch->read();

// make the axis idle if the motor has turned off for whatever reason
	if (filterMode!=kFilterProblem && filterMode != kFilterIdle && !motor->isOn())
		setFilterMode(kFilterIdle);

// If we are running to the tab switch, and we still didn't hit it,
// then add a bit more to the desired distance until we do.
// Also, slow down when we are close to where we expect limit to be.
	if(filterMode==kFilterTab)
	{
	// check if we hit the tab switch, stop at it (careful of overshoot!)
		if(atTab)
			 desiredPosition=getPosition();
		else
			 if(fabs(positionError)<kTabSlowDownDist)
			 {
				 scaleSpeed(kMotorCorrScale);
				 desiredPosition += sign(positionError)*kTabSlowDownDist;
			 }
	}

// init at Tab switch after motor has stopped
	if((filterMode==kFilterIdle)&&(initAtTabFlag))
	{
		 initPosition(0.0);
		 calTab();
		 initAtTabFlag=0;
	}

/*
** going to a cell position mode
*/
	if(!(filterMode==kFilterIdle) && !(filterMode==kFilterProblem))
	{

		if (motor->isRunning()) {
			motor->setDesiredPosition(-positionError+motor->getCurrentPosition());

// Check if the motor has been running for a while, if so it is likely
// just going round and round (pretty much lost). Signal a problem and stop

			if((time - startedRunningTime)>maxRunningPeriod)
			{
				stop();
				setFilterMode(kFilterProblem);
				sprintf(display->outString,"Problem with %s, motor running too long.",getObjectName());
				display->error();
			}
		} else if (fabs(positionError) > encoderStep) { // we moved off endpoint
			// set motor running at a slower speed to do small corrections
			scaleSpeed(kMotorCorrScale);
			// start up motor again
			moveMotor(positionError);
		} else {
			stop();	// stop the motor -- we got there
		}
	}

// update database once in a while
	if((time - lastDatabaseTime)>updateDatabasePeriod)
	{
		lastDatabaseTime = time;
		if (needUpdate()) updateDatabase(); // this writes new lengths and zeros to database
	}


// Check for problems (eg stalled motor) once in a while

	if((time - lastCheckProblemTime)>checkProblemPeriod)
	{
		lastCheckProblemTime = time;
		if(isEncoderError())
		{
			stop();
			setFilterMode(kFilterProblem);
			sprintf(display->outString,"Problem with %s, motor %.0f encoder: %.0f error: %.0f",
							getObjectName(),motor->getPosition(),encoder->length(),getEncoderError());
			display->error();
		}
	}
}

void Filter::moveMotor(float angle)
{
    if (fabs(angle) < remdeg(1)) {
        sprintf(display->outString,"Filter is already there");
        display->error();
    } else {
    	motor->changePosition(-angle);
	    startedRunningTime = RTC->time();
	}
}

void Filter::resetEncoderError() // force agreement between motor/encoder/loadcell
{
	oldPosition = encoder->length();
	motor->initPosition(-oldPosition);
	lastEncoderError = 0.0;
	if (filterMode == kFilterProblem) setFilterMode(kFilterIdle);
}


float Filter::getEncoderError()
{
	return(encoder->length() + motor->getPosition());
}

int Filter::isEncoderError()	// is the encoder error too large?
{
// lag up to the new error (smoothes out noise)
	lastEncoderError += 0.2*(getEncoderError()-lastEncoderError);
// is the encoder error is too large?
	return (lastEncoderError > kMaxEncoderError);
}

float Filter::getPosition(void)
{
	float curPosition = encoder->length();

	oldPosition = curPosition;
	return(curPosition);
}

void Filter::setMaxSpeed(char* commandString)
{
	motor->setCruiseSpeed(commandString);  // pass speed setting on to motor
}

void Filter::scaleSpeed(float scale)
{
	if (scale < kMinMotorScale) scale = kMinMotorScale;
	if (scale > kMaxMotorScale) scale = kMaxMotorScale;
	motor->setCruiseSpeed(scale * motor->getNormSpeed());
	motor->setAcceleration(scale * motor->getNormAccel());
}

void Filter::decommand(void)  // force Hardware subunits to refuse commands
{
	if (filterMode == kFilterProblem) commandOn();
	else commandOff();
	motor->commandOff();
	encoder->commandOff();
}

void Filter::recommand(void)	// enable Hardware subunit command processing
{
	commandOn();
	motor->commandOn();
	encoder->commandOn();
}

void Filter::initPosition(float position=0.0)
{
	encoder->setPosition(position);
	resetEncoderError();
	desiredPosition = presentPosition = position;
	initAtTabFlag = 0;
}

void Filter::reSynchPos(void)
{
/* This function just resets the wheel position to 0<x<360, just
	 in case rotations have been one way and angle is ridiculously high */
	float checkPosition = getPosition();
	if(fabs(checkPosition)>maxReSynchPos)
	{
		initPosition(remdeg(checkPosition));
	}
}

void Filter::findTab(void)
{
	initAtTabFlag = 1;
	toTab();
}

int Filter::getStopNumber(void)
{
// Check if the filterwheel is at any of the stop positions.
// We must compare at each stop position taking the remainder
// dividing by 360 deg since the wheel could be forward or
// backward some number of turns (use the remdeg function).
// First add a small offset to be sure difference is pos.
	int stopNum;
	float curPosition = getPosition()+kStopTolerance;
	for(i=0,stopNum=-1;i<NUM_STOPS;i++)
		 if(remdeg(curPosition-stopPosition[i])<(2.0*kStopTolerance)) stopNum=i;
	return(stopNum);
}

void Filter::setFilterMode(FilterMode mode)
{
	if (filterMode != mode) {
		filterMode = mode;
		if (filterMode != kFilterIdle) {
			decommand();              // lock out commands for sub-objects
		} else {
			recommand();
		}
	}
}

void Filter::cwd(float angle=0.0)
{
	setFilterMode(kFilterStep);	// set step mode
	startPosition = getPosition();
	desiredPosition = startPosition + angle;
	moveMotor(angle);
}

void Filter::ccwd(float angle=0.0)
{
	setFilterMode(kFilterStep);		// set step mode
	startPosition = getPosition();
	desiredPosition = startPosition - angle;
	moveMotor(-angle);
}

void Filter::cws(int stops=0)
{
//Find closest stop position, then add stops to it and move.
	int stopNum;
	float angle,diff;
	float checkPosition = getPosition()+180.0;
	reSynchPos();
	setFilterMode(kFilterSelect);
	for(i=0,stopNum=0,diff=fabs(remdeg(checkPosition-stopPosition[0])-180);i<NUM_STOPS;i++)
		 if(fabs(remdeg(checkPosition-stopPosition[i])-180)<diff)
		 {
				diff=fabs(remdeg(checkPosition-stopPosition[i])-180.0);
				stopNum=i;
		 }
	startPosition = getPosition();
	angle = remdeg(stopPosition[(stopNum+stops)%NUM_STOPS]-startPosition);
	if(angle>180.0) angle = angle-360.0;  //move shortest dist
	desiredPosition = startPosition+angle;
	moveMotor(angle);
}

void Filter::ccws(int stops=0)
{
	cws(-stops);
}

void Filter::position(int stopNum=0)
{
// Move to position, but avoid going through a filter
// position has a smaller value of ND, ie. you never want to
// go through filters that are brighter, so that SNO will not suddenly
// see a large burst of light. Here I must assume that the ND
// filters are ordered in either increasing or decreasing value,
// then avoid the path that would take you through position 0.
	float angle,angleCell0;
	reSynchPos();
	setFilterMode(kFilterSelect);
	startPosition = getPosition();
	angle = remdeg(stopPosition[stopNum%NUM_STOPS]-startPosition);
	if (stopNum==kOpenStopNum || getStopNumber()==kOpenStopNum) {
		// go the shortest way if either endpoint is the open position
		if (angle > 180) angle -= 360.0;
	} else {
		angleCell0 = remdeg(stopPosition[kOpenStopNum]-startPosition);
		if(angleCell0 < angle) angle -= 360.0;  //going through 0?
	}
	desiredPosition = startPosition+angle;
	moveMotor(angle);
}

void Filter::nd(float ndValue=0.0)
{
	int stopNum;
	float diff;

	setFilterMode(kFilterSelect);

	for(i=0,stopNum=0,diff=fabs(ndValue-neutralDensity[0]);i<NUM_STOPS;i++)
		 if(fabs(ndValue-neutralDensity[i])<diff)
		 {
				diff = fabs(ndValue-neutralDensity[i]);
				stopNum=i;
		 }
	position(stopNum);
}

void Filter::toTab(void)
{
/*
 Go to the tabstop, but set distance so that we only
 go move the wheel around once, regardless of how far the
 wheel has turned previously. Also only move to a tab stop
 in the forward (CW) direction so that positioning is reproducible.
 If we appear to be already at the tab stop then move anyway.
*/
	setFilterMode(kFilterTab);
	desiredPosition = remdeg(tabPosition);
	startPosition = getPosition();
	float delta = desiredPosition-startPosition;
	if (fabs(delta) < remdeg(1)) {
	    desiredPosition = startPosition + 360;
	} else {
	    desiredPosition += 360.0*(((int)(-delta/360.0))+0.5*(1.0-sign(delta)));
	}
	if(fabs(desiredPosition - startPosition)<kTabSlowDownDist)
		 desiredPosition += kTabSlowDownDist;
	moveMotor(desiredPosition - startPosition);
}

void Filter::calPosition(int stopNum=-1)
{
	char stopStr[128];
	setFilterMode(kFilterIdle);
	if((stopNum>=0)&&(stopNum<=NUM_STOPS))
	{
		stopPosition[stopNum] = getPosition();
// open output database file
		OutputFile out("FILTER",getObjectName(),1.00);

		for(i=0;i<NUM_STOPS;i++)
		{
			sprintf(stopStr,"STOP%d:",i);
			out.updateFloat(stopStr, stopPosition[i]);
		}
	}
	else
	{
		 sprintf(display->outString,"Problem with %s, Invalid stop position %d     ",
							getObjectName(),stopNum);
		 display->error();
	}
}

void Filter::calTab(void)
{
	tabPosition=getPosition();

	// open output database file
	OutputFile out("FILTER",getObjectName(),1.00);

	out.updateFloat("TAB_POSITION:", tabPosition);
}

void Filter::updateDatabase(void)
{
	// open output database file
	OutputFile out("FILTER",getObjectName(),1.00);

	// this is a little bit simpler now :)  - PH 01/31/97

	out.updateFloat("PRESENT_POSITION:", presentPosition);

	dbasePosition = presentPosition;
}
