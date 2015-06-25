#include "global.h"
#include "av.h"
#include "sensrope.h"
#include "nrutil.h"
#include "nr.h"
#include "datain.h"
#include "cjj_ran.h"

// AV_CALC_TRANSFORMS must be defined for the code to work.

#define AV_CALC_TRANSFORMS

// constants
const int   kMaxIterations=20;
const int   kDisplayLine = 23;
const float kReDoPollLimit = -0.1;  //centimeters,
const float	kMinUpdateTime = 10;		// minimum secs between database updates
						// make negative to always redo poll
const float kMoveCountThresh = 0.2;
const float kSmall = 1.0E-10;
const float kTORAD = 0.0174533;

// static member declarations
AV::SensingOn   AV::cmd_sensingon;
AV::SensingOff  AV::cmd_sensingoff;
AV::SetHeavyWaterLevel  AV::cmd_setHeavyWaterLevel;
AV::SetLightWaterLevel  AV::cmd_setLightWaterLevel;
AV::Mrqfit              AV::cmd_mrqfit;
AV::ChangePollTechnique AV::cmd_changePollTechnique;
AV::SetSkip             AV::cmd_setSkip;
AV::SetChiSq            AV::cmd_setChiSq;
AV::SetPoll2Lim         AV::cmd_setPoll2Lim;

Command* AV::commandList[AV_COMMAND_NUMBER] = {
	&cmd_sensingon,
	&cmd_sensingoff,
	&cmd_setHeavyWaterLevel,
	&cmd_setLightWaterLevel,
	&cmd_mrqfit,
	&cmd_changePollTechnique,
	&cmd_setSkip,
	&cmd_setChiSq,
	&cmd_setPoll2Lim

};



/************************************************************************/
/*                                                                      */
/*  AV database constructor.  Reads data from AV.DAT database file.     */
/*                                                                      */
/*                             -- written 96-02-23 tjr                  */
/*                                                                      */
/************************************************************************/


AV::AV(char* avName): // constructor from database
		 Controller("AV",avName)

{
	int i;
	char* key;            // pointer to character *AFTER* keyword
	InputFile in("AV",avName,1.00);

	mLastUpdateTime = 0;
	mUpdateFlag = 0;

	mLastCenter = in.nextThreeVec("CENTER:");
	ThreeVector rot = in.nextThreeVec("YPR:");
	mLastPhi = rot[0];
	mLastTheta = rot[1];
	mLastPsi = rot[2];

	for(i=0 ; i<7 ; i++ ) {
		char name[32];
		sprintf(name,"ROPE%d_LENGTH:",i);
		mLastLength[i] = in.nextFloat(name);
	}
	mNeckRingStaticPosition = in.nextThreeVec("NECK_RING_STATIC_POSITION:");

	mNeckRingRadius = in.nextFloat("NECK_RING_RADIUS:","<neck ring radius (cm)>");
	mNeckRingROC = in.nextFloat("NECK_RING_ROC:","<neck ring ROC (cm)>");
	mNeckRadius = in.nextFloat("NECK_RADIUS:","<accessible neck radius (cm)>");
	mNeckLength = in.nextFloat("NECK_LENGTH:","<neck length (cm)>");
	mVesselRadius = in.nextFloat("VESSEL_RADIUS:","<vessel radius (cm)>");
	mVesselThickness = in.nextFloat("VESSEL_THICKNESS:","<vessel thickness (cm)>");
	mTubeRadius = in.nextFloat("TUBE_RADIUS:","<inside tube face radius (cm)>");
	mHeavyWaterLevel = in.nextFloat("D2O_LEVEL:","<D2O level>");
	mLightWaterLevel = in.nextFloat("H2O_LEVEL:","<H2O level>");
	mHeightOfNeck = in.nextFloat("HEIGHT_TOP_OF_NECK:");
	mThicknessOfFlange = in.nextFloat("THICKNESS_OF_SS_FLANGE:");
	mThicknessOfTopBlock = in.nextFloat("THICKNESS_OF_TOP_BLOCKS:");
	char lLabel[50];
	char lSuffix[4];
	for(i=0;i<3;i++) {
		strcpy(lLabel,"TOP_BLOCK_POS_SURVEY_");
		sprintf(lSuffix,"%d:",i+1);
		strcat(lLabel,lSuffix);
		mTopBlockPosition[i] = in.nextThreeVec(lLabel);
	}
	for(i=0;i<3;i++) {
		strcpy(lLabel,"TOP_BLOCK_POS_VESSEL_");
		sprintf(lSuffix,"%d:",i+1);
		strcat(lLabel,lSuffix);
		mTopBlockVessel[i] = in.nextThreeVec(lLabel);
	}
	for(i=0;i<7;i++){
		strcpy(lLabel,"DELTA_");
		sprintf(lSuffix,"%d:",i+1);
		strcat(lLabel,lSuffix);
		mDelta[i] = in.nextThreeVec(lLabel);
		mDelta[i][Z] = 0.0;
	}

	// get manipulator block information

	char name1[100];
	for (i=0; i<4; ++i) {
		key = in.nextLine("BLOCK_LABEL:","<block name>");
		sscanf(key,"%s ",name1);

		stripComments(name1);
		mblocks[i].name = new char[strUP(name1)+1]; //strUP counts characters
		strcpy(mblocks[i].name,name1);
		mblocks[i].positionV = in.nextThreeVec("POSITION:");
		mblocks[i].radius = in.nextFloat("RADIUS:");
		mblocks[i].offset = in.nextFloat("OFFSET:");
	}

	for(i=0;i<7;i++)  {
		key = in.nextLine("SENSEROPE:","<sensrope name>");
		sscanf(key,"%s ",name1);
		srope[i] = new SenseRope(name1);
	}

	sensingOff(); //Mandatory. Does the initialization.
	key = in.nextLine("SENSING:","<Toggle for sensing>");
	(void)strUP(key);
	if(strstr(key,"ON")) sensingOn();
//	else display->message("Warning: The AV sensing is turned off.");

	mPollTechnique = (int)floor((in.nextFloat("POLL_TECHNIQUE:"))+0.1);
	mSkipped = (int)floor((in.nextFloat("SKIPPED_ROPE:"))+0.1);
	char tempStr[20];
	sprintf(tempStr,"%d",mSkipped);
	setSkip(tempStr);
	mChiSquareLimit = in.nextFloat("CHISQUARE_LIMIT:");
	mRopeDiffLimit = in.nextFloat("ROPE_DIFF_LIMIT:");
	mPositionAgreement = in.nextFloat("POSITION_AGREEMENT:");
	mConsistencyAgreement = in.nextFloat("CONSISTENCY_AGREEMENT:");
	mOrthogCheck = in.nextFloat("ORTHOG_CHECK:");

	// turn off sensing and initialize transform and rotations
	mlhs = matrix(1,3,1,3); // used in finding the rotation in polling
	mrhs = matrix(1,3,1,3);
	//  sprintf(display->outString,"About to enter AV::getRopeInfo().");

	//  display->messageB(1,kErrorLine);

	// set old values to design spec.

	mOldVtogTrans = 0.0;
	mOldTheta = 0.0;
	mOldPhi = 0.0;
	mOldPsi = 0.0;

	mLastPollOK = 0;
	mMovementCounter = 0;
//	mfBadPoll = fopen("BADAVPOLLS.DAT","a");
	mfBadPoll = NULL; // no bad poll file
	logFile = NULL;// fopen("AVLOG.DAT","w");
	mNumberOfBadPolls = 0;
	mNumberOfGoodPolls = 0;
	for( i=0 ; i<7 ; i++ ) mLastFailureLength[i] = 0.0;
	getRopeInfo(); // teach the AV about the ropes
	mPoll2Initialized = 0;
	mPoll2Mat=NULL;
	//  poll(); // the usual initial poll on start-up
	finSud = fopen("AVSUD.DAT","r");
	mChiSquare = 0.0;
	mPoll2Resid = 0.0;
}



AV::~AV()
{
	if (mUpdateFlag) updateDatabase();
	int i;
	for(i=0; i<7; i++) if(srope[i]!=NULL) delete srope[i];
	for(i=0;i<4;i++) delete[] mblocks[i].name;
	free_matrix(mlhs,1,3,1,3);     // Delete matrix used for rotation polling.
	free_matrix(mrhs,1,3,1,3);
	if( logFile != NULL ) fclose(logFile);
	if( mfBadPoll != NULL ) fclose(mfBadPoll);
}


void AV::monitor(char *outStr)
{
  int i;
	outStr += sprintf(outStr,"Mode: %s\n",mAVSensing ? "ON" : "OFF");
	outStr += sprintf(outStr,"Good: %ld  Bad: %ld Meth:%2d Res: %f\n",
										mNumberOfGoodPolls,mNumberOfBadPolls,mPollTechnique,mOldResid);
	outStr += sprintf(outStr,"Rope Lengths:");
	for (i=0; i<7; ++i) {
		outStr += sprintf(outStr," %.2f",mLength[i]);
	}
	outStr += sprintf(outStr,"\n");
	outStr += sprintf(outStr,"Raw ADCs:");
	for (i=0; i<7; ++i) {
	    float val;
	    if (mAVSensing) {
	        val = mRawLength[i];
	    } else {
	        val = srope[i]->getRawADC();
	    }
		outStr += sprintf(outStr," %.0f",val);
	}
	outStr += sprintf(outStr,"\n");
	outStr += sprintf(outStr,"NA1: %.2f %.2f %.2f\n",mLastTopBlock[0][0],
						 mLastTopBlock[0][1],mLastTopBlock[0][2]);
	outStr += sprintf(outStr,"NA2: %.2f %.2f %.2f\n",mLastTopBlock[1][0],
						 mLastTopBlock[1][1],mLastTopBlock[1][2]);
	outStr += sprintf(outStr,"NA3: %.2f %.2f %.2f\n",mLastTopBlock[2][0],
						 mLastTopBlock[2][1],mLastTopBlock[2][2]);

	outStr += sprintf(outStr,"Center: %.2f %.2f %.2f\n",mLastCenter[0],
										mLastCenter[1],mLastCenter[2]);
	outStr += sprintf(outStr,"YPR: %.3f %.3f %.3f\n",mLastPhi,mLastTheta,mLastPsi);

	outStr += sprintf(outStr,"Motion Cnt: %lu\n", mMovementCounter);
	outStr += sprintf(outStr,"D2O Lvl: %.2f\n",getHeavyWaterLevel());
	outStr += sprintf(outStr,"H2O Lvl: %.2f\n",getLightWaterLevel());

	outStr += sprintf(outStr,"Last Fail Lns:");
	for ( i=0; i<7; i++ ) {
		outStr += sprintf(outStr," %.2f",mLastFailureLength[i]);
	}
	outStr += sprintf(outStr,"\n");

/*
	outStr += sprintf(outStr,"Poll/Calculation OK: %s\n",mLastPollOK ? "YES": "NO");
	outStr += sprintf(outStr,"Translation: %.2f %.2f %.2f\n",
				mVtogTrans[X], mVtogTrans[Y], mVtogTrans[Z]);
	outStr += sprintf(outStr,"Rotation: %.4f %.4f %.4f\n",
						mTheta, mPhi, mPsi);     // FILL THIS IN!! - PH
	outStr += sprintf(outStr,"Rope Lengths: ");
	for (int i=0; i<7; ++i) {
		outStr += sprintf(outStr," %.2f",mLength[i]);
	}
	outStr += sprintf(outStr,"\n");


	//monitor the neck attachment points -mgb
	for (i = 0; i<3; i++){
	outStr += sprintf(outStr,"Neck At%2i: %.2f %.2f %.2f\n",i+1,
	mNeckAttachGlobal[i][X], mNeckAttachGlobal[i][Y],
	mNeckAttachGlobal[i][Z]);}

	outStr += sprintf(outStr,"Neck Cent: %.2f %.2f %.2f\n",
	(mNeckAttachGlobal[0][X]+mNeckAttachGlobal[1][X]+mNeckAttachGlobal[2][X])/3,
	(mNeckAttachGlobal[0][Y]+mNeckAttachGlobal[1][Y]+mNeckAttachGlobal[2][Y])/3,
	(mNeckAttachGlobal[0][Z]+mNeckAttachGlobal[1][Z]+mNeckAttachGlobal[2][Z])/3
	-mHeightOfNeck-mThicknessOfTopBlock-mThicknessOfFlange);

	outStr += sprintf(outStr,"Motion Cnt: %d\n", mMovementCounter);
	outStr += sprintf(outStr,"Heavy Water Level: %.2f\n",getHeavyWaterLevel());
	outStr += sprintf(outStr,"Light Water Level: %.2f\n",getLightWaterLevel());
	outStr += sprintf(outStr,"Next Poll:%2d\n",mPollStep);
*/


}



const VesselBlock *AV::getBlock(const char* axisName) const
{
	const VesselBlock *rtnBlock = NULL;
	int         ln = strlen(axisName);
	char        *name;

	name = new char[ln+1];
	if (!name) quit("out of mem");
	strcpy(name,axisName);
	strUP(name);
	for (int i=0; i<4; ++i) {
		if (strstr(name,mblocks[i].name)) {
			rtnBlock = mblocks + i;
			break;
		}
	}

	delete[] name;

	return rtnBlock;
}


const ThreeVector &AV::getBlockPosition(const char *axisName) const
{
	static ThreeVector  nullVector(0,0,0);
	const VesselBlock *theBlock = getBlock(axisName);
	if (theBlock) return(theBlock->positionG);
	else return(nullVector);

}


const float &AV::getBlockRadius(const char *axisName) const
{
	static float  nullFloat = 0;

	const VesselBlock *theBlock = getBlock(axisName);

	if (theBlock) return(theBlock->radius);
	else return(nullFloat);
}


const float &AV::getBlockOffset(const char *axisName) const
{
	static float  nullFloat = 0;

	const VesselBlock *theBlock = getBlock(axisName);

	if (theBlock) return(theBlock->offset);
	else return(nullFloat);
}



void AV::sensingOff()
{
	mPollOK = 0;
	mAVSensing = 0;
	mNeckDirection[X] = 0.f;          // initialization needed for forceMap
	mNeckDirection[Y] = 0.f;
	mNeckDirection[Z] = 1.f;

	mNeckRingPosition = mNeckRingStaticPosition;  // reset neck ring position - PH
	mCenterPosition = 0.0;
	mVtogTrans = 0.0;
	mVtogRot.setUnit();
	mGtovRot.setUnit();
	int i;
	for(i=0; i<7; i++) {
		srope[i]->pollOff();
		srope[i]->setLength(mLastLength[i]);
		mOldDeltaL[i]=1.0E9; // Some large number to force polling the first time
	}
	for(i=0;i<4;i++) mblocks[i].positionG = mblocks[i].positionV;
}



void AV::sensingOn(void)
{
	for(int i=0; i<7; i++) {
		srope[i]->pollOn();
	}
	mAVSensing = 1;
	mPollOK = 1;
	mPollStep = emGetData;
}


void AV::setHeavyWaterLevel(char *str)
{
	str = strtok(str, delim);
	if (!str) {
		sprintf(display->outString,"Heavy water level is %.2f cm",mHeavyWaterLevel);
		display->message();
	} else {
		sprintf(display->outString,"Heavy water level set to %s cm",str);
		display->message();

		mHeavyWaterLevel = atof(str);       // set the new water level
		// update data file
		OutputFile out("AV",getObjectName(),1.00);  // create output object
		out.updateLine("D2O_LEVEL:",str);

	}
}

void AV::setLightWaterLevel(char *str)
{
	str = strtok(str, delim);
	if (!str) {
		sprintf(display->outString,"Light water level is %.2f cm",mLightWaterLevel);
		display->message();
	} else {
		sprintf(display->outString,"Light water level set to %s cm",str);
		display->message();

		mLightWaterLevel = atof(str);       // set the new water level
		// update data file
		OutputFile out("AV",getObjectName(),1.00);  // create output object
		out.updateLine("H2O_LEVEL:",str);
	}
}

void AV::getRopeInfo(void)
{
	int i;
	mAverageBlock = 0.0;
	for(i=0;i<3;i++) mAverageBlock += mTopBlockPosition[i][Z];
	mAverageBlock/=3.0;
	for(i=0;i<3;i++)  {
		mNeckAttach[i] = mTopBlockPosition[i];
		mTopBlockVessel[i][Z] = mHeightOfNeck;
	}

	for(i = 0;i<7;i++)    mSnout[i] = srope[i]->getSnout();
	// start for AV find comes from rest position

	for(i=0;i<3;i++)  {
		mRestRope[i] = mNeckAttach[0] + mDelta[i] - mSnout[i];
		srope[i]->setZeroOffset(mNeckAttach[0]+mDelta[i]);
	}

	for(i=3;i<=4;i++) {
		mRestRope[i] = mNeckAttach[1] + mDelta[i] - mSnout[i];
		srope[i]->setZeroOffset(mNeckAttach[1]+mDelta[i]);
	}

	for(i=5;i<=6;i++) {
		mRestRope[i] = mNeckAttach[2] + mDelta[i] - mSnout[i];
		srope[i]->setZeroOffset(mNeckAttach[2]+mDelta[i]);
	}
	for(i=0;i<7;i++)  {
		mRestLength[i] = mRestRope[i].norm();
		mRestRope[i].normalize();
		srope[i]->poll();  //very important. Have changes zero offsets.
		if (!mAVSensing) srope[i]->setLength(mLastLength[i]);
	}
	// Find alpha and beta to get to middle of neck
	// x1 + alpha(x2-x1) + beta(x3-x1) + gamma*normal = 0
	// y1 + alpha(y2-y1) + beta(y3-y1) + gamma*normal = 0
	// z1 + alpha(x2-x1) + beta(x3-x1) + gamma*normal = 0
	float **llhs;
	float **lrhs;
	llhs = matrix(1,3,1,3);
	lrhs = matrix(1,3,1,1);
	ThreeVector lx21 = mTopBlockVessel[1] - mTopBlockVessel[0];
	ThreeVector lx31 = mTopBlockVessel[2] - mTopBlockVessel[0];
	lx21.normalize();
	lx31.normalize();
	ThreeVector lNormal = lx21.cross(lx31);
	lNormal.normalize();
	if(lNormal[Z] > 0.0) lNormal*=-1.0;

	llhs[1][1] = lx21[X]; llhs[1][2] = lx31[X]; llhs[1][3] = lNormal[X];
		lrhs[1][1] = -mTopBlockVessel[0][X];
	llhs[2][1] = lx21[Y]; llhs[2][2] = lx31[Y]; llhs[2][3] = lNormal[Y];
		lrhs[2][1] = -mTopBlockVessel[0][Y];
	llhs[3][1] = lx21[Z]; llhs[3][2] = lx31[Z]; llhs[3][3] = lNormal[Z];
		lrhs[3][1] = -mTopBlockVessel[0][Z];

	gaussj(llhs,3,lrhs,1);
	mFCAlpha = lrhs[1][1];
	mFCBeta  = lrhs[2][1];
	mFCGamma = lrhs[3][1];

	// Constraints for fitting.
	mConstraint12 = (mNeckAttach[1]-mNeckAttach[0]).norm();
	mConstraint13 = (mNeckAttach[2]-mNeckAttach[0]).norm();
	mConstraint23 = (mNeckAttach[2] - mNeckAttach[1]).norm();

	free_matrix(llhs,1,2,1,2);
	free_matrix(lrhs,1,2,1,1);

	mWhichNA[0] = mWhichNA[1] = mWhichNA[2] = 0;
	mWhichNA[3] = mWhichNA[4] = 1;
	mWhichNA[5] = mWhichNA[6] = 2;

}


void AV::setPollTechnique(char* aString)
{
	int technique = atoi(aString);
	if(technique>=1 && technique <=3) {
		mPollTechnique = technique;
		sensingOff();  // reset everything
		sensingOn();
		sprintf(display->outString,"Poll type%2d chosen. POlling reset and restarted.",technique);
		display->messageB(1,22);

	}
	else {
		sprintf(display->outString,"Poll type %d is illegal. Ignoring command...",technique);
		display->messageB(1,22);
	}
}

void AV::setChiSq(char* inString)
{
	float newCS = (float)atof(inString);
	mChiSquareLimit = newCS;


}

void AV::setPoll2Lim(char* inString)
{
	float newLim = (float)atof(inString);
	mRopeDiffLimit = newLim;


}


void AV::poll(void)
{
	switch(mPollTechnique) {
	case 1:
		poll1();
		break;
	case 2:
		poll2();
		break;
	case 3:
		poll3();
		break;
	default:
		sprintf(display->outString,"Illegal poll type (%d) chosen. Using 1.",mPollTechnique);
		display->messageB(1,22);
		poll1();
	}
	if(mJustFinishedPoll) {
		mJustFinishedPoll = 0;
		if( mPollOK ) {
			mLastPollOK = 1;
			reportGoodPoll();
		}
		else {
			mLastPollOK = 0;
			reportBadPoll();
		}
	}
	if (mUpdateFlag) updateDatabase();

/*
	if(logFile!=NULL ) {

		char hello[100];
		sprintf(hello,"%d %d %f\n",
									mNumberOfGoodPolls,mNumberOfBadPolls,mOldResid);
		fprintf(logFile,"%s",hello);


		fprintf(logFile,"%ld %ld %2d %f %f %f\n",mNumberOfGoodPolls,
						mNumberOfBadPolls,mPollTechnique,mLastCenter[0],mLastCenter[1],mLastCenter[2]);
		fprintf(logFile,"%.2f %.2f %.2f\n",mLastPhi,mLastTheta,mLastPsi);
		fprintf(logFile,"%.2f %.2f %.2f %.2f %.2f %.2f %.2f\n",
							mLastLength[0],mLastLength[1],mLastLength[2],mLastLength[3],
							mLastLength[4],mLastLength[5],mLastLength[6]);
		fprintf(logFile,"NA1: %.2f %.2f %.2f\n",mLastTopBlock[0][0],
						 mLastTopBlock[0][1],mLastTopBlock[0][2]);
		fprintf(logFile,"NA2: %.2f %.2f %.2f\n",mLastTopBlock[1][0],
						 mLastTopBlock[1][1],mLastTopBlock[1][2]);
		fprintf(logFile,"NA3: %.2f %.2f %.2f\n",mLastTopBlock[2][0],
						 mLastTopBlock[2][1],mLastTopBlock[2][2]);

	}
*/

}




void AV::poll1(void)

{
	static int slCounter=0;
	if (mAVSensing) {

		switch (mPollStep*mPollOK)  {
		case emErrorInPoll:
			mLastPollOK = 0;
			// Do not want to go to origin.
			//  sensingOff(); // initialize identity transformation
			sensingOn(); // but turn the sensing on so it will try again
			pollWarnError();
			mPollStep = emGetData;
			// Fall through. ie try the poll process again
		case emGetData:
			slCounter++;
			pollGetData();
			if (mReDoFlag) mPollStep = emFirstPoint;
			else mPollStep = emGetData; // start again
//			break;
		case emFirstPoint:
			pollFirstPoint();
			mPollStep = emSecondPoint;
//			break;
		case emSecondPoint:
			pollSecondPoint();
			mPollStep = emThirdPoint;
//			break;
		case emThirdPoint:
			pollThirdPoint();
			mPollStep = emCalcTranslation;
//			break;
		case emCalcTranslation:  // also checks constraint
			pollCalcTranslation();
			mPollStep = emCalcRotation;
//			break;
		case emCalcRotation:  // checks for orthog rotation matrix too
			pollCalcRotation();
			mPollStep = emPutTogether;
//			break;
		case emPutTogether:  // this can only get called if mPollOK>0
			pollPutTogether(); // if mPOllOK is zero then error in poll called
			mPollStep = emGetData;
			mJustFinishedPoll = 1;
			break;
		default: // shouldn't happen, but force restart next time
			mPollOK = 0;
			break;
		}
		if (mPollOK==0) mJustFinishedPoll = 1;
	}

}





void AV::reportGoodPoll() {
	mNumberOfGoodPolls++;
	int i;
	for (i=0; i<4; ++i) {
		mblocks[i].positionG = vtog(mblocks[i].positionV);
	}
	for(i=0 ; i<7 ; i++ ) {
		mLastLength[i] = mLength[i];
	}
	for(i=0 ; i<3 ; i++ ) {
		mLastTopBlock[i] = mNeckAttachGlobal[i];
	}
	mLastCenter = mVtogTrans;
	mLastTheta = mTheta;
	mLastPhi = mPhi;
	mLastPsi = mPsi;
	if(mPollTechnique==1)mOldResid = mPoll1Resid;
	if(mPollTechnique==2)mOldResid = mPoll2Resid;
	if(mPollTechnique==3)mOldResid = mChiSquare;
	updateDatabase();		// PH 05/06/03
}

void AV::reportBadPoll() {
	mNumberOfBadPolls++;
	int i;
	for(i=0 ; i<7 ; i++ ) mLastFailureLength[i] = mLength[i];

	if(mfBadPoll!=NULL && mNumberOfBadPolls<1000 ) {
		fprintf(mfBadPoll,"before step %d. %ld good. %ld bad\n",mPollStep,
						mNumberOfGoodPolls,mNumberOfBadPolls);
		fprintf(mfBadPoll,"%f %f %f %f %f %f %f\n",
			mLength[0],mLength[1],mLength[2],mLength[3],mLength[4],
			mLength[5],mLength[6]);
		fprintf(mfBadPoll,"%f %f %f %f %f %f %f\n",
			mStartingDL[0],mStartingDL[1],mStartingDL[2],mStartingDL[3],mStartingDL[4],
			mStartingDL[5],mStartingDL[6]);

	}
	if( mNumberOfBadPolls==1000 && mfBadPoll!=NULL) {
		fclose(mfBadPoll);
		mfBadPoll=NULL;
	}

}



void AV::pollWarnError(void)

{
	static int slErrorCounter = 1;
	char lErrorLine[80];
	sprintf(lErrorLine," AV Calculations failed. Using last value. Retrying next poll %d.",slErrorCounter);
	display->messageB(lErrorLine,1,kErrorLine+1);
	slErrorCounter++;
	mVtogTrans = mOldVtogTrans;
	mTheta = mOldTheta;
	mPhi = mOldPhi;
	mPsi = mOldPsi;
}



void AV::pollGetData(void)

{

	int i;
	float ldd;
	int moved = 0;
	mReDoFlag = 0;
	mPollOK = 1;
	for(i=0;i<7;i++) {
		if (mAVSensing) srope[i]->poll();
		mDeltaL[i] = srope[i]->getDeltaL();
		mLength[i] = srope[i]->getLength();
		mRawLength[i] = srope[i]->getUncalibrated();
		ldd = fabs(mDeltaL[i] - mOldDeltaL[i]);
		if(ldd>kReDoPollLimit) mReDoFlag = 1;
		if(ldd>kMoveCountThresh) moved = 1;
		mRopeTry[i] = mRestRope[i];  // these are unit vectors
	}
	mMovementCounter += moved;

	if(mReDoFlag) for(i=0;i<7;i++) {
		mOldDeltaL[i] = mDeltaL[i];
		mStartingDL[i] = mDeltaL[i];
	}

/*
	mPollOK = 1;
	mMovementCounter +=1;
	mReDoFlag = 1;
	char fileline[100];
	if(!fgets(fileline,99,finSud)) {
		printw("done!");
		exit(-1);
	}
	fgets(fileline,99,finSud);
	fgets(fileline,99,finSud);
	sscanf(fileline,"%f %f %f %f %f %f %f",mLength,mLength+1,mLength+2,
				 mLength+3,mLength+4,mLength+5,mLength+6);
	fgets(fileline,99,finSud);
	fgets(fileline,99,finSud);
	fgets(fileline,99,finSud);
	int i;
	for(i=0 ; i< 7 ; i++) {
		mDeltaL[i] = mLength[i] - mRestLength[i];
		mRopeTry[i] = mRestRope[i];
	}
	if(mReDoFlag) for(i=0;i<7;i++) {
		mOldDeltaL[i] = mDeltaL[i];
		mStartingDL[i] = mDeltaL[i];
	}
*/
}



void AV::pollFirstPoint(void)

{
	int i;
	mItNum = 0;
	mTotalDeltaX = 0.0;  // initialize to zero
	do {
		// The funny order of calculation helps because ropeTry[0]
		// and ropeTry[2] are almost orthog to start with.

		mBasis[0]  = mRopeTry[0];
		mBasis[2]  = mRopeTry[2] - mBasis[0]*(mRopeTry[2].dot(mBasis[0]));
		mBasis[2].normalize();
		mBasis[1]  = mRopeTry[1] - mBasis[0]*(mRopeTry[1].dot(mBasis[0]))
					 - mBasis[2]*(mRopeTry[1].dot(mBasis[2]));
		mBasis[1].normalize();

		mSAlpha = mBasis[1].dot(mRopeTry[1]); mSBeta  = mBasis[2].dot(mRopeTry[1]);
		mSGamma = mBasis[1].dot(mRopeTry[2]); mSTheta = mBasis[2].dot(mRopeTry[2]);
		mSrx = mDeltaL[1] - mDeltaL[0]*mBasis[0].dot(mRopeTry[1]);
		mSry = mDeltaL[2] - mDeltaL[0]*mBasis[0].dot(mRopeTry[2]);
		if( fabs(mDet = mSAlpha*mSTheta-mSBeta*mSGamma) > kSmall ) {
			mAnswerx = (mSTheta*mSrx - mSBeta*mSry)/mDet;
			mAnswery = (mSAlpha*mSry - mSGamma*mSrx)/mDet;
		}
		else {
			mPollOK = 0;
			break;
		}
		mDeltaX = mDeltaL[0]*mBasis[0] + mAnswerx*mBasis[1] + mAnswery*mBasis[2];
		mTotalDeltaX += mDeltaX;
		if( mDeltaX.norm() < 0.1*mPositionAgreement ) break;

		for(i=0;i<3;i++) {
			mRopeTry[i] = mRestLength[i]*mRestRope[i] + mTotalDeltaX;
			mDeltaL[i] = mLength[i] - mRopeTry[i].norm();
			mRopeTry[i].normalize();
		}

	} while (++mItNum<kMaxIterations);

	if(mItNum<kMaxIterations && mPollOK)
			mNeckAttachGlobal[0] = mNeckAttach[0]+mTotalDeltaX;
	else  mPollOK = 0;

}



void AV::pollSecondPoint(void)

{

	int i;
	mItNum = 0;
	mTotalDeltaX = 0.0;
	do {

		mXi12 = mNeckAttach[1]+mTotalDeltaX-mNeckAttachGlobal[0];
		mXi12ln = mXi12.norm();
		mDeltaLCon = mConstraint12 - mXi12ln;
		mXi12 /= mXi12ln;

		mBasis[0]  = mXi12;
		mBasis[1]  = mRopeTry[3] - mBasis[0]*(mRopeTry[3].dot(mBasis[0]));
		mBasis[1].normalize();
		mBasis[2]  = mRopeTry[4] - mBasis[0]*(mRopeTry[4].dot(mBasis[0]))
			- mBasis[1]*(mRopeTry[4].dot(mBasis[1]));
		mBasis[2].normalize();

		mSAlpha = mBasis[1].dot(mRopeTry[3]); mSBeta  = mBasis[2].dot(mRopeTry[3]);
		mSGamma = mBasis[1].dot(mRopeTry[4]); mSTheta = mBasis[2].dot(mRopeTry[4]);

		mSrx = mDeltaL[3] - mDeltaLCon*mBasis[0].dot(mRopeTry[3]);
		mSry = mDeltaL[4] - mDeltaLCon*mBasis[0].dot(mRopeTry[4]);

		if( fabs(mDet = mSAlpha*mSTheta-mSBeta*mSGamma) > kSmall ) {
			mAnswerx = (mSTheta*mSrx - mSBeta*mSry)/mDet;
			mAnswery = (mSAlpha*mSry - mSGamma*mSrx)/mDet;
		}
		else {
			mPollOK = 0;
			break;
		}

		mDeltaX = mDeltaLCon*mBasis[0] + mAnswerx*mBasis[1] + mAnswery*mBasis[2];
		mTotalDeltaX +=mDeltaX;
		if( mDeltaX.norm() < mPositionAgreement ) break;
		for(i=0;i<2;i++) {
			mRopeTry[i+3] = mRestRope[i+3]*mRestLength[i+3] + mTotalDeltaX;
			mDeltaL[i+3] = mLength[i+3] - mRopeTry[i+3].norm();
			mRopeTry[i+3] /= mRopeTry[i+3].norm();
		}
	} while(++mItNum<kMaxIterations);

	if(mItNum<kMaxIterations && mPollOK)
		mNeckAttachGlobal[1] = mNeckAttach[1]+mTotalDeltaX;
	else  mPollOK = 0;
}



void AV::pollThirdPoint(void)
{
	int i;
	mItNum = 0;
	mTotalDeltaX = 0.0;
	do {
		mXi13 = mNeckAttach[2]+mTotalDeltaX-mNeckAttachGlobal[0];
		mXi13ln = mXi13.norm();
		mDeltaLCon = mConstraint13 - mXi13ln;
		mXi13 /= mXi13ln;
		mBasis[0]  = mXi13;
		mBasis[1]  = mRopeTry[5] - mBasis[0]*(mRopeTry[5].dot(mBasis[0]));
		mBasis[1].normalize();
		mBasis[2]  = mRopeTry[6] - mBasis[0]*(mRopeTry[6].dot(mBasis[0]))
			- mBasis[1]*(mRopeTry[6].dot(mBasis[1]));
		mBasis[2].normalize();

		mSAlpha = mBasis[1].dot(mRopeTry[5]); mSBeta  = mBasis[2].dot(mRopeTry[5]);
		mSGamma = mBasis[1].dot(mRopeTry[6]); mSTheta = mBasis[2].dot(mRopeTry[6]);
		mSrx = mDeltaL[5] - mDeltaLCon*mBasis[0].dot(mRopeTry[5]);
		mSry = mDeltaL[6] - mDeltaLCon*mBasis[0].dot(mRopeTry[6]);

		if( fabs(mDet = mSAlpha*mSTheta-mSBeta*mSGamma) > kSmall ) {
			mAnswerx = (mSTheta*mSrx - mSBeta*mSry)/mDet;
			mAnswery = (mSAlpha*mSry - mSGamma*mSrx)/mDet;
		}
		else {
			mPollOK = 0;
			break;
		}

		mDeltaX = mDeltaLCon*mBasis[0] + mAnswerx*mBasis[1] + mAnswery*mBasis[2];
		mTotalDeltaX +=mDeltaX;
		if( mDeltaX.norm() < mPositionAgreement ) break;
		for(i=0;i<2;i++) {
			mRopeTry[i+5] = mRestRope[i+5]*mRestLength[i+5] + mTotalDeltaX;
			mDeltaL[i+5] = mLength[i+5] - mRopeTry[i+5].norm();
			mRopeTry[i+5].normalize();
		}
	} while(++mItNum<kMaxIterations);

	if(mItNum<kMaxIterations && mPollOK)
		mNeckAttachGlobal[2] = mNeckAttach[2]+mTotalDeltaX;
	else  mPollOK = 0;
}

void AV::pollCalcTranslation(void)
{
	mXi23 = mNeckAttachGlobal[2] - mNeckAttachGlobal[1];
	mXi23ln = mXi23.norm();
	mXi23.normalize();
	mPoll1Resid = fabs(mXi23ln-mConstraint23);
	if(mPoll1Resid>mConsistencyAgreement) mPollOK = 0;
	else {
		mXi12 = mNeckAttachGlobal[1] - mNeckAttachGlobal[0]; // not updated after last iter in FirstPoint
		mXi13 = mNeckAttachGlobal[2] - mNeckAttachGlobal[0]; // not updated after last iter in FirstPoint
		mNormal = mXi12.cross(mXi13);
		mXi12.normalize();
		mXi13.normalize();
		mNormal.normalize();
		if(mNormal[Z] >= 0.0) mNormal *=-1.0;
		mVtogTrans = mNeckAttachGlobal[0]
			+ mXi12 * mFCAlpha + mXi13 * mFCBeta + mNormal * mFCGamma;
	}

}



void AV::pollCalcRotation(void)
{
	int i;
	for(i=0;i<3;i++) {
//		mlhs[i+1][1] = mTopBlockVessel[i][X] + mVtogTrans[X];
//		mlhs[i+1][2] = mTopBlockVessel[i][Y] + mVtogTrans[Y];
//		mlhs[i+1][3] = mTopBlockVessel[i][Z] + mVtogTrans[Z];
//		mrhs[i+1][1] = mNeckAttachGlobal[i][X];
//		mrhs[i+1][2] = mNeckAttachGlobal[i][Y];
//		mrhs[i+1][3] = mNeckAttachGlobal[i][Z];
		mlhs[i+1][1] = mTopBlockVessel[i][X];
		mlhs[i+1][2] = mTopBlockVessel[i][Y];
		mlhs[i+1][3] = mTopBlockVessel[i][Z];
		mrhs[i+1][1] = mNeckAttachGlobal[i][X] - mVtogTrans[X];
		mrhs[i+1][2] = mNeckAttachGlobal[i][Y] - mVtogTrans[Y];
		mrhs[i+1][3] = mNeckAttachGlobal[i][Z] - mVtogTrans[Z];
	}
	gaussj(mlhs,3,mrhs,3);
	// rhs is now the transpose of the rotation matrix.
	// Better be unitary
	// If the row vectors of a complex matrix  form an orthonormal
	// set then the matrix is unitary

	float lSumCheck;
	float lSumTemp = 0.0;

	for(i=0;i<3;i++) lSumTemp += mrhs[1][i+1]*mrhs[2][i+1];
	lSumCheck = fabs(lSumTemp);
	lSumTemp = 0.0;
	for(i=0;i<3;i++) lSumTemp += mrhs[1][i+1]*mrhs[3][i+1];
	lSumCheck += fabs(lSumTemp);
	lSumTemp = 0.0;
	for(i=0;i<3;i++) lSumTemp += mrhs[2][i+1]*mrhs[3][i+1];
	lSumCheck += fabs(lSumTemp);


	if (lSumCheck > mOrthogCheck) mPollOK = 0;
	else {
		for(int p=0;p<3;p++) {
			lSumCheck = 0.0;
			for(i=0;i<3;i++) lSumCheck += mrhs[p+1][i+1]*mrhs[p+1][i+1];
			if (fabs(lSumCheck-1.0) > mOrthogCheck) {mPollOK = 0;break;}
		}
	}
}

void AV::pollPutTogether(void)
{
#ifndef AV_CALC_TRANSFORMS
//    sprintf(display->outString,"AV Sensing on but transformation NOT calculated.");
//    display->messageB(1,kErrorLine);
#endif // !AV_CALC_TRANSFORMS
#ifdef AV_CALC_TRANSFORMS
	mNeckDirection = -mNormal;
	mCenterPosition = mVtogTrans;
	mVtogRot.set(X,X,mrhs[1][1]); // Transpose of mrhs
	mVtogRot.set(X,Y,mrhs[2][1]);
	mVtogRot.set(X,Z,mrhs[3][1]);

	mVtogRot.set(Y,X,mrhs[1][2]);
	mVtogRot.set(Y,Y,mrhs[2][2]);
	mVtogRot.set(Y,Z,mrhs[3][2]);

	mVtogRot.set(Z,X,mrhs[1][3]);
	mVtogRot.set(Z,Y,mrhs[2][3]);
	mVtogRot.set(Z,Z,mrhs[3][3]);

	mGtovRot.set(X,X,mrhs[1][1]); // Transpose of vtogRot
	mGtovRot.set(X,Y,mrhs[1][2]); // (therefore = to mrhs)
	mGtovRot.set(X,Z,mrhs[1][3]);

	mGtovRot.set(Y,X,mrhs[2][1]);
	mGtovRot.set(Y,Y,mrhs[2][2]);
	mGtovRot.set(Y,Z,mrhs[2][3]);

	mGtovRot.set(Z,X,mrhs[3][1]);
	mGtovRot.set(Z,Y,mrhs[3][2]);
	mGtovRot.set(Z,Z,mrhs[3][3]);
	mNeckRingPosition = vtog(mNeckRingStaticPosition);

	// transform block locations
	// now in reportGoodPoll
	// as is recording of the "old" values
	yawPitchRoll();
#endif

}





void AV::yawPitchRoll(void) {

#ifdef AV_CALC_TRANSFORMS
	float lSinTheta = -mVtogRot.get(X,Z);
	mTheta = (float)asin(lSinTheta);
	float lCosTheta = (float)cos(mTheta);
	float lSinPhi;
	if(lCosTheta != 0) {
		float lSinPsi = (float)mVtogRot.get(Y,Z)/lCosTheta;
		mPsi = (float)asin(lSinPsi);
		lSinPhi = (float)mVtogRot.get(X,Y)/lCosTheta;
		mPhi = (float)asin(lSinPhi);
	}
	else {
		sprintf(display->outString,"The AV class thinks the detector is lying on its side.  Leave for surface.");
		display->messageB(1,kErrorLine);
		mPollOK = 0;
	}

	mTheta *= 180/3.1415926;
	mPhi *= 180/3.1415926;
	mPsi *= 180/3.1415926;
#endif

}

ThreeVector AV::vtog(ThreeVector x)  const// transforms x from vessel to global co-ordinates
{
	// vtogRot is an object of RotationMatrix
	// vtogTrans is the translation
	return mVtogTrans + mVtogRot * x;

}

ThreeVector AV::gtov(ThreeVector u)  const// transforms x from global to vessel co-ordinates
{
	// use -vtogTrans instead of +gtovTrans
	// this is correct
	// u = T + Rx above routine
	// u-T = Rx
	// R^-1(u-T) = x
	return mGtovRot * (u-mVtogTrans);

}

void AV::mrqFunc(float x, float* a, float* ymod, float* dyda, int ma) {
	int lRope = (int)floor(x+0.1);
	float xx,yy,zz; // mNeckAttach[j][X] + deltax
	if(lRope <= 3) {
		xx = mTopBlockVessel[0][X] + a[1] + mDelta[lRope-1][X];
		yy = mTopBlockVessel[0][Y] + a[2] + mDelta[lRope-1][Y];
		zz = mTopBlockVessel[0][Z] + a[3] + mDelta[lRope-1][Z];
	}
	if(lRope==4 || lRope==5) {
		xx = mTopBlockVessel[1][X] + a[1] + mDelta[lRope-1][X];
		yy = mTopBlockVessel[1][Y] + a[2] + mDelta[lRope-1][Y];
		zz = mTopBlockVessel[1][Z] + a[3] + mDelta[lRope-1][Z];
	}
	if(lRope==6 || lRope==7) {
		xx = mTopBlockVessel[2][X] + a[1] + mDelta[lRope-1][X];
		yy = mTopBlockVessel[2][Y] + a[2] + mDelta[lRope-1][Y];
		zz = mTopBlockVessel[2][Z] + a[3] + mDelta[lRope-1][Z];
	}

	float lBigX,lBigY,lBigZ;
	lBigX = mrqA11*xx + mrqA12*yy + mrqA13*zz;
	lBigY = mrqA21*xx + mrqA22*yy + mrqA23*zz;
	lBigZ = mrqA31*xx + mrqA32*yy + mrqA33*zz;



	float lLx,lLy,lLz,lLength;
	lLx = mSnout[lRope-1][X] - lBigX;
	lLy = mSnout[lRope-1][Y] - lBigY;
	lLz = mSnout[lRope-1][Z] - lBigZ;

	lLength = sqrt(lLx*lLx + lLy*lLy + lLz*lLz);
	(*ymod) = lLength;
	float lDerivPrefix = -1.0/lLength;

	// the derivs for x,y, and z are easy. Simply the rot matrix
	// times some length terms

	dyda[1] = lLx*mrqA11 + lLy*mrqA21 + lLz*mrqA31;
	dyda[2] = lLx*mrqA12 + lLy*mrqA22 + lLz*mrqA32;
	dyda[3] = lLx*mrqA13 + lLy*mrqA23 + lLz*mrqA33;


	// the derivs for the angles are harder
	float dxdphi =  -mrqCthe*mrqSthe*xx + mrqCthe*mrqCphi*yy;
	float dydphi = (-mrqSpsi*mrqSthe*mrqSphi - mrqCpsi*mrqCphi)*xx +
		( mrqSpsi*mrqSthe*mrqCphi - mrqCpsi*mrqSphi)*yy;
	float dzdphi = (-mrqCpsi*mrqSthe*mrqSphi + mrqSpsi*mrqCphi)*xx +
		( mrqCpsi*mrqSthe*mrqCphi + mrqSpsi*mrqSphi)*yy;
	float dxdthe = -mrqSthe*mrqCphi*xx - mrqSthe*mrqSphi*yy-mrqCthe*zz;
	float dydthe = ( mrqSpsi*mrqSthe*mrqCphi - mrqCpsi*mrqSphi)*xx +
		(-mrqSpsi*mrqCthe*mrqSphi + mrqCpsi*mrqCphi)*yy;
	float dzdthe = ( mrqCpsi*mrqCthe*mrqCphi + mrqSpsi*mrqSphi)*xx +
		( mrqCpsi*mrqCthe*mrqSphi - mrqSpsi*mrqCphi)*yy
		-mrqSthe*mrqSpsi*zz;
	float dxdpsi = 0.0;
	float dydpsi = ( mrqCpsi*mrqSthe*mrqCphi + mrqSpsi*mrqSphi)*xx +
		( mrqCpsi*mrqSthe*mrqSphi - mrqSpsi*mrqCphi)*yy
		+ mrqCthe*mrqCpsi*zz;
	float dzdpsi = (-mrqSpsi*mrqSthe*mrqCphi + mrqCpsi*mrqSphi)*xx +
		(-mrqSpsi*mrqSthe*mrqSphi - mrqCpsi*mrqCphi)*yy
		-mrqCthe*mrqSpsi*zz;

	dyda[4] = lLx*dxdphi + lLy*dydphi + lLz*dzdphi;
	dyda[5] = lLx*dxdthe + lLy*dydthe + lLz*dzdthe;
	dyda[6] = lLx*dxdpsi + lLy*dydpsi + lLz*dzdpsi;

	for(int i=1 ; i<=6 ; i++) {
		dyda[i] *= lDerivPrefix;
	}
}

#define NRANSI
void AV::mrqcof(float x[], float y[], float sig[], int ndata, float a[],
		int ia[],       int ma, float **alpha, float beta[], float *chisq)

{
	int i,j,k,l,m,mfit=0;
	float ymod,wt,sig2i,dy,*dyda;

	dyda=vector(1,ma);
	for (j=1;j<=ma;j++)
		if (ia[j]) mfit++;
	for (j=1;j<=mfit;j++) {
		for (k=1;k<=j;k++) alpha[j][k]=0.0;
		beta[j]=0.0;
	}
	*chisq=0.0;

	// My hack. I precalculate some thins for the function call to
	// save redoing them in the loop.
	mrqCphi = cos(a[4]);
	mrqSphi = sin(a[4]);
	mrqCthe = cos(a[5]);
	mrqSthe = sin(a[5]);
	mrqCpsi = cos(a[6]);
	mrqSpsi = sin(a[6]);


	mrqA11 = mrqCthe*mrqCphi;
	mrqA12 = mrqCthe*mrqSphi;
	mrqA13 = -mrqSthe;

	mrqA21 = mrqSpsi*mrqSthe*mrqCphi - mrqCpsi*mrqSphi;
	mrqA22 = mrqSpsi*mrqSthe*mrqSphi + mrqCpsi*mrqCphi;
	mrqA23 = mrqCthe*mrqSpsi;

	mrqA31 = mrqCpsi*mrqSthe*mrqCphi + mrqSpsi*mrqSphi;
	mrqA32 = mrqCpsi*mrqSthe*mrqSphi - mrqSpsi*mrqCphi;
	mrqA33 = mrqCthe*mrqCpsi;

	for (i=1;i<=ndata;i++) {
		mrqFunc(x[i],a,&ymod,dyda,ma);
		sig2i=1.0/(sig[i]*sig[i]);
		dy=y[i]-ymod;
		for (j=0,l=1;l<=ma;l++) {
			if (ia[l]) {
	wt=dyda[l]*sig2i;
	for (j++,k=0,m=1;m<=l;m++)
		if (ia[m]) alpha[j][++k] += wt*dyda[m];
	beta[j] += dy*wt;
			}
		}
		*chisq += dy*dy*sig2i;
	}
	for (j=2;j<=mfit;j++)
		for (k=1;k<j;k++) alpha[k][j]=alpha[j][k];
	free_vector(dyda,1,ma);

}

void AV::mrqmin(float x[], float y[], float sig[], int ndata, float a[],
	int ia[],int ma, float **covar, float **alpha, float *chisq,
	float *alamda)
{
	int j,k,l,m;
	static int mfit;
	static float ochisq,*atry,*beta,*da,**oneda;

	if (*alamda < 0.0) {
		atry=vector(1,ma);
		beta=vector(1,ma);
		da=vector(1,ma);
		for (mfit=0,j=1;j<=ma;j++)
			if (ia[j]) mfit++;
		oneda=matrix(1,mfit,1,1);
		*alamda=0.001;
		mrqcof(x,y,sig,ndata,a,ia,ma,alpha,beta,chisq);
		ochisq=(*chisq);
		for (j=1;j<=ma;j++) atry[j]=a[j];
	}

	for (j=0,l=1;l<=ma;l++) {
		if (ia[l]) {
			for (j++,k=0,m=1;m<=ma;m++) {
	if (ia[m]) {
		k++;
		covar[j][k]=alpha[j][k];
	}
			}
			covar[j][j]=alpha[j][j]*(1.0+(*alamda));
			oneda[j][1]=beta[j];
		}
	}
	gaussj(covar,mfit,oneda,1);
	for (j=1;j<=mfit;j++) da[j]=oneda[j][1];
	if (*alamda == 0.0) {
		covsrt(covar,ma,ia,mfit);
		free_matrix(oneda,1,mfit,1,1);
		free_vector(da,1,ma);
		free_vector(beta,1,ma);
		free_vector(atry,1,ma);
		return;
	}
	for (j=0,l=1;l<=ma;l++)
		if (ia[l]) atry[l]=a[l]+da[++j];
	mrqcof(x,y,sig,ndata,atry,ia,ma,covar,da,chisq);
	if (*chisq < ochisq) {
		*alamda *= 0.1;
		ochisq=(*chisq);
		for (j=0,l=1;l<=ma;l++) {
			if (ia[l]) {
	for (j++,k=0,m=1;m<=ma;m++) {
		if (ia[m]) {
			k++;
			alpha[j][k]=covar[j][k];
		}
	}
	beta[j]=da[j];
	a[l]=atry[l];
			}
		}
	} else {
		*alamda *= 10.0;
		*chisq=ochisq;
	}
}

#undef NRANSI

void AV::mrqfit(void) {
	mrqfit(0);
}


void AV::mrqfit(int silent) {
	float*        lmrqX;
	float*        lmrqY;
	float*        lmrqSig;      // These variables are described in NR in C
	int     lmrqNData = 7;
	float*        lmrqA;
	int*    lmrqIA;
	float**       lmrqCovar;
	float** lmrqAlpha;
	float         lmrqChiSquare;
	float         lmrqOldChiSquare;
	float         lmrqAlamda;
	int                   lmrqMA = 6;
	lmrqX = vector(1,7);
	lmrqY = vector(1,7);
	lmrqSig = vector(1,7);
	lmrqA = vector(1,6);
	lmrqIA = ivector(1,6);
	lmrqCovar = matrix(1,6,1,6);
	lmrqAlpha = matrix(1,6,1,6);
	int i;
	pollGetData();  // want a common function to read data from sropes
	for(i=0;i<7;i++) {
		lmrqX[i+1] = 1.0*(i+1);
		lmrqY[i+1] = mLength[i];
		lmrqSig[i+1] = 0.2;
	}
	for( i=0 ; i<6 ; i++ ) {
		lmrqIA[i+1] = 1;
		lmrqA[i+1] = 0.0;
	}
//	lmrqIA[4] = lmrqIA[5] = lmrqIA[6] = 0;


	lmrqAlamda = -1.0;

	mrqmin(lmrqX,lmrqY,lmrqSig,lmrqNData,lmrqA,lmrqIA,lmrqMA,
	 lmrqCovar,lmrqAlpha,&lmrqChiSquare,&lmrqAlamda);
	int lChiCount = 0;
	for(i=0;i<99;i++) {
		lmrqOldChiSquare = lmrqChiSquare;
		mrqmin(lmrqX,lmrqY,lmrqSig,lmrqNData,lmrqA,lmrqIA,lmrqMA,
		 lmrqCovar,lmrqAlpha,&lmrqChiSquare,&lmrqAlamda);
		if(lmrqChiSquare>=(lmrqOldChiSquare-0.001)) lChiCount++;
		if(lmrqChiSquare<(lmrqOldChiSquare-0.001)) lChiCount = 0;
		if(lChiCount==3) break;
	}
	if(i>97)  {  // Fit failed badly.
		sprintf(display->outString,"AV mrqfit failed. Too many iterations.");
		display->messageB(1,23);
		if(mPollTechnique==3) mPollOK = 0;
	}

	else {

		lmrqAlamda = 0;
		mrqmin(lmrqX,lmrqY,lmrqSig,lmrqNData,lmrqA,lmrqIA,lmrqMA,
		 lmrqCovar,lmrqAlpha,&lmrqChiSquare,&lmrqAlamda);
		mrqX = lmrqA[1];
		mrqY = lmrqA[2];
		mrqZ = lmrqA[3];
		mChiSquare=lmrqChiSquare;
		if(lmrqChiSquare>mChiSquareLimit) mPollOK = 0;
		else {if( mPollTechnique==3 ) mPollOK = 1;}
		if( !silent ) {
			sprintf(display->outString,"Results from AV fit.");
			display->messageB(1,15);
			sprintf(display->outString,
				"Chi Sq = %5.2f. x = %6.3f. y = %6.3f. z = %6.3f.",
				lmrqChiSquare,lmrqA[1],lmrqA[2],lmrqA[3]);
			display->messageB(1,16);
			sprintf(display->outString,"Yaw = %f +/- %f. Pitch = %f +/- %f. Roll = %f +/- %f.",
				lmrqA[4],sqrt(lmrqCovar[4][4]),lmrqA[5],sqrt(lmrqCovar[5][5]),lmrqA[6],sqrt(lmrqCovar[6][6]));
			display->messageB(1,17);
			for(int j = 0;j<3;j++) {
				float x = mrqA11*(mTopBlockVessel[j][X]+lmrqA[1]) +
									mrqA12*(mTopBlockVessel[j][Y]+lmrqA[2]) +
									mrqA13*(mTopBlockVessel[j][Z]+lmrqA[3]);
				float y = mrqA21*(mTopBlockVessel[j][X]+lmrqA[1]) +
									mrqA22*(mTopBlockVessel[j][Y]+lmrqA[2]) +
									mrqA23*(mTopBlockVessel[j][Z]+lmrqA[3]);
				float z = mrqA31*(mTopBlockVessel[j][X]+lmrqA[1]) +
									mrqA32*(mTopBlockVessel[j][Y]+lmrqA[2]) +
									mrqA33*(mTopBlockVessel[j][Z]+lmrqA[3]);
				sprintf(display->outString,"Block 1: %f\t%f\t%f",x,y,z);
				display->messageB(1,18+j);

			}
		}
	}
	free_vector(lmrqX,1,7);
	free_vector(lmrqY,1,7);
	free_vector(lmrqSig,1,7);
	free_vector(lmrqA,1,6);
	free_ivector(lmrqIA,1,7);
	free_matrix(lmrqCovar,1,6,1,6);
	free_matrix(lmrqAlpha,1,6,1,6);
}

void AV::poll3(void)
{
	int i;
	// pollGetData is called from within mrqfit
	mrqfit(1);
	if( mPollOK ) {
		mVtogRot.set(X,X,mrqA11);
		mVtogRot.set(X,Y,mrqA12);
		mVtogRot.set(X,Z,mrqA13);
		mVtogRot.set(Y,X,mrqA21);
		mVtogRot.set(Y,Y,mrqA22);
		mVtogRot.set(Y,Z,mrqA23);
		mVtogRot.set(Z,X,mrqA31);
		mVtogRot.set(Z,Y,mrqA32);
		mVtogRot.set(Z,Z,mrqA33);

		mGtovRot.set(X,X,mrqA11);
		mGtovRot.set(Y,X,mrqA12);
		mGtovRot.set(Z,X,mrqA13);
		mGtovRot.set(X,Y,mrqA21);
		mGtovRot.set(Y,Y,mrqA22);
		mGtovRot.set(Z,Y,mrqA23);
		mGtovRot.set(X,Z,mrqA31);
		mGtovRot.set(Y,Z,mrqA32);
		mGtovRot.set(Z,Z,mrqA33);

		// mrqfit defiined in terms of R(x+T'). want to go to convention
		// of T + R(x). Then R(T') = T

		ThreeVector temp(mrqX,mrqY,mrqZ);

		mVtogTrans = mVtogRot*temp;


		mTheta = 180*acos(mrqCthe)/M_PI;
		mPhi = 180*acos(mrqCphi)/M_PI;
		mPsi = 180*acos(mrqCpsi)/M_PI;

		for( i=0 ; i<3 ; i++ ) mNeckAttachGlobal[i] = vtog(mTopBlockVessel[i]);
	}
	mJustFinishedPoll=1;

}

void AV::poll2(void) {
	if( !mPoll2Initialized ) poll2Init();
	pollGetData();
	int i,j;
	float ldx[6];

	for( i=0 ; i<6 ; i++ ) {
		ldx[i] = 0.0;
		for( j=0 ; j<6 ; j++ ) {
			ldx[i] += mPoll2Mat[i+1][j+1] * mDeltaL[mSkip[j]];
		}
	}
	mPoll2Rot.ypr(0.01*ldx[3],0.01*ldx[4],0.01*ldx[5]);
	ThreeVector center(ldx[0],ldx[1],ldx[2]);
	ThreeVector lNA[3];
	ThreeVector lNAg[3];
	for( i=0 ;  i<3 ; i++ ) {
		lNA[i] = mNeckAttach[i];
		lNA[i][Z] -= mHeightOfNeck;
		lNAg[i] = center+mPoll2Rot*lNA[i];
		mTotalDeltaX = lNAg[i] - lNA[i];
		mNeckAttachGlobal[i] = mNeckAttach[i] + mTotalDeltaX;
	}
	mPollOK = 1;
	pollCalcTranslation();
	if(mPollOK)pollCalcRotation();
	if(mPollOK)pollPutTogether();
	if(mPollOK)yawPitchRoll();
	if(mPollOK) {
		int lcheckRope = mSkipped-1;
		//ThreeVector lcheckna = mNeckAttach[mWhichNA[lcheckRope]];
		ThreeVector lcheckna = mNeckAttachGlobal[mWhichNA[lcheckRope]];
		float checkln = (lcheckna-mSnout[lcheckRope]).norm();
		mPoll2Resid = fabs(checkln-mLength[lcheckRope]);
		if(mPoll2Resid>mRopeDiffLimit) mPollOK = 0;
	}
	mJustFinishedPoll = 1;
}

void AV::setSkip(char* aString) {
	int skipped = atoi(aString); // 1 to 7
	int i;
	if(skipped<=0 || skipped>7) {
		sprintf(display->outString,"Bad rope chosen (%d) to skip in polling. Skipping rope 1",skipped);
		display->messageB(1,22);
		skipped = 1;
	}
	mSkipped = skipped;
	skipped -=1;  // 0 to 6
	int j=0;
	for(i=0 ; i<7; i++ ) {
		if(i!=skipped) {
			mSkip[j] = i;
			j++;
		}
	}
	mPoll2Initialized = 0;

}

void AV::poll2Init() {
	mPoll2Rot.setUnit();
	float offLength[6];
	ThreeVector lCenter(0.0);
	ThreeVector lCenter1(0.0);
	ThreeVector naOffset[3];
	ThreeVector lNAcenter[3];
	ThreeVector lSnoutCenter[7];
	int i,j,k;
	for(i=0 ; i<3 ; i++) {
		lNAcenter[i] = mNeckAttach[i];
		lNAcenter[i][Z] -= mHeightOfNeck;
	}
	for(i=0 ; i<7 ; i++) {
		lSnoutCenter[i] = mSnout[i];
		lSnoutCenter[i][Z] -= mHeightOfNeck;
	}

	if( mPoll2Mat==NULL ) mPoll2Mat = matrix(1,6,1,6);
	for( i=0 ; i<6 ; i++ ) {
		lCenter1 = lCenter;
		if(i==0) lCenter1 = lCenter + iVector;
		if(i==1) lCenter1 = lCenter + jVector;
		if(i==2) lCenter1 = lCenter + kVector;
		if(i==3) mPoll2Rot.ypr(.01,0,0);
		if(i==4) mPoll2Rot.ypr(0,.01,0);
		if(i==5) mPoll2Rot.ypr(0,0,.01);
		for( j=0 ; j<3 ; j++ ) {
			naOffset[j] = lCenter1+mPoll2Rot*lNAcenter[j];
		}
		for( k=0 ; k<6 ; k++ ) {
			offLength[k] = (lSnoutCenter[mSkip[k]] - naOffset[mWhichNA[mSkip[k]]]).norm();
			mPoll2Mat[k+1][i+1] = offLength[k]-mRestLength[mSkip[k]];
		}
	}
	float** dummy = matrix(1,6,1,1);
	dummy[1][1] = 1.0;
	for(i=2;i<=6;i++) dummy[i][1] = 0.0;
	gaussj(mPoll2Mat,6,dummy,1); // inverts matrix
	mPoll2Initialized = 1;


}

void AV::updateDatabase()
{
	double curTime = RTC->time();

	if (curTime >= mLastUpdateTime + kMinUpdateTime) {
		mLastUpdateTime = curTime;
		char buff[128];		// temporary string buffer

		OutputFile out("AV",getObjectName(),1.00);	// create output object
		sprintf(buff,"%.8g %.8g %.8g",
						mLastCenter[0], mLastCenter[1], mLastCenter[2]);
		out.updateLine("CENTER:", buff);
		sprintf(buff,"%.8g %.8g %.8g",
						mLastPhi, mLastTheta, mLastPsi);
		out.updateLine("YPR:", buff);
		for(int i=0 ; i<7 ; i++ ) {
			char name[32];
			sprintf(name,"ROPE%d_LENGTH:",i);
			sprintf(buff,"%g", mLastLength[i]);
			out.updateLine(name, buff);
		}
		mUpdateFlag = 0;	// reset update flag
	} else {
		mUpdateFlag = 1;	// set flag to update database later
	}
}

