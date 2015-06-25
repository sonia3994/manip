//
// Revisions:		01/30/97 PH - MAJOR overhaul
//							05/24/97 PH - added umbilical stuff
//

//#define UMBIL_TENSION_MODE	// comment this out to run umbilical in proper length mode
//#define FAST_SEEK						// stop quickly near endpoint instead of trying to be exact
#define GUIDE_TUBE_MOTION  		// with this defined, drive using umbilical in single-axis
															// or inside the neck
//#define DEBUG								// uncomment this to print debugging information

#include "polyaxis.h"
#include "global.h"
#include "ieee_num.h"
#include "rotation.h"
#include "manpos.h"
#include "umbil.h"
#include "clientde.h"

//#define	LOW_TENSION 		2.5f		// minimum allowed tension in newtons
//#define	CONTROL_TENSION	5				// minimum control tension in newtons
//#define	MIN_TENSION			10.0f		// minimum desired tension in newtons
//#define MAX_TENSION			110.0f 	// maximum desired tension in newtons

#define END_ERROR				0.5f		// condition for endpoint stop
#define AXIS_ACCURACY		1				// accuracy factor for meeting desired rope lengths

#define	NET_FORCE_LIMIT					50.f	// net force single sample limit
#define NECK_HEIGHT(av)		((av)->getNeckRingPosition()[Z]-3.f)

const short	kMaxReversals			= 5;	// maximum number of motor reversals in direct goto
const float	kMinSegmentLength	= 15.;// minimum length of path segment (cm)
const short	kPatternWaitTime	= 120;// time in seconds to wait at a pattern point
const short kMaxPatternError	= 30;	// max. error before pattern shuts off (cm)
const short	kErrorFreq1				= 2000;// 1st tone frequency for error sound
const short	kErrorFreq2				= 1000;// 2nd tone frequency for error sound
const float	kSoundDuration		= 0.1;// duration of each speaker tone
const short	kSoundCount				= 8;	// number of speaker tones
const float	kUmbilChangeThresh= 1;	// change in umbilical length to force new path point
const int		kUmbilicalIndex		= 4;	// index of umbilical in flags word
#ifdef UMBIL_TENSION_MODE
const short	kUmbilicalTension	= 30;	// tension for umbilical
#else
const float	kUmbilLengthCorr	= 0.5;// weighting for converting umbilical length error to drive speed
const float	kUmbilTensionCorr	=	0.0;// weighting for converting from umbilical tension error to drive speed
#endif

const double kRadToDeg				= 180 / 3.1415926536;

enum EMotionFlag {
	kMotionDirect,				// moves directly to endpoint
	kMotionOrtho,					// moves parallel to x,y,z axes
	kMotionUmbilical,			// moves in constant umbil length curves
};


static char*	motionString[] = { "DIRECT", "ORTHO", "UMBIL" };

const int kNumMotionTypes = sizeof(motionString) / sizeof(char *);

static void sound(int freq) //PH
{
}
static void nosound() //PH
{
}

static char*	stopCause[] = {"NONE","LOW_TENSION","HIGH_TENSION",
														 "ENDPOINT","STUCK","NET_FORCE",
														 "AXIS_ERROR","CALC_ERROR","AXIS_FLAG",
														 "AXIS_ALARM" };

static char *sourceOrientationStr[kMaxSourceOrientation] =
							{ "UNKNOWN", "NORTH", "EAST", "SOUTH", "WEST" };

// static member declarations
FILE *PolyAxis::tempFile	= NULL;

PolyAxis::To					PolyAxis::cmd_to;
PolyAxis::By					PolyAxis::cmd_by;
PolyAxis::Stop				PolyAxis::cmd_stop;
PolyAxis::Halt				PolyAxis::cmd_halt;
PolyAxis::Locate			PolyAxis::cmd_locate;
PolyAxis::TensionFind	PolyAxis::cmd_tensionfind;
PolyAxis::Pattern			PolyAxis::cmd_pattern;
PolyAxis::Tensions		PolyAxis::cmd_tensions;
PolyAxis::Lengths			PolyAxis::cmd_lengths;
//PolyAxis::SetTensions	PolyAxis::cmd_settensions;
PolyAxis::NetForce		PolyAxis::cmd_netforce;
PolyAxis::Connect			PolyAxis::cmd_connect;
PolyAxis::Disconnect	PolyAxis::cmd_disconnect;
PolyAxis::Reset				PolyAxis::cmd_reset;
PolyAxis::Motion			PolyAxis::cmd_motion;
PolyAxis::Equalize		PolyAxis::cmd_equalize;
PolyAxis::SourceOffset PolyAxis::cmd_sourceoffset;
PolyAxis::Orientation PolyAxis::cmd_orientation;
PolyAxis::Alias       PolyAxis::cmd_alias;

Command* PolyAxis::commandList[POLYAXIS_COMMAND_NUMBER] = {
	&cmd_to,
	&cmd_by,
	&cmd_stop,
	&cmd_halt,
	&cmd_locate,
	&cmd_tensionfind,
	&cmd_pattern,
	&cmd_tensions,
	&cmd_lengths,
//	&cmd_settensions,
	&cmd_netforce,
	&cmd_connect,
	&cmd_disconnect,
	&cmd_reset,
	&cmd_motion,
	&cmd_equalize,
	&cmd_sourceoffset,
	&cmd_orientation,
	&cmd_alias
};

/************************************************************************/
/*                                                                      */
/*  PolyAxis database constructor.  Constructor reads Axis names from   */
/*  database file POLYAXIS.DAT lets Axis constructor take care of the   */
/*  rest, in an impressive display of object-oriented construction.     */
/*                                                                      */
/*                             -- written 95-11-03 tjr                  */
//
// Doesn't do that anymore - PH :P~
/*                                                                      */
/************************************************************************/
PolyAxis::PolyAxis(char* polyaxisName,    // constructor from database
		 AV *av,									 // acrylic vessel object (for co-ordinates)
		 FILE *fp,
		 FILE **fpOut):
		 Controller("PolyAxis",polyaxisName)
{
	int		i;
	const int kBSize = 128;
	char	connectStr[kBSize];
	char* key;            // pointer to character *AFTER* keyword

	for (i=0; i<kMaxAxisCommands; ++i) {
		axisCommandList[i] = NULL;	// set all axis commands to NULL
	}

	acrylicVessel = av;		// save AV object for co-ordinate transforms etc.
	umbilical			= NULL;
	waterLevelPt	= &acrylicVessel->getHeavyWaterLevel();
	waterDensity	= D2O_DENSITY;

	updateFlag 		= 0;		// don't need database update
	inVessel			= 1;

	patternFlag      = 0;	// not running a pattern yet
	patternFile			 = NULL;
	patternLogFile	 = NULL;

	sourceOrientation= 0;

	sourceMass			 = 0;
	soundTime				 = 0;
	soundCount			 = 0;
	equalizeFlag		 = 0;


	// initialize array of desired tensions
	for (i=0; i<kMaxAxes; ++i) desiredTensions[i] = 0;

	setPolyMode(kPolyIdle);

	manPos = new ManPos(0,this,*av);		// create dummy manPos object
	if (!manPos) quit("PolyAxis","manPos");

	pollOff();		// PolyAxis object are NOT pollable by default

	stopCauseFlag = NO_CAUSE;						// hasn't stopped yet, so no cause

	// open file and find object if necessary
	InputFile in("POLYAXIS", polyaxisName, 1.00, NL_FAIL, NULL, fp);

	in.setErrorFlag(NL_QUIET);
  key = in.nextLine("ALIAS:");
	if (key && (key=strtok(key,delim))!=NULL && key[0] && key[0] != '/') {
		// call inherited setAlias() because we don't want to update the .dat file
		Object::setAlias(key);
	}
	in.setErrorFlag(NL_FAIL);
	key = in.nextLine("AXES:");
	strncpy(connectStr, key, kBSize);

	// get source position
	sourcePosition[X] = in.nextFloat("POSITION:");
	sourcePosition[Y] = in.nextFloat();		// follows on same line (hence, no key)
	sourcePosition[Z] = in.nextFloat();

	motionFlag = kMotionOrtho;

	key = in.nextLine("MOTION:");
	for (i=0; i<kNumMotionTypes; ++i) {
		if (!strncmp(key,motionString[i],strlen(motionString[i]))) {
			motionFlag = i;
			break;
		}
	}

	key = in.nextLine("EQUALIZE_TENSIONS:");
	if (!strncmp(key,"ON",3)) equalizeFlag = 1;

	centralTension = in.nextFloat("CENTRAL_TENSION:");
	sourceOffset[X]= in.nextFloat("SOURCE_OFFSET:");
	sourceOffset[Y]= in.nextFloat();
	sourceOffset[Z]= in.nextFloat();
	in.setErrorFlag(NL_QUIET);
	sourceOrientation = (int)(in.nextFloat("ORIENTATION:", "0") + 0.5);
	if (sourceOrientation<0 || sourceOrientation>=kMaxSourceOrientation) {
		sourceOrientation = 0;
	}
	in.setErrorFlag(NL_FAIL);
	sourceMass     = in.nextFloat("SOURCE_MASS:");
	sourceVolume   = in.nextFloat("SOURCE_VOLUME:");
	sourceHeight   = in.nextFloat("SOURCE_HEIGHT:");
	sourceWidth    = in.nextFloat("SOURCE_WIDTH:");
	in.nextLine("SOURCE_SHAPE:");	// ignore source shape for now

	connect(connectStr, 1);		// connect to all our axes (initialize)

	if (getNumAxes() > kMaxAxes) quit("Too many axes");

	if (sourceHeight <= 0.0) quit("Invalid source height");

	if (fpOut) {
		in.leaveOpen();	// leave input file open if file is requested
		*fpOut = in.getFile();	// return file pointer
	}

	sourcePath = new SourcePath;

#ifdef FAST_SEEK
	static int	showedMsg=0;
	if (!showedMsg) {
		display->message("Compiled for fast seek mode");
		showedMsg = 1;
	}
#endif
}

PolyAxis::~PolyAxis(void)
{
	delete manPos;	// delete our manpos object

	newPath();
	delete sourcePath;
}

// return current rope tensions
float *PolyAxis::getTensions()
{
	LListIterator<Axis> li(axisList);
	static float	tensions[3];

	for (li=0; li<LIST_END; ++li) {
		tensions[li] = li.obj()->getTension();
	}
	return(tensions);
}
/*
void PolyAxis::setTensions()
{
	LListIterator<Axis> li(axisList);

	for (li=0; li<LIST_END; ++li) {
		li.obj()->setDesiredTension(li.obj()->getControlTension());
		li.obj()->setSticky(0);
		li.obj()->tensionActivate();  // start tension mode
	}
	if (umbilical) {
		umbilical->setDesiredTension(umbilical->getControlTension());
		umbilical->setSticky(0);
		umbilical->tensionActivate();  // start tension mode
	}
}
*/

void PolyAxis::monitor(char *outStr)
{
	LListIterator<Axis> li(axisList);

	if (isMoving()) {
		static char *modeStr[] = { "IDLE", "ABORT", "DIRECT", "UMBIL", "TENSION", "SINGLE_AXIS" };
		outStr += sprintf(outStr, "Status: MOVING_%s", modeStr[polyMode]);
	} else {
		outStr += sprintf(outStr, "Status: STOPPED_%s", getStopCause());
	}

	// display source position
	outStr += sprintf(outStr, "\nPosition:\t%.2f\t%.2f\t%.2f",
										sourcePosition[X], sourcePosition[Y], sourcePosition[Z]);

	outStr += sprintf(outStr, "\nSource Offset:\t%.2f\t%.2f\t%.2f",
										sourceOffset[X], sourceOffset[Y], sourceOffset[Z]);

	if (isMoving()) {
		outStr += sprintf(outStr,"\nDestination:\t%.2f\t%.2f\t%.2f",
										sourcePath->pos[X], sourcePath->pos[Y], sourcePath->pos[Z]);
	}
	// print out net force
	ThreeVector netForce = manPos->calcNetForce(getTensions());
	outStr += sprintf(outStr, "\nNet Force:\t%.2f\t%.2f\t%.2f",
										netForce[X], netForce[Y], netForce[Z]);

	// print out length error
	outStr += sprintf(outStr, "\nLength Error:\t%.2f", getPositionError());

	// print carriage tilt
	float	tilt = manPos->calcCarriageTilt();
	if (tilt) {
		outStr += sprintf(outStr, "\nCarriage Tilt:\t%.2f", tilt*kRadToDeg);
	}
	// print axis names
	outStr += sprintf(outStr, "\nAxes:\t");
	for (li=0; li<LIST_END; ++li) {
		outStr += sprintf(outStr, "\t%s", li.obj()->getObjectName());
	}
	if (umbilical)	outStr += sprintf(outStr, "\t%s", umbilical->getObjectName());

	// print rope lengths
	outStr += sprintf(outStr, "\nLengths:");
	for (li=0; li<LIST_END; ++li) {
		outStr += sprintf(outStr, "\t%.2f", li.obj()->getLength());
	}
	if (umbilical) outStr += sprintf(outStr, "\t%.2f", umbilical->getLength());

	if (isMoving()) {
		// print desired lengths
		outStr += sprintf(outStr, "\nTarget Lengths:");
		for (li=0; li<LIST_END; ++li) {
			outStr += sprintf(outStr, "\t%.2f", li.obj()->getDesiredPosition());
		}
		if (umbilical) outStr += sprintf(outStr,"\t%.2f", umbilical->getDesiredPosition());
	}
	// print rope tensions
	outStr += sprintf(outStr, "\nTensions:");
	for (li=0; li<LIST_END; ++li) {
		outStr += sprintf(outStr, "\t%.2f", li.obj()->getTension());
	}
	if (umbilical) outStr += sprintf(outStr, "\t%.2f", umbilical->getTension());

	// print axis errors
	outStr += sprintf(outStr, "\nEncoder Errors:");
	for (li=0; li<LIST_END; ++li) {
		outStr += sprintf(outStr, "\t%.2f", li.obj()->getEncoderError());
	}
	if (umbilical) outStr += sprintf(outStr, "\t%.2f", umbilical->getEncoderError());
	// print motor velocities
	outStr += sprintf(outStr, "\nVelocities:");
	for (li=0; li<LIST_END; ++li) {
		outStr += sprintf(outStr, "\t%.2f", li.obj()->getVelocity());
	}
	if (umbilical) outStr += sprintf(outStr, "\t%.2f", umbilical->getVelocity());

	float buoyancy = sourceVolume * waterDensity * GRAVITY;
	outStr += sprintf(outStr, "\nCentral tension: %.1f  (buoy = %.1f)",getCentralTension(sourcePosition),buoyancy);

	outStr += sprintf(outStr, "\nOrientation:\t%d (%s)",
										sourceOrientation, sourceOrientationStr[sourceOrientation]);

	outStr += sprintf(outStr,"\n");
}


float PolyAxis::getPositionError()
{
	return manPos->getPositionError();
}


// return source weight in newtons for a specified position
float PolyAxis::getSourceWeight(ThreeVector &pos)
{

	float	fractionSubmerged;
	float cmAboveWater;

	// eventually, this could be changed to calculate the actual
	// volume of the source that is submerged.  For now, just assume
	// a constant source cross-section
	cmAboveWater = pos[Z] - *waterLevelPt;
	if (cmAboveWater <= 0) {
		fractionSubmerged = 1.0;
	} else if (cmAboveWater >= sourceHeight) {
		fractionSubmerged = 0.0;
	} else {
		fractionSubmerged = 1.0 - cmAboveWater / sourceHeight;
	}

	return((sourceMass - sourceVolume * fractionSubmerged * waterDensity) * GRAVITY);
}

// get central tension for the current source location
float PolyAxis::getCentralTension(ThreeVector &pos)
{
	return(centralTension + getSourceWeight(pos) - sourceMass * GRAVITY);
}

// test if central tension is acceptable
// Inputs: central axis object
void PolyAxis::testCentralTension(Axis *centralAxis)
{
	// calculate minimum source weight in Newtons
	float buoyancy = sourceVolume * waterDensity * GRAVITY;

	if (centralTension - buoyancy < centralAxis->getMinControlTension()) {
		sprintf(display->outString,"Warning! %s central tension in water (%.1f N)\nis lower than minimum control tension (%.1f)!",
		getObjectName(),centralTension-buoyancy,centralAxis->getMinControlTension());
		display->message();
	}
}


void PolyAxis::reset()	// reset encoder errors for all axes
{
	for (LListIterator<Axis> li(axisList); li<LIST_END; ++li) {
		li.obj()->resetEncoderError();
	}
	if (umbilical) umbilical->resetEncoderError();
	manPos->reset();
}

int PolyAxis::calcSourcePosition()
{
	float	lengths[kMaxAxes];

	for (LListIterator<Axis> li(axisList); li<LIST_END; ++li) {
		lengths[li]  = li.obj()->getLength();
	}
	int rtnVal = manPos->calcState(lengths, getTensions());
	sourcePosition = manPos->getPosition();
	return(rtnVal);
}

void PolyAxis::errorSound()
{
	soundTime = RTC->time() + kSoundDuration;
	soundCount = kSoundCount;
	sound(kErrorFreq1);
}

void  PolyAxis::poll(void)  // poll Hardware -> this is where actual control is done
{
	Axis *axis;
	LListIterator<Axis>	li(axisList);

  if (!getNumAxes()) return;  // do nothing if no axes connected - PH 08/31/98
  
	double time = RTC->time(); // current time

	// play our error sounds
	if (soundCount && time>soundTime) {
		if (--soundCount) {
			sound(soundCount&0x01 ? kErrorFreq2 : kErrorFreq1);
			soundTime = time + kSoundDuration;
		} else {
			nosound();
		}
	}

	/* if stopped in pattern mode for more than pause time run next pattern point */
	if (!isMoving() && patternFlag && ((time-pauseTime)>patternWaitTime))
	{
		to(patternString);
		return;									// allow other objects to be polled first
	}
	int calcErr = calcSourcePosition();

	if (!isMoving()) {
		// stop on tension errors even if the polyaxis is inactive
		// if one or more of the axes is in tension mode
		for (li=0; li<LIST_END; ++li) {
// now stop on tension errors if any axis is moving - PH 06/06/00
			if (li.obj()->getAxisMode() != kAxisIdle) break;
//			if (li.obj()->getAxisMode() == kAxisTension) break;
		}
		if (li==LIST_END) return;  // do nothing if no axis moving
	} else if (polyMode == kPolyAbort) {
		// if we are ramping down all motors, check to see if they have stopped
		for (li=0; li<LIST_END; ++li) if (li.obj()->isRunning()) break;
		if (li == LIST_END) {
			stop();	// stop when all motors come to rest
			return;
		}
	} else if (calcErr) {
		// there is a calculation problem, stop the motors
		// because we are controlling the source with bad information
		stop();
		sprintf(display->outString,"Calculation error stop!");
		display->error();
		writeCmdLog();
		stopCauseFlag = CALC_ERROR_STOP;
		errorSound();
		nextPatternPoint();
		return;
	}

/*
** check axis error stop status's and stop if there is a problem
*/
	for (li=0; ; ++li)
	{
		if (li < LIST_END) {
			axis = li.obj();
		} else if (umbilical) {
			axis = umbilical;
		} else {
			break;
		}
		int stopCause = axis->getErrorStopStatus();
		if (stopCause) {
			stopCauseFlag = stopCause;
			if (polyMode == kPolyAbort) {
				stop();
			} else {
				errorSound();
				if (stop()) {
					sprintf(display->outString,"%s stopped due to %s error stop",
									getObjectName(),axis->getObjectName());
					display->error();
				}
				nextPatternPoint();
			}
			return;
		}
		if (li==LIST_END) break;
	}

	if (polyMode == kPolyAbort) return;
/*
** check for axis flags and alarms
*/
	for (li=0;; ++li) {
		if (li < LIST_END) {
			axis = li.obj();
		} else if (umbilical) {
			axis = umbilical;
		} else {
			break;
		}
		if (axis->getFlagError()) {
			// handle flag errors too - PH 08/06/99
			stop();
			sprintf(display->outString,"%s stopped due to Axis flag error",getObjectName());
			display->error();
			writeCmdLog();
			stopCauseFlag = AXIS_FLAG_STOP;
			errorSound();
			nextPatternPoint();
			return;
		} else if (axis->getAlarmStatus() > 1) {
			stop();
			sprintf(display->outString,"%s stopped due to Alarm",getObjectName());
			display->error();
			writeCmdLog();
			stopCauseFlag = AXIS_ALARM_STOP;
			errorSound();
			nextPatternPoint();
			return;
		}
		if (li==LIST_END) break;
	}

	// handle single axis mode - PH 08/22/00
	if (polyMode == kPolySingleAxis) {
		// check the tension of all axes
		int isRunning = 0;
		// check to see if any Axis is running
		for (li=0; ; ++li) {
			if (li < LIST_END) {
				if (li.obj()->getAxisMode() != kAxisIdle) {
					isRunning = 1;
					break;
				}
			} else if (umbilical) {
				if (umbilical->getAxisMode() != kAxisIdle) {
					isRunning = 1;
					break;
				}
			} else {
				break;
			}
			if (li == LIST_END) break;
		}
		if (isRunning) {
			// loop through connected Axes, looking for tension problems
			for (li=0; ; ++li) {
				int mask;
				if (li < LIST_END) {
					axis = li.obj();
					mask = 0x01 << (int)li;
				} else if (umbilical) {
					axis = umbilical;
					mask = 0x01 << kUmbilicalIndex;
				} else {
					break;
				}
				// check for axes going into low or high tension
				// (only if not in that state before)
				if (axis->isBadTension()) {
					if ((!(mask & lo_flags) && axis->isLowTension()) ||
							(!(mask & hi_flags) && axis->isHighTension()))
					{
						stop();		// stop all axes
						axis->checkErrors(1);	// print error message
						if (axis->isLowTension()) {
							stopCauseFlag = LOW_TENSION_STOP;
						} else {
							stopCauseFlag = HIGH_TENSION_STOP;
						}
						isRunning = 0;
						errorSound();
						break;		// no need to check other axes
					}
				}
				if (li == LIST_END) break;
			}
		} else {
			// go to idle mode because all axes have stopped
			setPolyMode(kPolyIdle);
		}
	}

	// return now if we aren't in polyaxis mode
	if (!isMoving() || polyMode==kPolySingleAxis) return;

	// handle tension equalization
	if (polyMode == kPolyTension) {
		// we are equalizing tensions
		int	onCount = 0;
		for (li=0; li<LIST_END; ++li) if (li.obj()->isOn()) ++onCount;
//		if (umbilical && umbilical->isMoving()) ++onCount;
		if (onCount) {
			// follow with the umbilical
			if (umbilical) {
				umbilical->setDesiredPosition(umbilical->calcLength(sourcePosition));
			}
		} else {
			// equalization is done
			if (sourcePath && sourcePath->next) {
				// return original motor speed scaling
				for (li=0; li<LIST_END; ++li) {
					li.obj()->scaleSpeed(1.0);
				}
				if (umbilical) {
					umbilical->scaleSpeed(1.0);
				}
				// we have another pathpoint to do
				SourcePath	*next = sourcePath->next;
				delete sourcePath;
				sourcePath = next;
				moveSource();
			} else {
				stop(); 										// all stop
				display->message("Endpoint stop");
				writeCmdLog("Endpoint stop");
				stopCauseFlag = ENDPOINT_STOP;
				nextPatternPoint();
			}
		}
		return;
	}

	int atEnd = 1;		// reset this flag below if we aren't at endpoint yet

/*
** Scale the velocities of all axes according to the calculated rope rates
*/
	float desiredRate[kMaxAxes];
	float actualRate[kMaxAxes];
	float maxSpeedFraction = 0;		// fraction of normal motor speed
	ThreeVector endDir;

	// calculate unit vector pointing towards turnpoint
	endDir = (sourcePath->pos - sourcePosition).normalize();

	// modify the direction in umbilical mode to follow the
	// path of a constant-length umbilical
	if (umbilical && polyMode==kPolyUmbil) {
		endDir = umbilical->calcConstLengthDir(sourcePosition, endDir);
	}

	// calculate rope speeds based on 1 cm/s velocity toward turnpoint
	manPos->calcRopeSpeeds(desiredRate, endDir, sourcePosition);

	for (li=0; li<LIST_END; ++li) {
		//
		// Note: By taking the absolute value here, I simplify
		// the calculations, but it does effect the control
		// algorithm in some situations:  When the initial rope
		// rope rate is opposite to the desired length change,
		// this will have the affect of driving the rope to the
		// final length earlier.  I don't think this will present
		// any problems, and this method does simplify the
		// calculations. - PH
		//

		// convert from absolute rate to rate as a fraction of normal speed
		desiredRate[li] = fabs(desiredRate[li]) / li.obj()->getNormSpeed();

		// find maximum speed fraction
		if (maxSpeedFraction < desiredRate[li]) {
				maxSpeedFraction = desiredRate[li];
		}

		// fill in actual rope velocities for later
		actualRate[li] = li.obj()->getVelocity();
	}

	if (umbilical && polyMode!=kPolyUmbil) {
		// limit maximum source velocity by umbilical max speed
		float desiredVel = umbilical->calcDriveSpeed(sourcePosition, endDir);
		float frac = fabs(desiredVel) / umbilical->getNormSpeed();
		if (maxSpeedFraction < frac) maxSpeedFraction = frac;
	}

	// calculate actual source velocity based on rope motor speeds
	ThreeVector sourceVel;

#ifdef GUIDE_TUBE_MOTION
	if (umbilical && (getNumAxes()==1 || sourcePath->inNeck)) {
		// the source is driven by the umbilical for guide tube motion
		sourceVel[0] = 0;
		sourceVel[1] = 0;
		sourceVel[2] = -umbilical->getVelocity();
	} else {
#endif
		// calculate the source velocity from the rope rates
		sourceVel = manPos->calcVelocity(actualRate, sourcePosition,
																							 sourcePath->inNeck);
#ifdef GUIDE_TUBE_MOTION
	}
#endif

	for (li=0; li<LIST_END; ++li) {

#ifdef GUIDE_TUBE_MOTION
		if (umbilical && (getNumAxes()==1 || sourcePath->inNeck)) {
			if (li.obj()->getRope()->ropeType == ManipulatorRope::centralRope) {
				// set desired velocity on centralrope for guidetube motion
				float ropeVelocity = -manPos->getRopeVector(li).dot(sourceVel);
				li.obj()->setDesiredVelocity(ropeVelocity);
				// set desired tension according to source weight - PH 05/26/99
				li.obj()->setDesiredTension(getCentralTension(sourcePosition));
			}
		} else
#endif
		 if (!sourcePath->inNeck) {
			// scale motor speeds in vessel only
			if (li.obj()->getAxisMode() == kAxisStep) {
				// scale step-mode axis speeds according to desired rope rate
				li.obj()->scaleSpeed(desiredRate[li]/maxSpeedFraction);
			} else {
				// set desired velocity to follow motion due to ropes in step mode
				float ropeVelocity = -manPos->getRopeVector(li).dot(sourceVel);
				li.obj()->setDesiredVelocity(ropeVelocity);
			}
#ifdef GUIDE_TUBE_MOTION
		}
#endif

		// test to see if we are at the turnpoint
		if (li.obj()->isRunning()) {
			// can't be done if we are still running
			if (li.obj()->getAxisMode() == kAxisStep) {
				if (li.obj()->getNumReversals() > kMaxReversals) {
					stop();
					// give up after we have reversed too many times
					sprintf(display->outString,"Stuck stop: %s",li.obj()->getObjectName());
					display->error();
					writeCmdLog();
					stopCauseFlag = STUCK_STOP;
					errorSound();
					nextPatternPoint();
					return;
				}
			}
			// not at turnpoint if any of the axes are still stepping
			atEnd = 0;

		} else {

			// not at end if we haven't stepped to our final position
			if (li.obj()->getAxisMode() == kAxisStep &&
				 !li.obj()->atDesiredPosition()) {
				atEnd = 0;
			}
		}
	}
#ifdef GUIDE_TUBE_MOTION
	if (umbilical && (getNumAxes()==1 || sourcePath->inNeck)) {
		int	umbilDone = 1;
		// calculate distance remaining
		float dz = sourcePath->pos[2] - sourcePosition[2];

		if (!umbilical->atDesiredPosition()) {
			if (dz < 0) {
				if (guideTubeDir < 0) {
					umbilDone = 0;
				}
			} else if (dz > 0) {
				if (guideTubeDir > 0) {
					umbilDone = 0;
				}
			}
		}
		if (!umbilDone) {
			// update desired umbilical length according to current position
			umbilical->setDesiredPosition(umbilical->getLength() - dz);
		}
		// update at end flag
		if (!umbilDone) atEnd = 0;
	}
#endif

#ifndef UMBIL_TENSION_MODE
	else if (umbilical && polyMode!=kPolyUmbil) {
		// modify umbilical velocity to try and maintain the proper length at all times
		float	lengthError = umbilical->getLength() - umbilical->calcLength(sourcePosition);
		float tensionError = umbilical->getTension() - umbilical->calcDriveTension(sourcePosition);
		float umbilVel = umbilical->calcDriveSpeed(sourcePosition, sourceVel);
		umbilical->setDesiredVelocity(umbilVel
																	- lengthError * kUmbilLengthCorr
																	+ tensionError * kUmbilTensionCorr);
		if (umbilical->isRunning()) atEnd = 0;
	}
#endif

	if (atEnd) {
		// we have reached our endpoint
		if (sourcePath->next) {
			// continue moving source to next point in path
			SourcePath	*next = sourcePath->next;
			// equalize tensions between vessel and neck - PH 05/29/99
			if (manPos->getNumAxes()>1 && (sourcePath->inNeck^next->inNeck)) {
				display->message("Setting rope tensions");
				// now set rope tensions
				for (li=0; li<LIST_END; ++li) {
					if (li.obj()->getRope()->ropeType == ManipulatorRope::centralRope) {
						li.obj()->scaleSpeed(1.0);	// return original motor speed scaling
						float theTension;
						if (next->inNeck) {
							theTension = getCentralTension(sourcePosition);
						} else {
							theTension = li.obj()->getControlTension();
						}
						li.obj()->setDesiredTension(theTension);
						li.obj()->tensionActivate();  // start tension mode
					}
					li.obj()->setSticky(0);	// turn off sticky mode
				}
				if (umbilical) {
					umbilical->scaleSpeed(1.0);
					// initialize umbilical length when entering vessel - PH 08/21/00
					if (sourcePath->inNeck) {
						umbilical->init(sourcePosition);
					}
					umbilical->to(umbilical->calcLength(sourcePosition));
				}
				setPolyMode(kPolyTension);
			} else {
				delete sourcePath;
				sourcePath = next;
				moveSource();
			}
		} else {
			if (equalizeFlag && manPos->getNumAxes()>1) {
				display->message("Endpoint");
				// now equalize the tensions
				for (li=0; li<LIST_END; ++li) {
					li.obj()->scaleSpeed(1.0);	// return original motor speed scaling
					li.obj()->setDesiredTension(manPos->getTension(li));
					if (li.obj()->getXbot()) {	// is it not the central rope?
						li.obj()->tensionActivate();  // start tension mode
					}
					li.obj()->setSticky(0);	// turn off sticky mode
				}
/* try it without this first...
			if (umbilical) {
				umbilical->scaleSpeed(1.0);
				umbilical->setSticky(0);
				umbilical->setDesiredTension(umbilical->calcDriveTension(sourcePosition));
			}
*/
				setPolyMode(kPolyTension);
			} else {	// not equalizing tensions
				stop(); 										// all stop
				display->message("Endpoint stop");
				writeCmdLog("Endpoint stop");
				stopCauseFlag = ENDPOINT_STOP;
				nextPatternPoint();
			}
		}
	}
}

void  PolyAxis::to(char* inString)  // move source to position x
{
	ThreeVector point(inString);        // generic source position point
	to(point);
}

void  PolyAxis::by(char* inString)  // move source to position x
{
	ThreeVector point(inString);        // generic source position point
	to(sourcePosition + point);
}

int PolyAxis::forbidden(ThreeVector &point)	// defines allowed points
{
	ThreeVector top;

	// can't go above central rope top bushing - PH 09/05/00
	ManipulatorRope *centralRope = getCentralRope();
	if (centralRope) {
		top = centralRope->getBushing();
		if (point[Z] > top[Z]) {
			return FORBIDDEN_HIGH;
		}
	}
	if (inVessel) {
		if (point[Z] > NECK_HEIGHT(acrylicVessel))
		{
			if (sqrt(point[X]*point[X]+point[Y]*point[Y]) >
						acrylicVessel->getNeckRingRadius() - sourceWidth/2)
			{
				return FORBIDDEN_NECK;		// outside neck
			}
		}
		else if (point.norm() > acrylicVessel->getVesselRadius())
		{
			return FORBIDDEN_VESSEL;		// outside vessel
		} else {
			// test two corners to make sure the source doesn't hang down and touch the vessel
			ThreeVector corner = point;
			corner[X] += sourceWidth/2;
			corner[Y] += sourceWidth/2;
			corner[Z] -= sourceHeight;
			if (corner.norm() > acrylicVessel->getVesselRadius())
			{
				return FORBIDDEN_VESSEL;
			}
			corner[X] -= sourceWidth;
			corner[Y] -= sourceWidth;
			if (corner.norm() > acrylicVessel->getVesselRadius())
			{
				return FORBIDDEN_VESSEL;
			}
		}
	} else {
		// source is in one of the guide tubes
		// check it doesn't hit AV or PSUP - PH 09/05/00
		if (centralRope) {
			float r = sqrt(top[X]*top[X]+top[Y]*top[Y]);
			float z = point[Z] - sourceHeight;
			float r_av = acrylicVessel->getVesselOutsideRadius();
			if (r < r_av) {
				// check for hitting AV
				r -= sourceWidth / 2;	// account for source width
				if (z<0 || (z*z+r*r<r_av*r_av)) {
					return FORBIDDEN_AV;
				}
			} else {
				// check for hitting PSUP
				r += sourceWidth / 2;	// account for source width
				float r_psup = acrylicVessel->getTubeRadius();
				if (z<0 && (z*z+r*r>r_psup*r_psup)) {
					return FORBIDDEN_PSUP;
				}
			}
		} else {
			return FORBIDDEN_ERROR;	// error: no central rope
		}
	}

	LListIterator<Axis> li(axisList);
	ManPos *tmpManPos = newManPos();
	if (!tmpManPos) quit("tmpManPos","PolyAxis::forbidden()");

	if (tmpManPos->calcState(point, desiredTensions)) {
		delete tmpManPos;
		return(FORBIDDEN_ERROR);
	}

	// set the desired lengths for all the ropes
	for (li=0; li<LIST_END; ++li) {
		float	tens = tmpManPos->getTension(li);
		if (tens > li.obj()->getMaxControlTension()) {
			delete tmpManPos;
			return FORBIDDEN_HIGH_T;
		}	else if (tens < li.obj()->getMinControlTension()) {
			delete tmpManPos;
			return FORBIDDEN_LOW_T;
		}
	}
	delete tmpManPos;
	return FORBIDDEN_OK;	// point is ok
}


void  PolyAxis::to(ThreeVector inDest)  // move source to position x
{
	LListIterator<Axis>	li(axisList);
	int   i;                            // loop counter
	SourcePath *path;
	ThreeVector	dest = inDest;

	if (sourcePosition == inDest) return;

	// don't let us move if any axis isn't being polled - PH 03/31/03
	int errcount = 0;
	for (li=0; ; ++li) {
		Axis *theAxis;
		if (li >= LIST_END) {
			if (!umbilical) break;
			theAxis = umbilical;
		} else {
			theAxis = li.obj();
		}
		if (!theAxis->isPollable()) {
			sprintf(display->outString,"Can't move %s -- not pollable!\n", theAxis->getObjectName());
			display->message();
			++errcount;
		}
		if (li >= LIST_END) break;
	}
	if (errcount) {
		display->message("Move aborted.");
		return;
	}

	manPos->constrainPosition(dest);	// contrain source motion

	i = forbidden(dest);		// test for forbidden regions
	if (i)
	{
		char *str;

		if 			(i == FORBIDDEN_NECK)		str = "is outside neck";
		else if (i == FORBIDDEN_VESSEL) str = "is outside vessel";
		else if (i == FORBIDDEN_LOW_T)	str = "has low tension";
		else if (i == FORBIDDEN_HIGH_T)	str = "has high tension";
		else if (i == FORBIDDEN_ERROR)	str = "is bad";
		else if (i == FORBIDDEN_AV)			str = "would hit vessel";
		else if (i == FORBIDDEN_PSUP)		str = "would hit PSUP";
		else if (i == FORBIDDEN_HIGH)		str = "is above top feedthrough";
		else														str = "has unknown error";

		sprintf(display->outString,"Point: %.3f %.3f %.3f %s",
						dest[X],dest[Y],dest[Z],str);
		display->message();

		nextPatternPoint();

		return;
	}

	if (isMoving()) stop();						  // stop all motors prior to changes

	stopCauseFlag = NO_CAUSE;						// hasn't stopped yet, so no cause

	if (sourcePosition == dest) return; // don't move

	float	nh = NECK_HEIGHT(acrylicVessel);
	// are our endpoints in the neck?
	int	srcInNeck = (sourcePosition[Z] > nh);
	int	dstInNeck = (dest[Z] > nh);

	newPath();

	sourcePath->pos = dest;
	sourcePath->mode = kPolyDirect;	// may change this later
	sourcePath->inNeck = dstInNeck;

	if (manPos->getNumAxes() > 1) {

		if (srcInNeck ^ dstInNeck) {
			// set axis in tension up mode if we are going up through the neck
			if (dstInNeck) {
				sourcePath->flags |= kTensionUp;
			}
			// insert new path point at start of list
			sourcePath = new SourcePath(sourcePath);
			if (!sourcePath) quit("sourcePath","Polyaxis::to");
			// make a turning point in center below the neck
			sourcePath->pos[Z] = nh - 10;
			sourcePath->mode = kPolyDirect;
			sourcePath->inNeck = srcInNeck;
		}

		// segment moves if necessary
		if (motionFlag!=kMotionDirect) {

			SourcePath **pathPt = &sourcePath;

			// handle moving the umbilical
			if (umbilical && motionFlag==kMotionUmbilical) {
				// step through the source path, adding necessary intermediate points
				for (;;) {
					path = *pathPt;
					ThreeVector	thePos = sourcePosition;
					// does the umbilical length change significantly?
					double	curLength = umbilical->calcLength(thePos);
					double	newLength = umbilical->calcLength(path->pos);

					if (fabs(newLength-curLength) > kUmbilChangeThresh) {
						// if umbilical is rubbing at this point, we must take it to
						// a position where it isn't rubbing before changing length
						if (umbilical->calcRubForce(thePos)) {
							// must insert new path point before "path"
							*pathPt = new SourcePath(path);
							if (!*pathPt) quit("sourcePath","Polyaxis::to");
							(*pathPt)->pos = umbilical->calcNeutralPos(curLength);
							(*pathPt)->mode = kPolyUmbil;
							(*pathPt)->inNeck = path->inNeck;
							pathPt = &(*pathPt)->next;	// move insertion pointer to new path point
						}
						// do the same before we move the destination point
						if (umbilical->calcRubForce(path->pos)) {
							// must insert new path point before "path"
							*pathPt = new SourcePath(path);
							if (!*pathPt) quit("sourcePath","Polyaxis::to");
							(*pathPt)->pos = umbilical->calcNeutralPos(newLength);
							(*pathPt)->mode = kPolyDirect;	// we can move directly here
							(*pathPt)->inNeck = path->inNeck;
							pathPt = &(*pathPt)->next;	// move insertion pointer to new path point
							path->mode = kPolyUmbil;	// change original path to umbilical mode
						}
					}
					if (!path->next) break;	// done if no more path points

					thePos = path->pos;	// use last path entry as new starting point
					pathPt = &path->next;
				}
			} else if (!(srcInNeck && dstInNeck)) { // no umbilical - segment if not in neck

				ThreeVector p1, p2;

				if (srcInNeck) {
					if (!sourcePath->next) quit("big error");
					pathPt = &sourcePath->next; // insert extra points before 2nd point in path
					p1 = sourcePath->pos;
				} else {
					p1 = sourcePosition;
				}
				if (!*pathPt) quit("huge error");
				path = *pathPt;
				p2 = path->pos;
				float d1 = sqrt(p1[X]*p1[X] + p1[Y]*p1[Y]);
				float d2 = sqrt(p2[X]*p2[X] + p2[Y]*p2[Y]);
				if (fabs(d1-d2)>kMinSegmentLength &&
						fabs(p1[Z]-p2[Z])>kMinSegmentLength)
				{
					if (d2 > d1) {	// we are moving away from the centerline
						// segment move only if necessary
						if ((p1[Z]*p2[Z] < 0) ||
								(p1[Z]>0 && p2[Z]<p1[Z]) ||
								(p1[Z]<0 && p2[Z]>p1[Z]))
						{
							// insert path point between these two
							*pathPt = new SourcePath(path);
							if (!*pathPt) quit("sourcePath","Polyaxis::to");
							(*pathPt)->pos = p1;
							(*pathPt)->pos[Z] = p2[Z];
							(*pathPt)->mode = kPolyDirect;
						}
					} else { // we are moving toward the centerline
						if ((p1[Z]*p2[Z] < 0) ||
								(p1[Z]>0 && p2[Z]>p1[Z]) ||
								(p1[Z]<0 && p2[Z]<p1[Z]))
						{
							// insert path point between these two
							*pathPt = new SourcePath(path);
							if (!*pathPt) quit("sourcePath","Polyaxis::to");
							(*pathPt)->pos = p2;
							(*pathPt)->pos[Z] = p1[Z];
							(*pathPt)->mode = kPolyDirect;
						}
					}
				}
			}
		}
	} else if (manPos->getNumAxes() < 1) {
		display->message("Can't move source with no Axes attached!");
		return;
	}

	// count the number of path points
	for (i=0,path=sourcePath; path; ++i,path=path->next);

	sprintf(display->outString,"Move will be accomplished in %d segment%s:",
					i, i==1 ? "" : "s");
	display->message();
	for (i=1,path=sourcePath; path; ++i,path=path->next) {
		sprintf(display->outString,"  %d) %s move to %6.1f %6.1f %6.1f",
						i, path->mode==kPolyDirect ? "Direct" : "Umbil.",
						path->pos[X], path->pos[Y], path->pos[Z]);
		display->message();
	}

	moveSource();			// move the source to the turn point
}


// reset source path
void PolyAxis::newPath()
{
	while (sourcePath->next) {
		// save pointer to next path
		SourcePath *next = sourcePath->next;
		// remove next path from list
		sourcePath->next = next->next;
		// delete path entry
		delete next;
	}
	sourcePath->mode = kPolyIdle;
	sourcePath->pos = oVector;
	sourcePath->flags = 0;		// PH 11/20/97
}


// move source to the next point in sourcePath
void PolyAxis::moveSource()
{
	LListIterator<Axis>	li(axisList);
	ManPos 				*tmpManPos;
	ThreeVector		dest = sourcePath->pos;

	// check for axis alarms
	for (li=0; li<LIST_END; ++li) {
		if (li.obj()->getAlarmStatus()) {
			if (li.obj()->getAlarmStatus() > 1) {
				sprintf(display->outString,"Cannot move. Axis %s has alarm.",
													li.obj()->getObjectName());
				display->message();
				return;
			} else {
				sprintf(display->outString,"Warning. Axis %s has alarm.",
													li.obj()->getObjectName());
				display->message();
			}
		}
	}
	if (umbilical) {
		if (umbilical->getAlarmStatus()) {
			if (umbilical->getAlarmStatus() > 1) {
				sprintf(display->outString,"Cannot move. Umbilical %s has alarm.",
													umbilical->getObjectName());
				display->message();
				return;
			} else {
				sprintf(display->outString,"Warning. Umbilical %s has alarm.",
													umbilical->getObjectName());
				display->message();
			}
		}
	}

	sprintf(display->outString,"%s point: %.3f %.3f %.3f",
					sourcePath->next ? "Turn" : "End", dest[X], dest[Y], dest[Z]);
	display->message();

	// calculate rope lengths at the destination.
	// this will assume the same central tension at the dest
	tmpManPos = newManPos();
	if (!tmpManPos) quit("tmpManPos","PolyAxis::moveSource()");

	if (tmpManPos->calcState(dest, desiredTensions)) {
		display->message("PolyAxis is not controllable. Check connections.");
		delete tmpManPos;
		return;
	}

	// set the desired lengths for all the ropes
	for (li=0; li<LIST_END; ++li) {
		li.obj()->setDesiredPosition(tmpManPos->getLength(li));
	}
	delete tmpManPos;
	if (umbilical) {
#ifdef GUIDE_TUBE_MOTION
		if (getNumAxes()==1 || sourcePath->inNeck) {
			// calculate distance remaining
			float dz = sourcePath->pos[2] - sourcePosition[2];
			// update desired umbilical length according to current position
			umbilical->setDesiredPosition(umbilical->getLength() - dz);
			if (dz < 0) guideTubeDir = -1;
			else				guideTubeDir = 1;
		} else {
#endif
			umbilical->setDesiredPosition(umbilical->calcLength(dest));
#ifdef GUIDE_TUBE_MOTION
		}
#endif
	}
/*
** start the axes on their way
*/
	if (getNumAxes() == 1) {

		li = 0;
#ifdef GUIDE_TUBE_MOTION
		if (umbilical) {
			sprintf(display->outString,"Guide tube motion");
			li.obj()->setDesiredTension(getCentralTension(sourcePosition));
			li.obj()->setSticky(1);
			li.obj()->tensionActivate();  // start tension mode
		} else {
#endif
			sprintf(display->outString,"Target length: %.2f",li.obj()->getDesiredPosition());
			li.obj()->to(li.obj()->getDesiredPosition());
#ifdef GUIDE_TUBE_MOTION
		}
#endif

	} else {	// must be 3 ropes

		char *str = display->outString;
		str += sprintf(str,"Target lengths: ");
		for (li=0; li<LIST_END; ++li) {
			// accumulate string with target rope lengths
			str += sprintf(str," %.2f",li.obj()->getDesiredPosition());

			int isTension;
			int ropeType = li.obj()->getRope()->ropeType;
			float theTension = li.obj()->getControlTension();

			if (sourcePath->inNeck) {
#ifdef GUIDE_TUBE_MOTION
				if (umbilical) {
					isTension = 1;
				} else {
					isTension = (ropeType != ManipulatorRope::centralRope);
				}
				// use special central rope tension for guide tube motion
				if (ropeType == ManipulatorRope::centralRope) {
					theTension = getCentralTension(sourcePosition);
				}
#else
				isTension = (ropeType != ManipulatorRope::centralRope);
#endif
			} else {
				isTension = (ropeType == ManipulatorRope::centralRope);
			}

			if (isTension) {
				// put rope in constant tension mode
				li.obj()->setSticky();		// tension ropes are always sticky
				li.obj()->setDesiredTension(theTension);
				li.obj()->tensionActivate(sourcePath->flags & kTensionUp);
			} else {
#ifdef FAST_SEEK
				li.obj()->setAccuracy(1.0);	// reduce accuracy to avoid hunting for endpoint
#else
				// set axis in sticky command mode so it will hold the position
				// if pulled away by the other motors (only if this is the final point)
				if (sourcePath->next) {
					// this is an intermediate point, make sure we don't spend
					// time hunting for the endpoint
					li.obj()->setSticky(0);
					li.obj()->setAccuracy(1.0);
				} else {
					// this is our final point, make it sticky with high accuracy
					li.obj()->setSticky();
					li.obj()->setAccuracy(li.obj()->getEncoderStep() * AXIS_ACCURACY);
				}
#endif
				// start axis moving toward destination length
				li.obj()->to(li.obj()->getDesiredPosition());
			}
		}
	}

	// only need to drive the umbilical if we aren't in umbilical mode and we aren't in the neck
	if (umbilical && (sourcePath->mode!=kPolyUmbil || sourcePath->inNeck)) {
#ifdef GUIDE_TUBE_MOTION
		if (umbilical && (getNumAxes()==1 || sourcePath->inNeck)) {
			umbilical->to(umbilical->getDesiredPosition());
			sprintf(display->outString,"Umbilical Target length: %.2f",umbilical->getDesiredPosition());
		} else {
#endif
			umbilical->setSticky();
#ifdef UMBIL_TENSION_MODE
			umbilical->setDesiredTension(kUmbilicalTension);
			umbilical->tensionActivate();
#else
			umbilical->velocityActivate();
#endif
#ifdef GUIDE_TUBE_MOTION
		}
#endif
	}

	display->message();

	setPolyMode(sourcePath->mode);
	commandOff();			// disable commands in polyaxis
}

char *PolyAxis::getStopCause()
{
	return(stopCause[stopCauseFlag]);
}

void PolyAxis::rampDown()
{
	setPolyMode(kPolyAbort);	// set abort mode
	patternStop();		// stop pattern if running
	for (LListIterator<Axis> li(axisList); li<LIST_END; ++li) {
		li.obj()->rampDown();	// abort movement on all axes
	}
	if (umbilical) umbilical->rampDown();
}

int PolyAxis::stop(int i)         // stop all motors
{
	int		wasRunning = 0;

	commandOn();		// re-enable commands through this object

	if (i) patternStop();						// turn off pattern mode
	for (LListIterator<Axis> li(axisList); li<LIST_END; ++li) {
		if (li.obj()->getAxisMode() != kAxisIdle) {
			wasRunning = 1;
		}
		li.obj()->stop();      // stop motor and  make it controllable
		li.obj()->setAxisMode(kAxisIdle);
		li.obj()->scaleSpeed(1.0);	// return original motor speed scaling
	}
	if (umbilical) {
		if (umbilical->getAxisMode() != kAxisIdle) {
			wasRunning = 1;
		}
		umbilical->stop();      // stop motor and  make it controllable
		umbilical->setAxisMode(kAxisIdle);
		umbilical->scaleSpeed(1.0);	// return original motor speed scaling
	}
	// write out stop position if we weren't stopped before
	if (wasRunning || (polyMode!=kPolyIdle && polyMode!=kPolyAbort)) {
		sprintf(display->outString,"%s stopped at %.2f %.2f %.2f", getObjectName(),
						sourcePosition[X], sourcePosition[Y], sourcePosition[Z]);
		writeCmdLog();
	}

	setPolyMode(kPolyIdle);

	return(wasRunning);
}

// Modified to leave axes unchanged - PH 02/03/97
void PolyAxis::tensions(char* inString)	// display calculated tensions
{
	LListIterator<Axis> li(axisList);
	ManPos	*tmpManPos;
	tmpManPos = newManPos();
	if (!tmpManPos) return;

	// calculate tensions at new position if necessary
	// (this will assume the same central tension)
	// use desired tensions for any position other than the current location
	ThreeVector	pos;

	if (strlen(inString) >= 5) {
		pos = inString;
		// use desired tensions here if position is given - PH 10/31/97
		tmpManPos->calcState(pos, desiredTensions);
	} else {
		pos = sourcePosition;
		tmpManPos->calcState(pos, getTensions());
	}

	char *pt = display->outString;
	pt += sprintf(pt,"Expected tensions:");
	for (li=0; li<LIST_END; ++li) {
		pt += sprintf(pt, " %7.2f", tmpManPos->getTension(li));
	}
	display->message();

	pt = display->outString;
	pt += sprintf(pt,"Tension errors:   ");
	for (li=0; li<LIST_END; ++li) {
		pt += sprintf(pt, " %+7.2f", li.obj()->getTension() - tmpManPos->getTension(li));
	}
	display->message();

	// print carriage tilt
	float	tilt = tmpManPos->calcCarriageTilt();
	sprintf(display->outString, "Carriage Tilt:   %7.2f", tilt*kRadToDeg);
	display->message();

	if (umbilical) {
		double	tx = umbilical->calcHorizTension(pos);
		double	ty = umbilical->calcVertTension(pos);
		sprintf(display->outString,"Expected umbilical source tension: %7.2f (horiz=%.2f,vert=%.2f)",
						sqrt(tx*tx+ty*ty),tx,ty);
		display->message();
		sprintf(display->outString,"Expected umbilical drive tension:  %7.2f",
						umbilical->calcDriveTension(pos));
		display->message();
		sprintf(display->outString,"Actual umbilical drive tension:    %7.2f",
						umbilical->getTension());
		display->message();
		sprintf(display->outString,"Umbilical drive tension error:     %7.2f",
						umbilical->getTension() - umbilical->calcDriveTension(pos));
		display->message();
	}

	delete tmpManPos;
}

void PolyAxis::lengths(char* inString)	// display rope lengths at given point
{
	ThreeVector	xs;
	ManPos	*tmpManPos;
	tmpManPos = newManPos();
	if (!tmpManPos) return;

	ThreeVector pos;
	if (strlen(inString) >= 5) pos = inString;
	else pos = sourcePosition;

	tmpManPos->calcState(pos, desiredTensions);

	sprintf(display->outString,"Expected lengths: %.2f %.2f %.2f",
		tmpManPos->getLength(0),
		tmpManPos->getLength(1),
		tmpManPos->getLength(2));
	display->message();

	if (umbilical) {
		sprintf(display->outString,"Umbilical length: %.2f",umbilical->calcLength(pos));
		display->message();
	}

	delete tmpManPos;
}

float PolyAxis::getNetForce()
{
	LListIterator<Axis> li(axisList);
	return manPos->calcNetForce(getTensions()).norm();
}

void PolyAxis::netforce(char* inString)	// display net force
{
	ManPos	*tmpManPos;
	tmpManPos = newManPos();
	if (!tmpManPos) return;

	float	*tensions = getTensions();

	if (strlen(inString) >= 5) tmpManPos->calcState(inString, tensions);
	else tmpManPos->calcState(sourcePosition, tensions);

	ThreeVector net = tmpManPos->calcNetForce(tensions);

	sprintf(display->outString,"Net force: %.2f %.2f %.2f [ %.2f ]",
		net[X], net[Y], net[Z], net.norm());
	display->message();
}

/* create manPos object and handle a change in our axis list */
void PolyAxis::initManPos()
{
	delete manPos;		// get rid of old manPos object

	manPos = newManPos();

	if (!manPos) quit("manPos","PolyAxis::initManPos()");

	// fill in desired tensions for this configuration
	for (LListIterator<Axis> li(axisList); li<LIST_END; ++li) {
		desiredTensions[li] = li.obj()->getControlTension();
	}
	if (axisList.count() < 3) {
		if (sourceOrientation) {
			sourceOrientation = 0;
			display->message("Source orientation reset");
		}
	}
}


// take ownership of an axis object by name
// - sets display outstring and returns NULL on error
Axis *PolyAxis::claimAxisObject(char *name)
{
	Hardware *obj = findObject(name);	// look for object in list
	if (!obj) {
		sprintf(display->outString, "%s is not a valid object", name);
	} else if (strcmp(obj->getClassName(), "Axis")) {
		sprintf(display->outString, "%s is not an Axis object", name);
		obj = 0;
	} else if (obj->getOwner()) {
		sprintf(display->outString,"%s is connected to %s",
						name, obj->getOwner()->getObjectName());
		obj = 0;
	} else {
		signOut(obj);							// claim ownership of the new Axis object
	}
	return((Axis *)obj);	// return the object
}

void PolyAxis::motion(char *inString)
{
	changeFlag(inString,"Motion", &motionFlag, kUpdateMotion, motionString, kNumMotionTypes);
}

void PolyAxis::equalize(char *inString)
{
	changeFlag(inString,"Tension equalization", &equalizeFlag, kUpdateEqualize);
}

void PolyAxis::changeFlag(char *inString, char *msg, int *flagPt, int updateFlag,
													char *strings[], int numStrings)
{
	int		i;
	char	*msgPt = 0;
	static char	*defaultStrings[] = { "OFF", "ON" };

	if (!strings) {
		strings = defaultStrings;
		numStrings = 2;
	}

	char	*pt = strtok(inString, delim);

	if (pt) {
		for (i=0; i<numStrings; ++i) {
			if (!strcasecmp(pt,strings[i])) {
				if (*flagPt != i) {
					sprintf(display->outString,"%s was %s.",msg,strings[*flagPt]);
					display->message();
					*flagPt = i;
					updateDatabase(updateFlag);		// update the database
					sprintf(display->outString,"%s set to %s",msg,strings[i]);
				} else {
					sprintf(display->outString,"%s is already %s",msg,strings[i]);
				}
				break;
			}
		}
		if (i == numStrings) {
			strcpy(display->outString,"Please specify ");
			msgPt = " only";
		}
	} else {
		sprintf(display->outString,"%s is currently %s.",msg,strings[*flagPt]);
		display->message();
		strcpy(display->outString,"Specify ");
		msgPt = " to change";
	}
	if (msgPt) {
		for (i=0; ;) {
			strcat(display->outString,strings[i]);
			++i;
			if (i >= numStrings) {
				strcat(display->outString,msgPt);
				break;
			} else if (i == numStrings-1) {
				strcat(display->outString," or ");
			} else {
				strcat(display->outString, ", ");
			}
		}
	}
	display->message();
}

// set our source offset
void PolyAxis::setSourceOffset(char *inString)
{
	if (!inString[0]) {
		display->message("No new offset specified");
		sprintf(display->outString,"Source offset is: %.3f %.3f %.3f",
						sourceOffset[X],sourceOffset[Y],sourceOffset[Z]);
		display->message();
	} else {
		ThreeVector newOffset(inString);        // generic source position point
		sprintf(display->outString,"Source offset was: %.3f %.3f %.3f",
						sourceOffset[X],sourceOffset[Y],sourceOffset[Z]);
		display->message();
		sourceOffset = newOffset;
		sprintf(display->outString,"Offset now set to: %.3f %.3f %.3f",
						sourceOffset[X],sourceOffset[Y],sourceOffset[Z]);
		display->message();
		updateDatabase(kUpdateSourceOffset);
	}
}

// set our source orientation
void PolyAxis::setOrientation(char *inString)
{
	int changed = 0;
	char *setStr = "is";
	if (inString[0]) {
		int val = atoi(inString);
		if (val>=0 && val<kMaxSourceOrientation) {
			sourceOrientation = val;
			changed = 1;
			setStr = "set to";
		} else {
			display->message("Invalid orientation specified");
		}
	}
	sprintf(display->outString,"Source orientation %s %d (%s)", setStr,
					sourceOrientation, sourceOrientationStr[sourceOrientation]);
	display->message();
	if (changed) {
		updateDatabase(kUpdateOrientation);
	} else {
		display->message("");
		display->message("Valid orientations are:");
		for (int i=0; i<kMaxSourceOrientation; ++i) {
			sprintf(display->outString,"  %d (%s)",
							i, sourceOrientationStr[i]);
			display->message();
		}
	}
}

// connect axes to polyaxis
// if showMsg is zero, then we don't update the dat file
// our output a message to the screen
void PolyAxis::connect(char *inString, int init)
{
	char	*pt = strtok(inString, delim);

	if (pt) {
		// build our axis commands
		display->outString[0] = 0;	// set this to zero so we can check later
		// loop until we run out of tokens, or hit a comment marker
		while (pt && *pt!='/') {
			Axis *theAxis = claimAxisObject(pt);
			if (theAxis) {
				if (theAxis->getAxisType() == kUmbilicalAxisType) {
					if (umbilical) {
						sprintf(display->outString, "Umbilical already connected");
					} else {
						umbilical = (Umbilical *)theAxis;
						for (LListIterator<Axis> li(axisList); li<LIST_END; ++li) {
							if (li.obj()->getRope()->ropeType == ManipulatorRope::centralRope) {
									testCentralTension(theAxis);
									break;
							}				
						}
					}
				} else if (getNumAxes() >= kMaxAxes) {
					sprintf(display->outString, "Maximum %d axes connected at once", kMaxAxes);
				} else {
					// we have added the axis to our list.  Set our water parameters depending

					// on whether we are in the heavy or light water

					if (theAxis->isInHeavyWater()) {
						inVessel = 1;
						waterLevelPt = &acrylicVessel->getHeavyWaterLevel();
						waterDensity = D2O_DENSITY;
						// check for consistency
						for (LListIterator<Axis> li(axisList); li<LIST_END; ++li) {
							if (!li.obj()->isInHeavyWater()) {
								sprintf(display->outString,"Warning: Axis %s is outside vessel while %s is inside!",
												li.obj()->getObjectName(),theAxis->getObjectName());
							}
						}
					} else {
						inVessel = 0;
						waterLevelPt = &acrylicVessel->getLightWaterLevel();
						waterDensity = H2O_DENSITY;
						// check for consistency
						for (LListIterator<Axis> li(axisList); li<LIST_END; ++li) {
							if (li.obj()->isInHeavyWater()) {
								sprintf(display->outString,"Warning: Axis %s is inside vessel while %s is outside!",
												li.obj()->getObjectName(),theAxis->getObjectName());
							}
						}
					}

					if (theAxis->getRope()->ropeType == ManipulatorRope::centralRope && umbilical) {
						testCentralTension(theAxis);
					}

					/* add this axis to our list */
					axisList += theAxis;

				}
			}
			pt = strtok(NULL, delim);	// get next token
		}
		if (init) {
			// we are initializing from file
			if (display->outString[0]) {
				quit(display->outString);	// quit if we had any errors
			}
		} else {
			// display message if we have one
			if (display->outString[0]) display->message();
			updateDatabase(kUpdateAxes);		// update the axis list in database
		}
		if (getNumAxes() > 0) pollOn();	// start polling if we have a axis
		initManPos();		// re-create manPos object

		if (!init) {
			connections();	// show current connections if not initializing
// bad idea
//			locate(sourcePosition);	// re-locate source to set axis lengths
		}

		// update axis command list since our axes have changed
		updateAxisCommands();

	} else {
		// give a list of available axes
		showObjects("Axis", NULL, kMatchType | kNotOwned);
		connections();	// show current connections if not initializing
	}
}

void PolyAxis::updateAxisCommands()
{
	int i;

	// delete existing commands
	for (i=0; i<kMaxAxisCommands; ++i) {
		if (axisCommandList[i]) {
			delete axisCommandList[i];
			axisCommandList[i] = NULL;
		}
	}
	// create new axis commands
	i = 0;
	for (LListIterator<Axis> li(axisList); li<LIST_END; ++li) {
		axisCommandList[i] = new AxisCommand(li.obj()->getObjectName());
		if (!axisCommandList[i]) {
			display->error("Error creating Axis command -- out of memory!");
		}
		++i;
	}
	if (umbilical) {
		axisCommandList[i] = new AxisCommand(umbilical->getObjectName());
		if (!axisCommandList[i]) {
			display->error("Error creating Axis command -- out of memory!");
		}
	}
}

int PolyAxis::axisMotionOK(Axis *anAxis, float delta)
{
	LListIterator<Axis> li(axisList);

	int index;
	int axisIndex = 0;							// index of axis to move
	int centralIndex = 0;
	Axis *axis;

	lo_flags = 0;
	hi_flags = 0;

	if ((polyMode != kPolyIdle) &&
			(polyMode != kPolySingleAxis))
	{
		return(0);	// can't move single axis when already running
	}

	for (li=0; ; ++li)
	{
		if (li < LIST_END) {
			axis = li.obj();
			index = (int)li;
			if (axis->getRope()->ropeType == ManipulatorRope::centralRope) {
				centralIndex = index;
			}
		} else if (umbilical) {
			axis = umbilical;
			index = kUmbilicalIndex;
		} else {
			break;
		}
		// save index of axis we are moving
		if (axis == anAxis) {
			axisIndex = index;
		}
		// reset error stop status on idle Axes so we
		// don't stop unnecessarily on old errors
		if (axis->getAxisMode() == kAxisIdle) {
			axis->resetErrorStopStatus();
		}
		// set high/low tension flags
		float tension = axis->getTension();
		if (tension < axis->getMinTension()) {
			lo_flags |= (0x01 << index);
		}
		if (tension > axis->getMaxTension()) {
			hi_flags |= (0x01 << index);
		}
		if (li==LIST_END) break;
	}
	if (!(lo_flags | hi_flags)) {
		stopCauseFlag = NO_CAUSE;						// hasn't stopped yet, so no cause
		setPolyMode(kPolySingleAxis);	// set single axis mode
		return(1);	// all tensions OK
	}
	if (lo_flags & (0x01 << axisIndex)) {
		sprintf(display->outString,"%s has low tension",anAxis->getObjectName());
		display->error();
		writeCmdLog();
		return(0);	//don't allow a low tension axis to be moved
	}
	// get motion vector for the axis being moved
	ThreeVector axisMotion;
	if (axisIndex == kUmbilicalIndex) {
		// assume umbilical rope vector in the same
		// direction as the central rope
		axisMotion = manPos->getRopeVector(centralIndex) * delta;
	} else {
		axisMotion = manPos->getRopeVector(axisIndex) * delta;
	}
	// loop again to determine effect of rope motion on low/high
	// tension ropes
	for (li=0; ; ++li)
	{
		if (li < LIST_END) {
			axis = li.obj();
		} else if (umbilical) {
			axis = umbilical;
		} else {
			break;
		}
		ThreeVector ropeVector;
		index = (int)li;
		if (index == LIST_END) {
			index = kUmbilicalIndex;
			// assume umbilical rope vector in the same
			// direction as the central rope
			ropeVector = manPos->getRopeVector(centralIndex);
		} else {
			ropeVector = manPos->getRopeVector(index);
		}
		if ((lo_flags | hi_flags) & (0x01 << index)) {
			float dot = axisMotion * ropeVector;
			// if dot product is positive, this rope with lengthen
			if (dot < 0) {
				// rope will get longer -- this is bad if in low tension
				if (lo_flags & (0x01 << index)) {
					sprintf(display->outString,"Warning: %s has low tension",axis->getObjectName());
					display->error();
					writeCmdLog();
					display->message("This could make it worse");
#ifdef DEBUG
					sprintf(display->outString,"rope vector: %.2f %.2f %.2f",ropeVector[0],ropeVector[1],ropeVector[2]);
					display->message();
					sprintf(display->outString,"axis motion: %.2f %.2f %.2f",axisMotion[0],axisMotion[1],axisMotion[2]);
					display->message();
					sprintf(display->outString,"dot product is %.2f",dot);
					display->message();
#endif
					return(0);
				}
			} else {
				// rope will get shorter -- this is bad if in high tension
				if (hi_flags & (0x01 << index)) {
					sprintf(display->outString,"Warning: %s has high tension",axis->getObjectName());
					display->error();
					writeCmdLog();
					display->message("This could make it worse");
#ifdef DEBUG
					sprintf(display->outString,"rope vector: %.2f %.2f %.2f",ropeVector[0],ropeVector[1],ropeVector[2]);
					display->message();
					sprintf(display->outString,"axis motion: %.2f %.2f %.2f",axisMotion[0],axisMotion[1],axisMotion[2]);
					display->message();
					sprintf(display->outString,"dot product is %.2f",dot);
					display->message();
#endif
					return(0);
				}
			}
		}
		if (li==LIST_END) break;
	}
	stopCauseFlag = NO_CAUSE;						// hasn't stopped yet, so no cause
	setPolyMode(kPolySingleAxis);	// set single axis mode
	return(1);	// OK to move the rope
}

// axisCommandSafe - decide if this axis command is safe
// Returns 1 if safe, 0 if not.
// (only checks commands which are flagged as expert)
int PolyAxis::axisCommandSafe(Axis *axis, char *cmd, char *arg)
{
	int isSafe = 0;
	float val = atof(arg);
	if (!strcasecmp(cmd,"up")) {
		if (axisMotionOK(axis,-val)) {
			isSafe = 1;
		}
	} else if (!strcasecmp(cmd,"down")) {
		if (axisMotionOK(axis,val)) {
			isSafe = 1;
		}
	} else if (!strcasecmp(cmd,"to")) {
		if (axisMotionOK(axis,val-axis->getLength())) {
			isSafe = 1;
		}
	} else if (!strcasecmp(cmd,"addslack")) {
		if (axis == umbilical) {
			val += umbilical->calcLength(sourcePosition);
			if (axisMotionOK(axis, val-axis->getLength())) {
				isSafe = 1;
			}
		}
	} else if (!strcasecmp(cmd,"reset")) {
		isSafe = 1;
	} else if (!strcasecmp(cmd,"init")) {
		isSafe = 1;
	} else if (!strcasecmp(cmd,"maxspeed")) {
		isSafe = 1;
	} else if (!strcasecmp(cmd,"slack")) {
		isSafe = 1;
	}
	return(isSafe);	// return non-zero if command is safe
}

char *PolyAxis::axisCommand(char *name, char *str)
{
	LListIterator<Axis> li(axisList);

	for (li=0; ; ++li) {
		Axis *axis;
		if (li < LIST_END) {
			axis = li.obj();
		} else if (umbilical) {
			axis = umbilical;	// execute umbilical commands too
		} else {
			break;
		}
		// look for axis or umbilical with specified name
		if (!strcasecmp(name,axis->getObjectName())) {
			char *cmd, *arg;
			if (str) {
				cmd = strtok(str, Hardware::delim);
				arg = strtok(NULL, "");	// get the rest of the string
				if (!cmd) cmd = "";
				if (!arg) arg = "";
			} else {
				cmd = arg = "";
			}
			extern ClientDev *getCurrentClient();
			ClientDev *client = getCurrentClient();
			int wasExpert = client->isExpert(0);
//
// Decide whether this command is harmless.
// If so, temporarily make the client an expert.
//
			if (axisCommandSafe(axis, cmd, arg)) {
				client->setExpert(3);	// set to expert mode 3 (doesn't check timeout)
			}
			// execute the command
			char *rtnStr = axis->command(cmd,arg);
			// return to original expert state
			client->setExpert(wasExpert);	// restore original expert mode
			return(rtnStr);
		}
		if (li==LIST_END) break;
	}
	return "HW_ERR_NO_COMMAND";     // return no command run error
}


// disconnect axes from polyaxis
void PolyAxis::disconnect(char *inString)
{
	char	*pt = strtok(inString, delim);
	LListIterator<Axis> li(axisList);

	if (pt) {
		do {
			if (umbilical && !strcasecmp(umbilical->getObjectName(), pt)) {
				signIn(umbilical);
				umbilical = NULL;
			} else {
				for (li=0; li<LIST_END; ++li) {
					if (!strcasecmp(li.obj()->getObjectName(), pt)) {
						signIn(li.obj());		// sign it back in (must be done BEFORE we remove it from the list)
						axisList -= li.obj();	// remove from our list
						break;
					}
				}
				if (li==LIST_END) {
					sprintf(display->outString, "%s was not connected", pt);
					display->message();
				}
			}
			pt = strtok(NULL, delim);	// get next token
		} while (pt);
	} else {
		// no arguments; remove ALL axes
		for (li=0; li<LIST_END; ++li) {
			signIn(li.obj());			// sign them all back in
		}
		axisList.clear();				// reset the axis list

		if (umbilical) {				// drop the umbilical too
			signIn(umbilical);
			umbilical = NULL;
		}
	}
	initManPos();		// re-create manPos object

	if (getNumAxes()) {
		calcSourcePosition();// re-calculate source position
	} else {
		pollOff();					// stop polling if we have no axes
	}

	// must update axis command list - PH 09/09/00
	updateAxisCommands();

	updateDatabase(kUpdateAxes);	// update the axis list in database
	connections();				// display current connections
}

// display axes connected to this polyaxis
void PolyAxis::connections()
{
	char	*pt = display->outString;

	pt += sprintf(pt,"%s connected to",getObjectName());
	if (getNumAxes() || umbilical) {
		for (LListIterator<Axis> li(axisList); li<LIST_END; ++li) {
			pt += sprintf(pt," %s", li.obj()->getObjectName());
		}
		if (umbilical)	pt += sprintf(pt, " %s", umbilical->getObjectName());
	} else {
		pt += sprintf(pt," <nothing>");
	}
	display->message();
}

void PolyAxis::depoll(void)      // turn off Hardware polling
{
	for (LListIterator<Axis> li(axisList); li<LIST_END; ++li) li.obj()->pollOff();
}

void PolyAxis::repoll(void)      // turn on Hardware polling
{
	for (LListIterator<Axis> li(axisList); li<LIST_END; ++li) li.obj()->pollOn();
}


ManPos *PolyAxis::newManPos()
{
	return(createManPos(*acrylicVessel, axisList, this, umbilical));
}

ManipulatorRope *PolyAxis::getCentralRope()
{
	for (LListIterator<Axis> li(axisList); li<LIST_END; ++li) {
		if (li.obj()->getRope()->ropeType == ManipulatorRope::centralRope) {
			return(li.obj()->getRope());
		}
	}
	return(NULL);
}


void PolyAxis::tensionFind(void) // find and display location based on tensions
{
	ManPos	*tmpManPos;

	tmpManPos = newManPos();
	if (!tmpManPos) return;

	float *tensions = getTensions();

	// this will assume the central tension unchanged
	if (!tmpManPos->calcState(tensions)) {

		ThreeVector 	tmp = tmpManPos->getPosition();

		sprintf(display->outString,"Tension Position: %7.2f %7.2f %7.2f",tmp[X],tmp[Y],tmp[Z]);
		display->message();

		tmp -= manPos->getPosition();
		sprintf(display->outString,"Position Error:   %+7.2f %+7.2f %+7.2f",
			tmp[X],tmp[Y],tmp[Z]);
		display->message();

		tmp = tmpManPos->calcNetForce(tensions);
		sprintf(display->outString,"Net force:        %7.2f %7.2f %7.2f  norm = %.2f",
			tmp[X],tmp[Y],tmp[Z],tmp.norm());
		display->message();

	} else {

		display->message("Calculation Error!");
	}
	delete tmpManPos;
}

int PolyAxis::getNumCommands()
{
	int num = POLYAXIS_COMMAND_NUMBER + getNumAxes();
	if (umbilical) ++num;
	return(num);
}
Command *PolyAxis::getCommand(int num)
{
	if (num < POLYAXIS_COMMAND_NUMBER) {
		return commandList[num];
	} else {
		return axisCommandList[num - POLYAXIS_COMMAND_NUMBER];
	}
}

/******************************************************************/
/*																																*/
/*	Reset the source to a known location													*/
/*																																*/
/******************************************************************/
void PolyAxis::locate(ThreeVector xs)
{
	LListIterator<Axis> li(axisList);

	// this will assume the central tension unchanged
	if (manPos->calcState(xs,getTensions())) {
		display->message("ManPos calculation error!");
	} else {
		for (li=0; li<LIST_END; ++li) {
			li.obj()->setLength(manPos->getLength(li));
		}
		// recalculate source position to be consistent with new rope lengths - PH 05/26/99
		calcSourcePosition();
		
		if (umbilical) {
//			umbilical->setLength(umbilical->calcLength(xs));
			// make umbilical length consistent with new calculated position - PH 05/26/99
//			umbilical->setLength(umbilical->calcLength(sourcePosition));
			// use init() command instead (now corrects for slack) - PH 06/05/00
				umbilical->init(xs);
		}
		updateDatabase();                     // save our new position
	}
}


void PolyAxis::pattern(char* inString)
{
	static char *delim = " \t\n\r";
	char	*pt = strtok(inString,delim);

	patternFile = fopen(pt,"r");	// open pattern file
	if (!patternFile)
	{
		sprintf(display->outString,"Error opening pattern file: %s",inString);
		display->message();
		return;
	}
	pt = strtok(NULL,delim);
	if (!pt) patternWaitTime = kPatternWaitTime;
	else patternWaitTime = atof(pt);

	if (!fgets(patternString,80,patternFile))	// read pattern logfile name
	{
		sprintf(display->outString,"Error reading logfile name from pattern file: %s",inString);
		display->message();
		patternStop();
	}

	patternLogFile = fopen(noWS(patternString),"a");	// open pattern log file
	if (!patternLogFile)
	{
		sprintf(display->outString,"Error opening pattern log file: %s",patternString);
		display->message();
		patternStop();
		return;
	}

	sprintf(display->outString, "New pattern started: %s wait time %.0f sec",
					inString, patternWaitTime);
	display->message();
	fprintf(patternLogFile,"%s  ",RTC->timeStr(RTC->time()));
	fprintf(patternLogFile,"%s\n",display->outString);

	patternFlag = 1;                    // flag pattern running mode on

	if (!fgets(patternString,80,patternFile))	// read first x,y,z location
	{
		sprintf(display->outString,"Error reading first x,y,z from pattern file: %s",inString);
		display->message();
		patternStop();
	}

	to(patternString);	// start source in motion
}

void PolyAxis::patternStop()
{
	patternFlag = 0;
	if (patternFile) {
		fclose(patternFile);
		patternFile = NULL;
	}
	if (patternLogFile) {
		fclose(patternLogFile);
		patternLogFile = NULL;
	}
}

void PolyAxis::nextPatternPoint(void)	// run next pattern point, or finish up
{
	if (!patternFlag) return;	// do nothing if pattern not running

	// print position
	fprintf(patternLogFile,"%s  %.2f %.2f %.2f", RTC->timeStr(RTC->time()),
				sourcePosition[X],sourcePosition[Y],sourcePosition[Z]);

	// print tensions
	for (LListIterator<Axis> li(axisList); li<LIST_END; ++li) {
		fprintf(patternLogFile," %.2f",li.obj()->getTension());
	}
	// print stop cause
	fprintf(patternLogFile," %s\n",stopCause[stopCauseFlag]);

	if ((sourcePosition-sourcePath->pos).norm() > kMaxPatternError) {
		display->message("Pattern aborted!");
		patternStop();
		return;
	}

	for (;;) {
		if (!fgets(patternString,80,patternFile) ||
				strlen(patternString) < 5)	// read next x,y,z location
		{
			display->message("Done running pattern");
			patternStop();
			break;
		}

		patternString[strlen(patternString)-1] = '\0'; // get rid of \n

		if (!strcasecmp(patternString,"REPEAT")) // repeat the pattern forever
		{
			rewind(patternFile);
			fgets(patternString,80.,patternFile); // drop logfile line
		} else {
			pauseTime = RTC->time();	// set up delay time for next move
			break;
		}
	}
}

void PolyAxis::setAlias(char *str)
{
	int i;
	char buff[64];
	str = strtok(str,delim);
	if (!str || !str[0]) {
		str = getAlias();
		if (str) {
			sprintf(display->outString,"%s alias is %s",getTrueName(),str);
			display->message();
			sprintf(display->outString,"Use \"%s alias -\" to delete alias",getTrueName());
		} else {
			sprintf(display->outString,"%s has no alias",getTrueName());
		}
		display->message();
		return;
	}
	if (findObject(str)) {
		display->message("Error: Specified name already exists");
		return;
	}
	if (!strcmp(str,"-")) str = "";
	for (i=0; i<63 && str[i]; ++i) {
		buff[i] = toupper(str[i]);
	}
	buff[i] = '\0';
	str = buff;
	Controller::setAlias(str);
	if (str[0]) {
		sprintf(display->outString,"Alias for %s set to %s",
				getTrueName(), getAlias());
	} else {
		sprintf(display->outString,"Alias for %s deleted", getTrueName());
	}
	display->message();

	OutputFile out("POLYAXIS",getTrueName(),1.00);	// create output object
	out.setHardFail(0);
	if (out.updateLine("ALIAS:", str)) {
		display->message("Warning: Not updated in POLYAXIS.DAT");
		display->message("(did you add an ALIAS entry for this object?)");
	}
}

void PolyAxis::updateDatabase(int updateCode)
{
	char buff[128];		// temporary string buffer
	OutputFile out("POLYAXIS",getTrueName(),1.00);	// create output object

	if (updateCode & kUpdateAxes) {
		char *pt = buff;

		*pt = '\0';	// initialize string in case we have no axes

		for (LListIterator<Axis> li(axisList); li<LIST_END; ++li) {
			pt += sprintf(pt, " %s", li.obj()->getObjectName());
		}
		if (umbilical) {
			pt += sprintf(pt, " %s", umbilical->getObjectName());
		}
		out.updateLine("AXES:", buff);
	}

	if (updateCode & kUpdatePosition) {

		// don't let NAN get into the files
		for (int i=0; i<3; ++i) if (isNAN(sourcePosition[i])) {
			quit("Source position calculation error!");
		}
		sprintf(buff,"%.8g %.8g %.8g",
			sourcePosition[X],sourcePosition[Y],sourcePosition[Z]);

		out.updateLine("POSITION:", buff);
	}

	if (updateCode & kUpdateMotion) {
		out.updateLine("MOTION:", motionString[motionFlag]);
	}

	if (updateCode & kUpdateEqualize) {
		out.updateLine("EQUALIZE_TENSIONS:", (char *)(equalizeFlag ? "ON" : "OFF"));
	}

	if (updateCode & kUpdateSourceOffset) {
		sprintf(buff,"%.8g %.8g %.8g",
			sourceOffset[X],sourceOffset[Y],sourceOffset[Z]);

		out.updateLine("SOURCE_OFFSET:", buff);
	}

	if (updateCode & kUpdateOrientation) {
		out.setHardFail(0);
		int err = out.updateFloat("ORIENTATION:", (float)sourceOrientation);
		if (err && updateCode == kUpdateOrientation) {
			display->message("Warning: Orientation not saved to POLYAXIS.DAT");
			display->message("(you must add ORIENTATION: after SOURCE_OFFSET:)");
		}
		out.setHardFail(1);
	}

	updateFlag = 0;		// reset update flag
}

