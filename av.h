#ifndef __AV_H
#define __AV_H
#include "global.h"
#include "rotation.h"

#include "hardware.h"
#include "controll.h"
#include "sensrope.h"
#include "threevec.h"

#define HOME


#define AV_COMMAND_NUMBER 9

struct VesselBlock {
  char        *name;      // name of block ("north","south",etc)
	ThreeVector positionV;  // position in vessel coordinates
  ThreeVector positionG;  // position in global coordinates
  float       radius;     // block radius
  float       offset;     // offset from rope exit to start of curvature
};


class AV:public Controller
{
 private:

  enum PollStep {
    emErrorInPoll,
    emGetData,
    emFirstPoint,
    emSecondPoint,
    emThirdPoint,
    emCalcTranslation,
		emCalcRotation,
    emCheckConsistency,
		emPutTogether
  } mPollStep;

	FILE* finSud;
	float mChiSquareLimit;
	float mPoll1Resid;
	float mPoll2Resid;
	float mChiSquare;
	float mOldResid;
	float mRopeDiffLimit;
	int mPollTechnique;
	int mPoll2Initialized;
	RotationMatrix mPoll2Rot;
	float** mPoll2Mat;
	int mSkip[6];
	int mWhichNA[7];
	int mSkipped;
	int mJustFinishedPoll;
  float           mTheta;       // These angles are for creating the
  float           mPhi;         // transformation to convert from AV to global
  float           mPsi;         // NOTE: These are NOT Euler angles. They  are
	// (Goldstein p608-9) yaw (phi),pitch (theta) and roll(psi)

	ThreeVector mOldVtogTrans;
  float mOldTheta;
  float mOldPhi;
	float mOldPsi;
	int   mLastPollOK;

  VesselBlock mblocks[4];       // manipulator attachment blocks
	ThreeVector mNeckDirection;   // AV neck direction in global co-ordinates
	ThreeVector mNeckRingStaticPosition;  // neck ring position relative to AV centre
  ThreeVector mNeckRingPosition;
  ThreeVector mCenterPosition;    // the center of the vessel in global co-ords
  float       mNeckRingRadius;    // neck ring radius in cm
  float       mNeckRingROC;       // radius of curvature of neck ring round
  float       mNeckRadius;        // accessible radius of neck in cm
  float       mNeckLength;        // distance from neck ring to top of neck
	float       mVesselRadius;      // vessel radius in cm
	float				mVesselThickness;		// nominal vessel wall thickness
	float				mTubeRadius;				// tube inside face radius
  float       mHeavyWaterLevel;   // elevation of heavy water surface
	float       mLightWaterLevel;   // elevation of light water surface
  
	int         mAVSensing;         // non-zero if av position is calculated
	unsigned long int mMovementCounter;
  
  float       mHeightOfNeck;         // the height of neck above the AV center
  float       mThicknessOfTopBlock;  // thickness of the blocks to which the ropes are attached
  float       mThicknessOfFlange;    // thickness of the SS flange
  ThreeVector mTopBlockPosition[3];  // not quite neckAttach. 
  ThreeVector mTopBlockVessel[3];    // Where the top blocks are rel to vessel (but z = 0)

	ThreeVector mTopBlock;
  
	void getRopeInfo(void);
  ThreeVector vtog(ThreeVector x) const;  // transforms x from vessel to global co-ordinates
  ThreeVector gtov(ThreeVector x) const;  // transforms x from global to vessel co-ordinates
  const VesselBlock *getBlock(const char *axisName) const;

  void pollGetData(void);
  void pollFirstPoint(void);
  void pollSecondPoint(void);
  void pollThirdPoint(void);
  void pollCalcTranslation(void);
	void pollCalcRotation(void);
	void yawPitchRoll(void);
	void pollCheckRotation(void);
  void pollPutTogether(void);
  void pollWarnError(void);
	void reportBadPoll(void);
	void reportGoodPoll(void);

	void poll2Init(void);

	// Used in polling
	FILE* mfBadPoll;
	FILE* logFile;
	unsigned long    mNumberOfBadPolls;
	unsigned long    mNumberOfGoodPolls;

	float mLastLength[7];
	float mLastFailureLength[7];
	ThreeVector mLastTopBlock[3];
	ThreeVector mLastCenter;
	float mLastTheta;
	float mLastPhi;
	float mLastPsi;

	float       mPositionAgreement;     // precision of top block polling
	float       mConsistencyAgreement;  // to test if OK polling is consistent
	float       mOrthogCheck;           // Is rotation matrix unitary

	int         mPollOK;          // 1 if pollOK, 0 if failed
	int         mReDoFlag;        // 1 if ropes moved enough to warrant new poll
	ThreeVector mRestRope[7];     // Rest rope unit vectors
	float       mRestLength[7];   // Rest rope lengths

	SenseRope*  srope[7];         // level 3 hardware, PTRs to the ropes
	float       mLength[7];       // the 7 rope lengths as found by sropes
	unsigned    mRawLength[7];    // the 7 rope raw ADC values
	float       mDeltaL[7];       // change in ropelength from rest position
	float       mStartingDL[7];       // change in ropelength from rest position
	float       mOldDeltaL[7];    // used for checking if the AV has moved
	ThreeVector mSnout[7];
	ThreeVector mNeckAttach[3];
	ThreeVector mNeckAttachGlobal[3]; // the location of the neck attachment
	// points in global co-ords
	ThreeVector mDelta[7];    // the offset from mNeckAttach[i] to the rope hole
	ThreeVector mRopeTry[7];  // rope unit vector. changes with each iterartion
	ThreeVector mDeltaX;      // change in position between two iterations
	ThreeVector mTotalDeltaX; // sum of above for all iterations
	ThreeVector mBasis[3];    // need orthonormal basis in iterative routine
	int         mItNum;       // a counter on the number of iterations
	float       mSAlpha;      // These 4 floats are used in the
	float       mSBeta;       // s(S)olution for esach iteration
	float       mSGamma;      // Not to be confused with mFCAlpha and mFCBeta
	float       mSTheta;      // which f(F)ind the c(C)enter
	float       mDet;         // the determinant of the 2x2 equation
	float       mSrx;         // the RHS of the 2x2 eqn: x component
	float       mSry;         //            "            y
	float       mAnswerx;     // the solution to 2x2: x component
	float       mAnswery;     //            "         y

	float       mConstraint12;// Rigidity constraint
	float       mConstraint13;// Rigidity constraint
	float       mConstraint23;// Rigidity constraint
	ThreeVector mXi12;          // from first point to second point. changes in iterations
	ThreeVector mXi13;          //                     third
	ThreeVector mXi23;          //      second         third
	float       mXi12ln;        // length of above: changes from iteration to iteration
	float       mXi13ln;        // length of above
	float       mXi23ln;        // length of above
  float       mDeltaLCon;     // The change in "length" due to the constraint
  
  float**     mlhs;           // used in getting the rotation matrix
	float**     mrhs;           // allocated in constructor, deallocated in destructor
  
  ThreeVector mNormal;        // Normal to plane of neck ring
  
  ThreeVector     mVtogTrans;   // translational part of transformation
	RotationMatrix  mVtogRot;     // matrices of rotation
	RotationMatrix  mGtovRot;
  float           mFCAlpha;     // center of neck  = x1+(x2-x1)*alpha + (x3-x1)*beta + n*gamma
  float           mFCBeta;      // where this holds for x1,x2,x3 to be neckAttach or neckAttachGlobal
  float           mFCGamma;     // and n is perp to the palne of the neck ring
  
  // For testing only
	ThreeVector     mBottoms[4];  // the bottom block positions: testing only
  ThreeVector     mdX;          // input displcement
  float           mNAmovement;  // the distance the third neck attachment moves
  ThreeVector     mSnoutPermanent[7]; // the *real* snout locations
  float           mSnoutError;    // the error in the snout position
  ThreeVector     mSurveyTarget[4]; // for survey testing t Queen's
	float           mAverageBlock;

	double					mLastUpdateTime;
	int							mUpdateFlag;


	// angles (yaw, pitch roll) used by mrqfit.
	float mrqCphi,mrqSphi,mrqCthe,mrqSthe,mrqCpsi,mrqSpsi;
	// Rotation matrix used in mrqfit
	float mrqA11,mrqA12,mrqA13,mrqA21,mrqA22,mrqA23,mrqA31,mrqA32,mrqA33;
	float mrqX,mrqY,mrqZ;

public:
	AV(char* avName); // database constructor
	~AV();

	virtual void  monitor(char *outString);

	virtual void	updateDatabase();

	const ThreeVector & getCentrePosition(void) const {return mCenterPosition;}
	const ThreeVector & getNeckDirection(void) const {return mNeckDirection;}

	const ThreeVector & getBlockPosition(const char* axisName) const;
	const float & getBlockOffset(const char *axisName) const;
	const float & getBlockRadius(const char *axisName) const;

	const ThreeVector & getNeckRingPosition(void) const {return mNeckRingPosition;}
	const float & getNeckRingRadius(void) const {return mNeckRingRadius;}
	const float & getNeckRingROC(void) const {return mNeckRingROC;}
	const float & getVesselRadius(void) const {return mVesselRadius;}
	const float & getNeckRadius(void) const {return mNeckRadius;}
	const float & getNeckLength(void) const {return mNeckLength;}

	const float & getHeavyWaterLevel() const    { return mHeavyWaterLevel;  }
	void          setHeavyWaterLevel(char *str);
	const float & getLightWaterLevel() const    { return mLightWaterLevel;  }
	void          setLightWaterLevel(char *str);

	float				getVesselOutsideRadius() { return mVesselRadius + mVesselThickness; }
	float				getTubeRadius()					 { return mTubeRadius; }

	void        poll(void);     // reads AVPsense boards etc.
	void        poll1(void);     // reads AVPsense boards etc.
	void        poll2(void);     // reads AVPsense boards etc.
	void        poll3(void);     // reads AVPsense boards etc.
	void        sensingOn();    // turn on av sensing
	void        sensingOff();   // turn off av sensing
	void        setPollTechnique(char*); // changes poll technique
	void        setSkip(char*); // changes poll technique
	void        setChiSq(char*); // changes poll thresholds
	void        setPoll2Lim(char*); // changes poll thresholds
	void        mrqfit(void);       // find AV using mrqmin
	void        mrqfit(int silent);       // find AV using mrqmin
	void mrqmin(float x[], float y[], float sig[], int ndata, float a[],
				int ia[],int ma, float **covar, float **alpha, float *chisq,
				float *alamda);
	void mrqcof(float x[], float y[], float sig[], int ndata, float a[],
				int ia[], int ma, float **alpha, float beta[], float *chisq);
	void mrqFunc(float x, float* a, float* ymod, float* dyda, int ma);

private:
	static class SetChiSq : public Command  // find AV using mrqmin
		{public:
	SetChiSq(void):Command("setChiSqLim"){ ; }
	char* subCommand(Hardware* hw, char* inString)
		{
			((AV*) hw)->setChiSq(inString);
			return "HW_ERR_OK";
		}
		} cmd_setChiSq;
	static class SetPoll2Lim : public Command  // find AV using mrqmin
		{public:
	SetPoll2Lim(void):Command("setPoll2Lim"){ ; }
	char* subCommand(Hardware* hw, char* inString)
		{
			((AV*) hw)->setPoll2Lim(inString);
			return "HW_ERR_OK";
		}
		} cmd_setPoll2Lim;
	static class SetSkip : public Command  // find AV using mrqmin
		{public:
	SetSkip(void):Command("setSkip"){ ; }
	char* subCommand(Hardware* hw, char* inString)
		{
			((AV*) hw)->setSkip(inString);
			return "HW_ERR_OK";
		}
		} cmd_setSkip;
	static class ChangePollTechnique: public Command  // find AV using mrqmin
		{public:
	ChangePollTechnique(void):Command("polltechnique"){ ; }
	char* subCommand(Hardware* hw, char* inString)
		{
			((AV*) hw)->setPollTechnique(inString);
			return "HW_ERR_OK";
		}
		} cmd_changePollTechnique;
	static class Mrqfit: public Command  // find AV using mrqmin
		{public:
	Mrqfit(void):Command("mrqfit"){ ; }
	char* subCommand(Hardware* hw, char* inString)
		{
			((AV*) hw)->mrqfit();
			return "HW_ERR_OK";
		}
		} cmd_mrqfit;
		static class SensingOn: public Command  // turn on sensing
			{public:
	SensingOn(void):Command("on"){ ; }
	char* subCommand(Hardware* hw, char* inString)
		{
			((AV*) hw)->sensingOn();
			return "HW_ERR_OK";
		}
			} cmd_sensingon;
			static class SensingOff: public Command // turn off sensing
	{public:
	SensingOff(void):Command("off"){ ; }
	char* subCommand(Hardware* hw, char* inString)
		{
			((AV*) hw)->sensingOff();
			return "HW_ERR_OK";
		}
	} cmd_sensingoff;
	static class SetHeavyWaterLevel: public Command
		{public:
	SetHeavyWaterLevel(void):Command("heavyWaterLevel"){ ; }
	char* subCommand(Hardware* hw, char* inString)
		{
			((AV*) hw)->setHeavyWaterLevel(inString);
			return "HW_ERR_OK";
		}
		} cmd_setHeavyWaterLevel;
		static class SetLightWaterLevel: public Command
	    {public:
  SetLightWaterLevel(void):Command("lightWaterLevel"){ ; }
  char* subCommand(Hardware* hw, char* inString)
    {
      ((AV*) hw)->setLightWaterLevel(inString);
      return "HW_ERR_OK";
    }
	    } cmd_setLightWaterLevel;
	    
 protected:
	    virtual Command *getCommand(int num)  { return commandList[num]; }
	    virtual int     getNumCommands()      { return AV_COMMAND_NUMBER; }

 private:
	    static Command  *commandList[AV_COMMAND_NUMBER];  // list of commands
	    
};


#endif
