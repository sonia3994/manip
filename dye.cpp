// Revisions:	March 97 -- First version -- Richard Ford
//

#include "dye.h"
#include "laser.h"

extern DataInput *dataInput;

const float	kMotorCorrScale		= 0.4;  // relative speed to run motor for fine corrections
const float	kMinMotorScale		= 0.1;  // minimum motor speed scaling factor
const float kMaxMotorScale    = 2.0;  // maximun motor speed scaling factor
const short	kMaxEncoderError	= 5;	  // maximum encoder error
const float kLimitStandoff    = 0.05; // dist in cm to move away from limit switch
const float kLimitSlowDownDist= 2.0; // slow down motor when within this dist of limit switch
const float kLimitDistIncr    = 0.5;  // Increments to dist when seeking limit switch
const float updateDatabasePeriod = 10.0;
const float checkProblemPeriod= 0.5;  // check for big encoder error every 0.5s
const float kCellTolerance    = 0.05; // reported at a cell if within this tol

Command* Dye::commandList[DYE_COMMAND_NUMBER] = {
	new Init,
	new Wavelength,
	new Cell,
	new Forward,
	new Backward,
	new Tostart,
	new Toend,
	new Toneutral,
	new Stop,
	new Calcell,
	new Findzero
};


Dye::Dye(char* dyeName):  // constructor from database
		 Controller("Dye",dyeName)
{
			const int kMaxLen = 128;
			char componentName[kMaxLen];  // name of a subcomponent of an dye
			char *namePtr;
			char hwInStr[kMaxLen];

			// open file and find object
			InputFile in("DYE", dyeName, 2.00);

			in.nextLine("MOTOR:", "<motor name>");	 // get motor name
			strcpy(componentName,dyeName);
			strcat(componentName,"Motor");
			namePtr = noWS(componentName);
			motor = (Motor*)signOut(namePtr);
			if (!motor) quit("motor","Dye");

			in.nextLine("ENCODER:","<encoder name>"); // get encoder name
			strcpy(componentName,dyeName);
			strcat(componentName,"Encoder");
			encoder = (Encoder *)signOut(new Encoder(componentName));
			if (!encoder) quit("encoder","Dye");
			encoderStep = fabs(encoder->getStepSize());// get cm per step

			strcpy(hwInStr,dyeName);
			strcat(hwInStr,"STARTLIMITSWITCH");
			strcat(hwInStr,"DIGITALHARDWAREIO");
			dyeLaserLimitSwitch1 = dataInput->getHardwareIO(hwInStr);
			if (!dyeLaserLimitSwitch1) quit("hwInput","dyeLaserStartLimitSwitch");

			strcpy(hwInStr,dyeName);
			strcat(hwInStr,"ENDLIMITSWITCH");
			strcat(hwInStr,"DIGITALHARDWAREIO");
			dyeLaserLimitSwitch2 = dataInput->getHardwareIO(hwInStr);
			if (!dyeLaserLimitSwitch2) quit("hwInput","dyeLaserEndLimitSwitch");

			limitStart = in.nextFloat("START_LIMIT:", "<start limit position>");
			limitEnd = in.nextFloat("END_LIMIT:", "<end limit position>");
			neutralPosition = in.nextFloat("NEUTRAL_POSITION:", "<neutral position>");
			presentPosition = in.nextFloat("PRESENT_POSITION:","<present position>");
			presentCell = in.nextFloat("PRESENT_CELL:","<last cell>");

			for(i=0;i<NUM_DYE_CELLS;i++)
			{
				 sprintf(hwInStr,"CELL_POSITION%d:",i);
				 cellPosition[i] = in.nextFloat(hwInStr, "<cell position>");
				 sprintf(hwInStr,"CELL_WAVELENGTH%d:",i);
				 cellWavelength[i] = in.nextFloat(hwInStr, "<cell wavelength>");
			}

//Check if positions are sensible
			if(    (limitEnd<limitStart)   ||
				(cellPosition[0]<limitStart)||(cellPosition[0]>limitEnd)||
				(cellPosition[1]<limitStart)||(cellPosition[1]>limitEnd)||
				(cellPosition[2]<limitStart)||(cellPosition[2]>limitEnd)||
				(cellPosition[3]<limitStart)||(cellPosition[3]>limitEnd)||
				(cellPosition[4]<limitStart)||(cellPosition[4]>limitEnd))
				quit("DYE: Inconsistent dye cell locations found");

			time = RTC->time();
			lastDatabaseTime = time;
			lastCheckProblemTime = time;

			desiredPosition = presentPosition;
			resetEncoderError();
			initAtLimitFlag = 0;

// change to quiet error return
			in.setErrorFlag(NL_QUIET);
} // end of Dye-from-database constructor

Dye::~Dye()
{
	signIn(motor);		// sign motor back in
	delete encoder;
}

void Dye::monitor(char *outStr)
{
	static char *modeStr[] = { "IDLE","STEP_MODE","DYE_SELECT","TO_LIMIT_SWITCH","PROBLEM"};

	int cellNum = getCellNumber();
	outStr += sprintf(outStr,"Status:  %s\n", modeStr[getDyeMode()]);
	outStr += sprintf(outStr,"Cell position:   %d\n", cellNum);
	outStr += sprintf(outStr,"Wavelength:   %f\n", getWavelength(cellNum));
	outStr += sprintf(outStr,"Mirror distance: %6.2f\n", getPosition());
	outStr += sprintf(outStr,"Encoder error:   %6.2f\n", getEncoderError());
	outStr += sprintf(outStr,"Dye wavelengths:");
	for (int i=0; i<NUM_DYE_CELLS; ++i) {
		outStr += sprintf(outStr," %.1f", getWavelength(i));
	}
	outStr += sprintf(outStr,"\n");
}

void Dye::poll(void)
{
	time = RTC->time();                // get real time

	presentPosition = getPosition();
	float	positionError = desiredPosition - presentPosition;
	unsigned int startLimit = dyeLaserLimitSwitch1->read();
	unsigned int endLimit   = dyeLaserLimitSwitch2->read();

// make the axis idle if the motor has turned off for whatever reason
	if (dyeMode!=kDyeProblem && dyeMode!=kDyeIdle && !motor->isOn())
		 setDyeMode(kDyeIdle);

// check if we hit a limit switch, move off it.
    if (dyeMode != kDyeProblem && // ph 09/03/98
        motor->isOn() &&
       ((startLimit && motor->getDirection() < 0) ||
        (endLimit   && motor->getDirection() > 0)))
    {
        stop(); // stop the motor
        const char *str1;
        const char *str2 = "";
        if (startLimit) {
            if (endLimit) {
                str1 = "both";
                str2 = "es";
        		setDyeMode(kDyeProblem);
            } else {
                str1 = "start";
                if (dyeMode == kDyeLimit) {
                    // step forward off microswitch if we are looking for the limit
                    forward(kLimitStandoff);
                } else {
                    setDyeMode(kDyeIdle);
                }
            }
        } else {
            str1 = "end";
            if (dyeMode == kDyeLimit) {
                // step backward off microswitch if we are looking for the limit
                backward(kLimitStandoff);
            } else {
                setDyeMode(kDyeIdle);
            }
        }
        sprintf(display->outString,"%s hit %s limit switch%s",
            motor->getObjectName(), str1, str2);
        display->error();
        writeCmdLog();
    }

// If we are running to a limit switch, and we still didn't hit it,
// then add a bit more to the desired distance until we do.
// Also, slow down when we are close to where we expect limit to be.
	if(dyeMode==kDyeLimit)
	{
		if(fabs(positionError)<kLimitSlowDownDist)
			scaleSpeed(kMotorCorrScale);
		if(fabs(positionError)<kLimitDistIncr)
			desiredPosition += sign(positionError)*kLimitDistIncr;
	}

// init at limit switch after motor has stopped
	if((dyeMode==kDyeIdle)&&(initAtLimitFlag))
	{
		 initPosition(0.0);
		 calzero();
		 initAtLimitFlag=0;
	}

// Constantly adjust where the motor is headed
	if(!(dyeMode==kDyeIdle) && !(dyeMode==kDyeProblem))
	{
		if (motor->isRunning()) {
			motor->setDesiredPosition(positionError+motor->getCurrentPosition());
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
			setDyeMode(kDyeProblem);
			sprintf(display->outString,"Problem with %s, motor %.0f, encoder %.0f, error %.0f",
							getObjectName(),motor->getPosition(),encoder->length(),getEncoderError());
			display->error();
		}
	}
}

// okToMoveMotor - can only change dyes if laser beam is blocked - PH 09/11/00
int Dye::okToMoveMotor()
{
	Laser *laser = (Laser *)Hardware::findObject("N2Laser");
	if (laser) {
		return(laser->isBlocked());
	}
    return(1);
}

void Dye::resetEncoderError() // force agreement between motor/encoder/loadcell
{
	oldPosition = encoder->length();
	motor->initPosition(oldPosition);
	lastEncoderError = 0;
	if (dyeMode == kDyeProblem) setDyeMode(kDyeIdle);
}


float Dye::getEncoderError()
{
	return(encoder->length() - motor->getPosition());
}

int Dye::isEncoderError()	// is the encoder error too large?
{
// lag up to the new error (smoothes out noise)
	lastEncoderError += 0.5*(getEncoderError()-lastEncoderError);
// is the encoder error is too large?
	return (lastEncoderError > kMaxEncoderError);
}

float Dye::getPosition(void)
{
	float curPosition = encoder->length();

	oldPosition = curPosition;
	return(curPosition);
}

void Dye::decommand(void)
{
	if (dyeMode == kDyeProblem) commandOn();
	else commandOff();
	motor->commandOff();
	encoder->commandOff();
}

void Dye::recommand(void)
{
	commandOn();
	motor->commandOn();
	encoder->commandOn();
}

void Dye::initPosition(float position=0.0)
{
	if(position==-1.0) position = getPosition();
	encoder->setPosition(position);
	resetEncoderError();
	desiredPosition = presentPosition = position;
	initAtLimitFlag = 0;
}

void Dye::forward(float distance)
{
	setDyeMode(kDyeStep);	// set step mode
//	decommand();              // lock out commands for sub-objects

	startPosition = getPosition();
	desiredPosition = startPosition + distance;
	moveMotor(distance);
}

void Dye::backward(float distance)
{
	setDyeMode(kDyeStep);		// set step mode
//	decommand();              // lock out commands for sub-objects
//	motor->setCurrentHsteps(0L);

	startPosition = getPosition();
	desiredPosition = startPosition - distance;
	moveMotor(-distance);
}


void Dye::setDyeMode(DyeMode mode)
{
	if (dyeMode != mode) {
		dyeMode = mode;
		if (dyeMode != kDyeIdle) {
			decommand();              // lock out commands for sub-objects
		} else {
			recommand();
		}
	}
	numReversals = 0;		// zero reversal counter
	reverseFlag = 0;		// initialize reverse flag
}

void Dye::wavelength(float lambda)
{
	int cellnum;
	float diff=0;

	for(int i=0; i<NUM_DYE_CELLS; ++i) {
		 if(i==0 || fabs(lambda-cellWavelength[i])<diff) {
				diff = fabs(lambda-cellWavelength[i]);
				cellnum = i;
		 }
	}
	cell(cellnum);
}

void Dye::cell(int cell)
{
	if((cell>=0)&&(cell<NUM_DYE_CELLS))
	{
		setDyeMode(kDyeSelect);
		presentCell = cell;	// save last cell position - PH 12/10/01
		desiredPosition = cellPosition[cell];
		startPosition = getPosition();
		moveMotor(desiredPosition - startPosition);
	} else {
		display->error("Invalid cell number");
	}
}

void Dye::tostart(void)
{
	setDyeMode(kDyeLimit);
	desiredPosition = limitStart;
	startPosition = getPosition();
	if(desiredPosition>=startPosition)
		 desiredPosition =startPosition - kLimitSlowDownDist;
	moveMotor(desiredPosition - startPosition);
}

void Dye::toend(void)
{
	setDyeMode(kDyeLimit);
	desiredPosition = limitEnd;
	startPosition = getPosition();
	if(desiredPosition<=startPosition)
		 desiredPosition =startPosition + kLimitSlowDownDist;
	moveMotor(desiredPosition - startPosition);
}

void Dye::toneutral(void)
{
	desiredPosition = neutralPosition;
	startPosition = getPosition();
	moveMotor(desiredPosition - startPosition);
}

void Dye::findzero(void)
{
	initAtLimitFlag = 1;
	tostart();
}

int Dye::getCellNumber(void)
{
	int cell;
	if (presentCell>=0 &&
		fabs(getPosition()-cellPosition[presentCell]) < kCellTolerance)
	{
		cell = presentCell;
	} else {
		cell = -1;
		for(i=0; i<NUM_DYE_CELLS; i++) {
			if(fabs(getPosition()-cellPosition[i])<kCellTolerance) {
				cell=i;
				break;
			}
		}
	}
	return(cell);
}

void Dye::calzero(void)
{
	limitStart=getPosition();

	// open output database file
	OutputFile out("DYE",getObjectName(),2.00);

	out.updateFloat("START_LIMIT:", limitStart);
}

void Dye::calcell(int cell)
{
	char stopStr[128];
	setDyeMode(kDyeIdle);
	if(!(cell<0)&&(cell<NUM_DYE_CELLS))
	{
		cellPosition[cell] = getPosition();
// open output database file
		OutputFile out("DYE",getObjectName(),2.00);

		for(i=0;i<NUM_DYE_CELLS;i++)
		{
			sprintf(stopStr,"CELL_POSITION%d:",i);
			out.updateFloat(stopStr, cellPosition[i]);
		}
	}
	else
	{
		 sprintf(display->outString,"Problem with %s, Invalid cell number %d     ",
							getObjectName(),cell);
		 display->messageB(1,kErrorLine);
	}
}

void Dye::moveMotor(float dist)
{
	if (okToMoveMotor()) {
		int	revFlag = (dist > 0) ? 1 : -1;

		if (reverseFlag != revFlag) {
			if (reverseFlag) ++numReversals;
			reverseFlag = revFlag;
		}
		motor->changePosition(dist);
	} else {
		stop();
		setDyeMode(kDyeIdle);	// set back to idle mode
		display->error("Laser beam must be blocked to change dye cell");
	}
}

void Dye::setMaxSpeed(char* commandString)
{
	motor->setCruiseSpeed(commandString);  // pass speed setting on to motor
}

void Dye::scaleSpeed(float scale)
{
	if (scale < kMinMotorScale) scale = kMinMotorScale;
	if (scale > kMaxMotorScale) scale = kMaxMotorScale;
	motor->setCruiseSpeed(scale * motor->getNormSpeed());
	motor->setAcceleration(scale * motor->getNormAccel());
}


void Dye::updateDatabase(void)
{
// open output database file
	OutputFile out("DYE",getObjectName(),2.00);

// this is a little bit simpler now :)  - PH 01/31/97

	out.updateFloat("PRESENT_POSITION:", presentPosition);
	out.updateFloat("PRESENT_CELL:", presentCell);

	dbasePosition = presentPosition;
}
