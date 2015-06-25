#ifndef __POLYAXIS_H
#define __POLYAXIS_H

#include <stdio.h>
#include "axis.h"
#include "random.h"
#include "global.h"

#include "av.h"
#include "manpos.h"

#include "hardware.h"
#include "controll.h"

#define POLYAXIS_COMMAND_NUMBER 18
#define	NET_FORCE_SAMPLE_NUMBER	10		// number of net force samples

#define	FORBIDDEN_OK			0 // forbidden() return values: first is OK
#define	FORBIDDEN_NECK		1 //	outside neck
#define	FORBIDDEN_VESSEL	2 //  outside vessel
#define	FORBIDDEN_LOW_T		3 //	tension low on a rope
#define	FORBIDDEN_HIGH_T	4 //	tension high on a rope
#define FORBIDDEN_ERROR		5 //  calculation error or bad position
#define FORBIDDEN_AV			6 //  hits the outside of the AV (in guide tube)
#define FORBIDDEN_PSUP		7 //  hits the outside of the PSUP (in guide tube)
#define FORBIDDEN_HIGH		8	//  above the top feedthrough location

const short	kMaxAxes			= 3;	// maximum number of axes per polyaxis
const short kMaxAxisCommands	= kMaxAxes + 1;

enum SourceOrientation {
	kOrientation_Unknown,
	kOrientation_North,
	kOrientation_East,
	kOrientation_South,
	kOrientation_West,
	kMaxSourceOrientation
};

enum PolyMode {		// monitor() routine must be updated if modes are changed
	kPolyIdle,			// polyaxis is idle
	kPolyAbort,			// currently aborting a move command
	kPolyDirect,		// moving in direct mode
	kPolyUmbil,			// moving in constant umbilical-length mode
	kPolyTension,		// polyaxis is equalizing tensions
	kPolySingleAxis	// polyaxis is in single axis mode
};

enum DatabaseUpdateCode {
	kUpdatePosition	= 0x01,
	kUpdateAxes			= 0x02,
	kUpdateMotion		= 0x04,
	kUpdateEqualize	= 0x08,
	kUpdateSourceOffset = 0x10,
	kUpdateOrientation = 0x20,
	kUpdateAll			= 0xff
};

enum PathFlags {
	kTensionUp			= 0x01
};

struct SourcePath
{
	SourcePath(SourcePath *nxt=0)
		 : next(nxt), mode(kPolyIdle), pos(0,0,0), inNeck(0), flags(0) { }

	ThreeVector	pos;
	PolyMode		mode;
	SourcePath	*next;
	int					inNeck;
	int					flags;
};


class Umbilical;


class PolyAxis:public Controller, public SourceWeight
{
private:

	LList<Axis>	axisList;				// linked list of axis objects

	AV			*acrylicVessel;			// acrylic vessel object
	ManPos	*manPos;						// manipulator position calculations

	ThreeVector sourcePosition; // estimated position of source (cm from PSUP centre)

	ThreeVector sourceOffset;		// offset from reported position to center of source
	int			sourceOrientation;	// orientation of source
	float		sourceMass;					// mass of source for this PolyAxis
	float		sourceVolume;				// volume of source for this PolyAxis
	float		sourceWidth;				// width of source (cm)
	float		sourceHeight;				// height of source (cm)
	float		waterDensity;				// density of water
	float		centralTension;			// central rope tension for guide tube motion
															// Note: this is the tension in air.  Tension in water
															//       will be reduced by the source buoyancy

	const float	*	waterLevelPt;	// pointer to water level

	int			patternFlag;				// 0 if not in pattern mode, non-zero otherwise
	int			updateFlag;					// non-zero if we need to update database
	int			motionFlag;					// EMotionFlag motion strategy
	int			equalizeFlag;				// non-zero to equalize tensions after moving
	int			inVessel;						// non-zero if we are moving in the vessel
	int			guideTubeDir;				// <0 for moving down in guide tube

	FILE		*patternFile;				// file with a series of to() commands
	FILE    *patternLogFile;		// log file for pattern endpoints
	char		patternString[80];	// string with next pattern location
	double	pauseTime;					// time from end of recent pattern point run

	Umbilical	*umbilical;				// umbilical for the polyaxis

	PolyMode 	polyMode;					// the current mode of the polyaxis object

	float		desiredTensions[kMaxAxes];	// desired tensions of each rope

	int			stopCauseFlag;			// flag records reason why object stopped
	int			soundCount;					// counter for error sounds
	double	soundTime;					// time to stop playing error sound
	float		patternWaitTime;		// seconds to wait at each pattern point

	int			single_axis;				// non-zero if driving in single axis mode
	int			lo_flags;						// single-axis flags for axes in low tension
	int			hi_flags;						// single-axis flags for axes in high tension

	SourcePath	*sourcePath;		// path of source to endpoint (guaranteed to be non-null)

	void		updateAxisCommands();				// build new list of axis commands
	void		errorSound();								// play error sound
	void		initManPos();								// initialize manPos object
	void		updateDatabase(int code); 	// helper function to update database
	void		patternStop();							// stop pattern mode
	void		nextPatternPoint(void);			// run the next pattern point
	int			calcSourcePosition();				// calculate source position from rope lengths
	int			forbidden(ThreeVector &point);	// defines allowed points
	void		moveSource();								// move source to next point in path
	float *	getTensions();							// return array of rope tensions
	void		newPath();									// reset source path
	float		getCentralTension(ThreeVector &pos);	// get central tension for current source location
	void		testCentralTension(Axis *centralAxis);
	void		changeFlag(char *inString, char *msg, int *flagPt, int updateFlag,
										 char *strings[]=0, int numStrings=0);

protected:
	static FILE	*tempFile;		// convenience variable for use by derived classes

	Axis 				*claimAxisObject(char *name);

public:
	PolyAxis(char* polyaxisName,  // constructor from database
					 AV *av,							// acrlic vessel object
					 FILE *fp=NULL,				// input file if already open at the correct position
					 FILE **fpOut=NULL);	// returns database file if not null

	~PolyAxis(void);              // destructor

	virtual void	rampDown();			// ramp down motors gradually
	virtual int   stop(int i=0);	// stop all motors NOW (i!=0 pattern mode off)
	virtual void	setAlias(char *str);

	virtual void	updateDatabase()				{ updateDatabase(kUpdatePosition);	}
	virtual void	monitor(char *outStr);  // over-ride monitor function in Controller
	virtual int		needUpdate()						{ return updateFlag;	}
	virtual float	getSourceWeight(ThreeVector &pos);

	void					setPolyMode(PolyMode mode)	 { polyMode = mode;	}
	PolyMode			getPolyMode()					{ return polyMode; }
	ThreeVector &	getSourcePosition()		{ return sourcePosition; }
	float					getPositionError();
	ManPos *			newManPos();

	char *getStopCause(void);
	int		getStopCauseFlag(void) { return stopCauseFlag;		} // reason for last stop
	int		isMoving(void)				 { return polyMode != kPolyIdle;	}
	LList<Axis>& getAxisList()	 { return axisList;					}
	Umbilical		*getUmbilical()	 { return umbilical;				}
	int		getNumAxes()					 { return axisList.count();	}
	ManipulatorRope *getCentralRope(void);

	float	getNetForce();
	void	reset();

	void  to(char* inString); 			// move source to position x
	void  by(char* inString); 			// move source by displacement
	void	to(ThreeVector point);		// move source to position x
	void	locate(ThreeVector xs);		// relocate source
	void	tensionFind(void);				// use tensions to find source
	void	pattern(char* inString);	// run a pattern file
	void	tensions(char* inString);	// find tensions at a point
	void	lengths(char* inString);	// find lengths at a point
	void	netforce(char* inString);	// display net force
	void	connect(char *inString, int init=0);	// connect axes to polyaxis
	void	disconnect(char *inString);	// disconnect axes
	void	motion(char *inString);		// set motion strategy
	void	equalize(char *inString);	// turn on/off tension equalization
	void	setSourceOffset(char *inString);
	void	setOrientation(char *inString);
//	void	setTensions();						// set all axis tensions to control values
	void	connections();						// display current connections

	void  poll(void);       // poll Hardware;
	void  depoll(void);     // turn off Hardware polling
	void  repoll(void);     // turn on Hardware polling
	char *axisCommand(char *axis, char *str);
	int		axisCommandSafe(Axis *axis, char *cmd, char *arg);
	int		axisMotionOK(Axis *axis, float delta);

private:
	class AxisCommand: public Command
	{public:
		AxisCommand(char *name):Command(name,kAccessCode1){ }
		char *subCommand(Hardware *hw, char *inString)
		{
			return ((PolyAxis*) hw)->axisCommand(getName(),inString);
		}
	};

	static class To: public Command
	{public:
		To(void):Command("to","# # #",kAccessCode1){ ; }
		char* subCommand(Hardware* hw, char* inString)
		{
			((PolyAxis*) hw)->to(inString);
			return "HW_ERR_OK";
		}
	} cmd_to;
	static class By: public Command
	{public:
		By(void):Command("by","# # #",kAccessCode1){ ; }
		char* subCommand(Hardware* hw, char* inString)
		{
			((PolyAxis*) hw)->by(inString);
			return "HW_ERR_OK";
		}
	} cmd_by;
	static class Locate: public Command
	{public:
		Locate(void):Command("locate","# # #",kAccessCode1){ ; }
		char* subCommand(Hardware* hw, char* inString)
		{
			((PolyAxis*) hw)->locate(inString);
			return "HW_ERR_OK";
		}
	} cmd_locate;
	static class TensionFind: public Command
	{public:
		TensionFind(void):Command("tensionFind",kAccessCode1){ ; }
		char* subCommand(Hardware* hw, char* inString)
		{
			((PolyAxis*) hw)->tensionFind();
			return "HW_ERR_OK";
		}
	} cmd_tensionfind;
	static class Tensions: public Command
	{public:
		Tensions(void):Command("tensions",kAccessAlways){ ; }
		char* subCommand(Hardware* hw, char* inString)
		{
			((PolyAxis*) hw)->tensions(inString);
			return "HW_ERR_OK";
		}
	} cmd_tensions;
	static class Lengths: public Command
	{public:
		Lengths(void):Command("lengths",kAccessAlways){ ; }
		char* subCommand(Hardware* hw, char* inString)
		{
			((PolyAxis*) hw)->lengths(inString);
			return "HW_ERR_OK";
		}
	} cmd_lengths;
/*	static class SetTensions: public Command
	{public:
		SetTensions(void):Command("setTensions"){ ; }
		char* subCommand(Hardware* hw, char* inString)
		{
			((PolyAxis*) hw)->setTensions();
			return "HW_ERR_OK";
		}
	} cmd_settensions;
*/
	static class NetForce: public Command
	{public:
		NetForce(void):Command("netForce",kAccessAlways){ ; }
		char* subCommand(Hardware* hw, char* inString)
		{
			((PolyAxis*) hw)->netforce(inString);
			return "HW_ERR_OK";
		}
	} cmd_netforce;
	static class Pattern: public Command
	{public:
		Pattern(void):Command("pattern"){;}
		char* subCommand(Hardware* hw, char* inString)
		{
			((PolyAxis*) hw)->pattern(inString);
			return "HW_ERR_OK";
		}
	} cmd_pattern;
	static class Stop: public Command
	{public:
		Stop(void):Command("stop", kAccessAlways){ ; }	// always allow user to stop!
		char* subCommand(Hardware* hw, char* inString)
		{
			((PolyAxis*) hw)->rampDown();	// ramp down motors to a stop
			return "HW_ERR_OK";
		}
	} cmd_stop;
	static class Halt: public Command
	{public:
		Halt(void):Command("halt", kAccessAlways){ ; }	// always allow user to stop!
		char* subCommand(Hardware* hw, char* inString)
		{
			((PolyAxis*) hw)->stop(1);	// sudden motor stop + turn off pattern mode
			return "HW_ERR_OK";
		}
	} cmd_halt;
	static class Connect: public Command
	{public:
		Connect(void):Command("connect",kAccessCode1){ ; }
		char* subCommand(Hardware* hw, char* inString)
		{
			((PolyAxis*) hw)->connect(inString,0);
			return "HW_ERR_OK";
		}
	} cmd_connect;
	static class Disconnect: public Command
	{public:
		Disconnect(void):Command("disconnect",kAccessCode1){ ; }
		char* subCommand(Hardware* hw, char* inString)
		{
			((PolyAxis*) hw)->disconnect(inString);
			return "HW_ERR_OK";
		}
	} cmd_disconnect;
	static class Reset: public Command
	{public:
		Reset(void):Command("reset",kAccessCode1){ ; }
		char* subCommand(Hardware* hw, char* inString)
		{
			((PolyAxis*) hw)->reset();
			return "HW_ERR_OK";
		}
	} cmd_reset;
	static class Motion: public Command
	{public:
		Motion(void):Command("motion",kAccessCode1){ ; }
		char* subCommand(Hardware* hw, char* inString)
		{
			((PolyAxis*) hw)->motion(inString);
			return "HW_ERR_OK";
		}
	} cmd_motion;
	static class Equalize: public Command
	{public:
		Equalize(void):Command("equalize",kAccessCode1){ ; }
		char* subCommand(Hardware* hw, char* inString)
		{
			((PolyAxis*) hw)->equalize(inString);
			return "HW_ERR_OK";
		}
	} cmd_equalize;
	static class SourceOffset: public Command
	{public:
		SourceOffset(void):Command("sourceoffset",kAccessCode1){ ; }
		char* subCommand(Hardware* hw, char* inString)
		{
			((PolyAxis*) hw)->setSourceOffset(inString);
			return "HW_ERR_OK";
		}
	} cmd_sourceoffset;
	static class Orientation: public Command
	{public:
		Orientation(void):Command("orientation",kAccessCode1){ ; }
		char* subCommand(Hardware* hw, char* inString)
		{
			((PolyAxis*) hw)->setOrientation(inString);
			return "HW_ERR_OK";
		}
	} cmd_orientation;
	static class Alias: public Command
	{public:
		Alias(void):Command("alias",kAccessCode1){ ; }
		char* subCommand(Hardware* hw, char* inString)
		{
			((PolyAxis*) hw)->setAlias(inString);
			return "HW_ERR_OK";
		}
	} cmd_alias;

protected:
	virtual Command *getCommand(int num);
	virtual int 		getNumCommands();

private:
	static Command	*commandList[POLYAXIS_COMMAND_NUMBER];	// list of commands
	Command					*axisCommandList[kMaxAxisCommands];	// axis commands
};


#endif

