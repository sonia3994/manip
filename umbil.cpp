//---------------------------------------------------------------------------------
// File:				Umbil.cpp
//
// Description:	Routines to handle lookup tables for umbilical parameters
//
// Author:			Phil Harvey
//
// Revisions:		06/19/97 - PH created
//


#include <string.h>
#include "umbil.h"
#include "av.h"

// static variables
static FILE *tempFile;	// used by constructor

// static member declarations
double	Umbilical::interLevel[2][2];
double	Umbilical::interY[2];
double	Umbilical::interX[2];

Umbilical::Init		Umbilical::cmd_init;
Umbilical::Slack	Umbilical::cmd_slack;
Umbilical::AddSlack	Umbilical::cmd_addSlack;

Command* Umbilical::commandList[UMBILICAL_COMMAND_NUMBER] = {
	&cmd_init,
	&cmd_slack,
	&cmd_addSlack
};


Umbilical::Umbilical(char* objectName,// name of this object
									 AV *av,					// acrylic vessel object (for co-ordinates)
									 FILE *fp)				// optional file
					// keep database file open, and obtain file pointer in tempFile
				 :Axis(objectName, av, fp, &tempFile),
				 neckPosition(av->getNeckRingPosition()),
				 lastPosition(0,0,0), lastX(0), lastY(0)
{
	// open input file and find entry
	InputFile in("AXIS", objectName, AXIS_VER, NL_FAIL, NULL, tempFile);

	// initialize variables from database (re-written PH 02/20/97)
	linearDensity = in.nextFloat("LINEAR_DENSITY:");
	crossSection  = in.nextFloat("CROSS_SECTION:");
	slack				  = in.nextFloat("SLACK:");

	char *filename = noWS(in.nextLine("INDEX_FILE:",NULL,0));


	// save the path spec
	char *pt = strrchr(filename,'/');
	if (!pt) pt = strrchr(filename,':');
	if (!pt) pt = filename;
	else ++pt;
	pathlen = (int)(pt - filename);
	path = new char[pathlen+1];
	if (!path) quit("path","Umbilical::Umbilical");
	memcpy(path,filename,pathlen);
	path[pathlen] = 0;	// null terminate it
//
// read index file
//
	FILE *fp2 = fopen(filename, "rb");
	if (!fp2) {
		qprintf("\nCan't open %s index file: %s\n",objectName,filename);
		quit("aborting");
	}
	fseek(fp2,0L,SEEK_END);		// go to the end of the file
	numLookups = (int)(ftell(fp2) / sizeof(LookupInfo));
	lookupInfo = new XLookupInfo[numLookups];
	if (!lookupInfo) quit("lookupInfo","Umbilical::Umbilical");
	fseek(fp2,0L,SEEK_SET);
	for (int i=0; i<numLookups; ++i) {
		if (fread(lookupInfo+i, sizeof(LookupInfo), 1, fp2) != 1) {
			qprintf("Error reading file %s\n",filename);
			quit("aborting");
		}
		lookupInfo[i].maxX = lookupInfo[i].x + (lookupInfo[i].nx - 1) * lookupInfo[i].dx;
		lookupInfo[i].maxY = lookupInfo[i].y + (lookupInfo[i].ny - 1) * lookupInfo[i].dy;
		lookupInfo[i].maxLevel = lookupInfo[i].level + (lookupInfo[i].nlevel - 1) * lookupInfo[i].dlevel;
	}
	fclose(fp2);

	mLevel0 = -1e20;
	mLevel1	= -1e20;
	curLookup = 0;
	oneLevel	= 1;
	levelNum	= -1;

	mFile[0] = mFile[1] = 0;
	
	if (isInHeavyWater()) {
		waterLevelPt = &av->getHeavyWaterLevel();
	} else {
		waterLevelPt = &av->getLightWaterLevel();
	}
}


Umbilical::~Umbilical()
{
	if (mFile[0]) fclose(mFile[0]);
	if (mFile[1]) fclose(mFile[1]);
	delete lookupInfo;
	delete path;
}


void Umbilical::monitor(char *outStr)
{
	Axis::monitor(outStr);			// let Axis display its information
	
	outStr = strchr(outStr,0);	// find end of string
	
	outStr += sprintf(outStr,"Tabulated tension:  %6.2f\n", calcDriveTension(lastPosition));
	outStr += sprintf(outStr,"Tabulated length:   %6.2f (incl. %.2f cm slack)\n", calcLength(lastPosition), getSlack());
	outStr += sprintf(outStr,"Lookup info: %d) %.0f",curLookup+1,mLevel0);
	if (oneLevel) outStr += sprintf(outStr,"\n");
	else outStr += sprintf(outStr," %.0f\n",mLevel1);
}

Command *Umbilical::getCommand(int num)	// add our command list to the base class
{
	int	numInherited = Axis::getNumCommands();

	if (num >= numInherited) {
		return(commandList[num - numInherited]);
	} else {
		return(Axis::getCommand(num));	// allow all inherited commands
	}
}

int Umbilical::getNumCommands()	// we have an expanded command set
{
	return(UMBILICAL_COMMAND_NUMBER + Axis::getNumCommands());
}

// set current axis length to agree with our tablulated length
// for the current position
void Umbilical::init(ThreeVector &xs)
{
	// now compensates for slack which is added by calcLength() - PH 06/05/00
	setLength(calcLength(xs) - slack);
}

// specifies the amount of slack
void Umbilical::setSlack(char *inString)
{
	char	*pt = strtok(inString, delim);

	if (pt) {
		slack = atof(pt);

		// update database file
		OutputFile out("AXIS",getObjectName(),AXIS_VER);
		out.updateFloat("SLACK:", slack);

		sprintf(display->outString,"Umbilical slack set to %.2f cm",slack);
	} else {
		sprintf(display->outString,"Umbilical slack is %.2f cm",slack);
	}
	display->message();
}

void Umbilical::addSlack(char *inString)
{
	float val = 0;
	if (inString) val = atof(inString);
	if (val) {
		slack += val;
		sprintf(display->outString,"%.2f cm slack added to %s",val,getObjectName());
		display->message();
	}
	float newLen = calcLength(lastPosition);
	sprintf(display->outString,"Driving %s by %.2f cm to a slack of %.2f cm",
					getObjectName(),newLen-getLength(),slack);
	display->message();
	to(newLen);
}

// calculate interLevel and interY
void Umbilical::calcInterLevelY(ESourceStatType type)
{
	int			i, j;
	double	t0, t1, frac;

	if (oneLevel) {
		// oneLevel mode -- no interpolation necessary for water level
		for (i=0; i<2; ++i) {
			for (j=0; j<2; ++j) {
				interLevel[i][j] = ((float *)&mStat[i][j][0])[type];
			}
		}
	} else {
		// interpolate to the specified water level
		frac = (*waterLevelPt - mLevel0) / (mLevel1 - mLevel0);
		for (i=0; i<2; ++i) {
			for (j=0; j<2; ++j) {
				t0 = ((float *)&mStat[i][j][0])[type];
				t1 = ((float *)&mStat[i][j][1])[type];
				interLevel[i][j] = t0 + frac * (t1 - t0);
			}
		}
	}

	// now interpolate to the specified Y coordinate
	frac = (lastY - mY0) / (mY1 - mY0);
	for (i=0; i<2; ++i) {
		t0 = interLevel[i][0];
		t1 = interLevel[i][1];
		interY[i] = t0 + frac * (t1 - t0);
	}
}

// calculate interpolation in X
void Umbilical::calcInterX()
{
	int			i;
	double	t0, t1, frac;

	// and interpolate to the specified X coordinate
	frac = (lastX - mX0) / (mX1 - mX0);
	for (i=0; i<2; ++i) {
		t0 = interLevel[0][i];
		t1 = interLevel[1][i];
		interX[i] = t0 + frac * (t1 - t0);
	}
}


// interpolate one of the source parameters for a specified location
double Umbilical::interpolate(ThreeVector &pos,ESourceStatType type)
{
	double	frac;

	if (lookup(pos,*waterLevelPt)) return(0);		// look up this position in the tables

	// interpolate in water level and Y coordinate
	calcInterLevelY(type);

	// finally, interpolate to the specified X coordinate
	frac = (lastX - mX0) / (mX1 - mX0);
	return(interY[0] + frac * (interY[1] - interY[0]));
}


// get table entries required to look up umbilical
// characteristics for the specified coordinates and water level
int Umbilical::lookup(ThreeVector &pos, double level)
{
	int					flags = 0;
	char				fname[128];
	XLookupInfo	*look = lookupInfo + curLookup;

	enum {
		kChangeXY 		= 0x01,	// XY has changed, but is still in table
		kChangeLevel	= 0x02,	// water level has changed, but is still in table
		kOutsideXY		= 0x04,	// XY coordinates are outside lookup table
		kOutsideLevel	= 0x08	// water level is outside lookup table
	};

	// set our current position
	if (pos != lastPosition) {
		lastPosition = pos;
		// convert from detector xyz coordinates to umbilical xy frame.
		// Reference x=0 to feedthrough location while in neck,
		// and center of AV neck ring while in vessel.
		ThreeVector		p2;
		if (isInHeavyWater() && pos[Z]<neckPosition[Z]) {
			p2 = pos - neckPosition;
		} else {
			p2 = pos - getXtop();
		}
		lastX = sqrt(p2[X]*p2[X]+p2[Y]*p2[Y]);
		lastY = pos[Z];	// don't use p2 here!
	}

	// only reloads entries from the files
	// if our current entries are obsolete
	if (lastX<mX0 || lastX>mX1) {
		flags |= kChangeXY;
		if (lastX<look->x || lastX>look->maxX) {
			flags |= kOutsideXY;
		}
	}
	if (lastY<mY0 || lastY>mY1) {
		flags |= kChangeXY;
		if (lastY<look->y || lastY>look->maxY) {
			flags |= kOutsideXY;
		}
	}
	// always try to get back into the tables if we are in oneLevel mode
	if (level<mLevel0 || level>mLevel1 || oneLevel) {
		flags |= kChangeLevel;
		if (level<look->level || level>look->maxLevel) {
			flags |= kOutsideLevel;
		}
	}

	if (flags) {

		// do we need to open new lookup table files?
		if (flags & (kChangeLevel | kOutsideLevel | kOutsideXY)) {

			// set flag to indicate we need to change lookup files
			int changeFiles = 1;
			int	newLookup = curLookup;

			if (flags & (kOutsideLevel | kOutsideXY)) {

				// search for a lookup which contains the specified coordinates
				float nearest = 1e20;
				int		nearestLookup = -1;

				for (newLookup=0; newLookup<numLookups; ++newLookup) {
					look = lookupInfo + newLookup;
					if (lastX>=look->x && lastX<=look->maxX &&
							lastY>=look->y && lastY<=look->maxY)
					{
						if (level>=look->level && level<=look->maxLevel) break;
						// keep track of the closest level file
						float diff;
						if (level < look->level) diff = look->level - level;
						else diff = level - look->maxLevel;
						if (diff < nearest) {
							nearest = diff;
							nearestLookup = newLookup;
						}
					}
				}
				// did we find a suitable lookup table?
				if (newLookup >= numLookups) {
					// nope.  Did we find one that is close enough?
					if (nearestLookup < 0) {
						// nope. return an error
						mLevel0 = mLevel1 = -1e20;
						curLookup = 0;
						oneLevel = 1;
						levelNum = -1;
						return(-1);		// error!
					} else {
						// yup. use the nearest lookup in oneLevel mode
						newLookup = nearestLookup;
						look = lookupInfo + newLookup;
						oneLevel = 1;
					}
				} else if (look->level == look->maxLevel) {
					/* special case if our water level is exactly */
					/* on the single level of a solitary lookup */
					oneLevel = 1;
				} else {
					oneLevel = 0;
				}
			}

			// open the necessary water-level files
			int newNum;
			if (oneLevel) {
				if (level < look->level) newNum = 0;
				else newNum = look->nlevel - 1;
				// check to see if we really need to load new files
				if (newNum==levelNum && newLookup==curLookup) {
					// can return now if x and y are unchanged as well
					if (!(flags & kChangeXY)) return(0);
					// no need to load new files after all
					changeFiles = 0;
				}
			} else {
				newNum = (level - look->level) / look->dlevel;
			}

			if (changeFiles) {
				// keep track of lookup and level numbers for the current files
				curLookup = newLookup;
				levelNum = newNum;
				mLevel0 = look->level + levelNum * look->dlevel;
				mLevel1 = mLevel0 + look->dlevel;

				// close existing files
				if (mFile[0]) { fclose(mFile[0]);	mFile[0] = 0; }
				if (mFile[1]) { fclose(mFile[1]); mFile[1] = 0; }

				memcpy(fname,path,pathlen);
				for (int k=0; k<2-oneLevel; ++k) {
					sprintf(fname+pathlen, look->nameFormat, k?mLevel1:mLevel0);
					mFile[k] = fopen(fname, "rb");
					if (!mFile[k]) {
						qprintf("Can't open file %s\n",fname);
						quit("aborting from Umbilical::lookup");
					}
				}
			}
		}

		int i = (lastX - look->x) / look->dx;
		if (i == look->nx-1) --i;	// handle case where we are right at end of tables
		mX0 = look->x + i * look->dx;
		mX1 = mX0 + look->dx;

		int j = (lastY - look->y) / look->dy;
		if (j == look->ny-1) --j;	// handle case where we are right at end of tables
		mY0 = look->y + j * look->dy;
		mY1 = mY0 + look->dy;

		// load the required entries from the lookup files
		int	count = 0;
		for (int i2=0; i2<2; ++i2) {
			long	offset = ((long)(i+i2) * look->ny + j) * sizeof(SourceStats);
			for (int k2=0; k2<2-oneLevel; ++k2) {
				// seek to the first Y entry for this X in the file
				if (fseek(mFile[k2], offset, SEEK_SET)) {
					quit("Seek error in Umbilical::lookup");
				}
				// read the two adjacent Y entries from the file
				for (int j2=0; j2<2; ++j2) {
					count += fread(&mStat[i2][j2][k2], sizeof(SourceStats), 1, mFile[k2]);
				}
			}
		}
		if (count != 4*(2-oneLevel)) {
			quit("File read error in Umbilical::lookup");
		}
	}
	return(0);
}


// calculate the speed for the drive motor given
// a source position and velocity (cm/s)
// (a positive speed lowers the umbilical)
double Umbilical::calcDriveSpeed(ThreeVector &pos, ThreeVector vel)
{
	if (lookup(pos,*waterLevelPt)) return(0);		// lookup this position in the tables

	// calculate direction of our x-axis
	ThreeVector xDir = pos;
	xDir[Z] = 0;
	if (xDir[X] || xDir[Y]) xDir.normalize();
	else xDir = iVector;	// handle case where we are on the x-y axis

	// transform velocity into our coordinate system
	double	vx = xDir.dot(vel);
	double	vy = vel[Z];

	// interpolate in water level and Y coordinate
	calcInterLevelY(kStatsLength);
	// interpolate in X coordinate too
	calcInterX();

	// calculate drive speed due to motion in X and Y
	double	sx = vx * (interY[1] - interY[0]) / lookupInfo[curLookup].dx;
	double	sy = vy * (interX[1] - interX[0]) / lookupInfo[curLookup].dy;

	return(sx + sy);	// return the total drive speed
}


// calculate direction of motion necessary to
// maintain the umbilical at a constant length
ThreeVector Umbilical::calcConstLengthDir(ThreeVector &pos, ThreeVector &destDir)
{
	if (lookup(pos,*waterLevelPt)) return(destDir);	// lookup this position in the tables

	// interpolate in water level and Y coordinate
	calcInterLevelY(kStatsLength);
	// interpolate in X coordinate too
	calcInterX();

	// calculate rate of rope length change due to motion in x and y
	double	sx = (interY[1] - interY[0]) / lookupInfo[curLookup].dx;
	double	sy = (interX[1] - interX[0]) / lookupInfo[curLookup].dy;

	// calculate direction of our x-axis
	ThreeVector xDir = pos;
	xDir[Z] = 0;
	if (xDir[X] || xDir[Y]) xDir.normalize();
	else xDir = iVector;	// handle case where we are on the x-y axis

	ThreeVector	constDir;
	if (sx) {
		constDir = xDir.operator*((-sy / sx));
		constDir[Z] = 1;
		constDir.normalize();
	} else {
		constDir = kVector;
	}
	// now set the absolute direction to be towards the destination
	if (constDir.dot(destDir) < 0) {
		constDir = -constDir;
	}
	return(constDir);
}


// calculate rest position of source for umbilical
// of given length
ThreeVector Umbilical::calcNeutralPos(double length)
{
	ThreeVector	pos = getXtop();

	pos[Z] -= length;

	if (pos[Z] < neckPosition[Z]) {
		pos[X] = neckPosition[X];
		pos[Y] = neckPosition[Y];
	}
	return(pos);
}
