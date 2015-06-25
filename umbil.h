//---------------------------------------------------------------------------------
// File:				Umbil.h
//
// Description:	Routines to handle lookup tables for umbilical parameters
//
// Author:			Phil Harvey
//
// Revisions:		06/19/97 - PH created
//

#include <stdio.h>
#include "axis.h"
#include "threevec.h"

#define UMBILICAL_COMMAND_NUMBER	3

const int	kUmbilicalAxisType	= 1;

struct LookupInfo {
	float	x,y;						// source position for first calculated point
	float	dx,dy;					// distance between calculated points
	float	level, dlevel;	// initial water level, and water level increment
	short	nx,ny;					// number of calculated points in x and y
	short	nlevel;					// number of water levels (one per file)
	short	inNeck;					// non-zero if source is in neck
	char	nameFormat[16];	// file name formats for these tables
};

struct XLookupInfo : LookupInfo {
	float	maxX, maxY, maxLevel;
};

enum ESourceStatType {
	kStatsLength,
	kStatsDriveTension,
	kStatsHorizTension,
	kStatsVertTension,
	kStatsRubForce
};

struct SourceStats {
	float	length;				// length of umbilical (cm)
	float	driveTension;	// tension at umbilical drive (N)
	float	horizTension;	// horizontal tension at source (N, always positive)
	float	vertTension;	// vertical tension at source (N, up is positive);
	float	rubForce;			// force on rub ring (N)
};


class Umbilical : public Axis {
public:
							Umbilical(char* axisName, AV *av, FILE *fp=NULL);
							~Umbilical();

  virtual void	monitor(char *outString);
  
	virtual int	getAxisType()					 { return kUmbilicalAxisType;	}
	
	double			getLinearDensity()				 { return linearDensity;	}
	double 			getCrossSection()					 { return crossSection;		}

	double			calcDriveSpeed(ThreeVector &pos,ThreeVector vel);
	ThreeVector	calcConstLengthDir(ThreeVector &pos, ThreeVector &destDir);
	ThreeVector	calcNeutralPos(double length);

	double			calcLength(ThreeVector &pos)
								{ return(slack+interpolate(pos,kStatsLength));	}
	double			calcDriveTension(ThreeVector &pos)
								{ return(interpolate(pos,kStatsDriveTension));	}
	double			calcHorizTension(ThreeVector &pos)
								{ return(interpolate(pos,kStatsHorizTension));	}
	double			calcVertTension(ThreeVector &pos)
								{ return(interpolate(pos,kStatsVertTension));		}
	double			calcRubForce(ThreeVector &pos)
								{ return(interpolate(pos,kStatsRubForce));}

	void				init()				{ init(lastPosition);	}
	void				init(ThreeVector &xs);
	void				setSlack(char *inString);
	double			getSlack()		{ return(slack); }
	void				addSlack(char *inString);

protected:
	virtual Command *getCommand(int num);
	virtual int 		getNumCommands();

private:

	double			interpolate(ThreeVector &pos,ESourceStatType t);
	int					lookup(ThreeVector &pos, double level);
	void				calcInterLevelY(ESourceStatType t);
	void				calcInterX();

	double			linearDensity;	// mass per unit length of rope
	double			crossSection;		// cross-sectional area in cm^2
	double			slack;					// slack in umbilical length
	ThreeVector	lastPosition;		// last xyz position
	double			lastX, lastY;		// x and y coordinates for last position

	const float	*waterLevelPt;
	const ThreeVector &neckPosition;
	XLookupInfo	*lookupInfo;
	int					numLookups;
	int					curLookup;
	int					oneLevel;				// flag indicating only one water level for lookup
	int					levelNum;

	double			mLevel0, mLevel1;
	double			mX0, mX1;
	double			mY0, mY1;
	SourceStats	mStat[2][2][2];
	FILE				*mFile[2];
	char				*path;
	int					pathlen;

	static double		interLevel[2][2];	// intermediate values interpolated to water level
	static double		interY[2];				// intermediate values interpolated in Y
	static double		interX[2];				// intermediate values interpolated in X

	static Command *commandList[UMBILICAL_COMMAND_NUMBER];	// list of commands

	static class Init: public Command {
		public:
			Init(void):Command("init") { }
			char* subCommand(Hardware *hw, char* inString) {
				((Umbilical*) hw)->init();
				return "HW_ERR_OK";
			}

	} cmd_init;
	static class Slack: public Command {
		public:
			Slack(void):Command("slack") { }
			char* subCommand(Hardware *hw, char* inString) {
				((Umbilical*) hw)->setSlack(inString);
				return "HW_ERR_OK";
			}
	} cmd_slack;
	static class AddSlack: public Command {
		public:
			AddSlack(void):Command("addSlack") { }
			char* subCommand(Hardware *hw, char* inString) {
				((Umbilical*) hw)->addSlack(inString);
				return "HW_ERR_OK";
			}
	} cmd_addSlack;
};
