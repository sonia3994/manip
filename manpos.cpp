/*
	This class calculates lengths and tensions for various manipulator
positions.
	Required Information:
		positions of anchor blocks, manipulator ropes, central rope,
			and neck ring height, radius and radius of curvature
			length of manipulator carriage connector
			radius of manipulator pulleys

			For some calcuations, the tensions in (some) ropes are also
			required; all tensions are given in units of the source
			weight.

	 1.  Get Lengths(position, angle) gets lengths of ropes
	 2.  Get Tensions (position, angle, Tension3)
	 3.  Get Position (length1, length2, length3, tension3);

	 All calculations are done in terms of global coordinates.

In order to allow changes to be made to the fixed positions, one should
derive a class from ManPos3 which will change the protected
variables.  I believe that this is somewhat more efficient and
equally flexible to the other alternative-
having the position finding routines call a virtual function. */

//
// Notes:
//
// - left and right pulley offsets must be the same, and are
//		assumed to be in line with the center of the carriage
//

#include "threevec.h"
#include "av.h"
#include "manpos.h"
#include "ieee_num.h"
#include "umbil.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

//#define DEBUG_CALC_ERRS

const int		kMaxIterations = 20;					// max. no. iterations before reporting error
const int		kMaxUndampedInterations = 10;	// max. no. iterations without damping
const float	kDampingFactor = 0.25;				// damping factor for position solution
const float	kConvergenceDistance = 0.03;	// maximum distance between iterations for convergence
const int		kResetCount	= 250;	// recalculate from center of vessel
																// every 250 polls (about once per sec)

const float	kSideRopeNeckTens	= 10;
const float	kMinCalcTens			= 2;	// minimum calculated tension

// silly version 3.1 compiler - PH
static ThreeVector	t1(0,0,0);
static ThreeVector	t2(0,0,0);
static ThreeVector	t3 = cross(t1,t2);


//------------------------------------------------------------------
// ManPos methods
//
ManPos::ManPos(int numAxes, SourceWeight *weight, const AV &aav):
	av(aav),mWeight(weight),mNumAxes(numAxes)
{
	mUmbilical = NULL;
}


// calculate net force based on measured tensions
ThreeVector ManPos::calcNetForce(const float tensions[])
{
	ThreeVector	netForce(0,0,0);
	for (int i=0; i<mNumAxes; ++i) {
		netForce += getRopeVector(i) * tensions[i];
	}
	netForce += calcUmbilicalForce();

	netForce[Z] -= mWeight->getSourceWeight(mPosition);	// compensate for source weight

	return(netForce);
}


// calculate force on carriage due to umbilical at current position
ThreeVector ManPos::calcUmbilicalForce()
{
	ThreeVector	force;

	if (mUmbilical) {
		// calculate umbilical tension
		ThreeVector umbilDir = mUmbilical->getXtop() - mPosition;
		umbilDir[Z] = 0;
		umbilDir.normalize();
		force = kVector.operator*(mUmbilical->calcVertTension(mPosition))
				 + umbilDir.operator*(mUmbilical->calcHorizTension(mPosition));
	} else {
		force = oVector;
	}
	return(force);
}



//------------------------------------------------------------------
// ManPos3 methods - three rope manipulator
//

ManPos3::ManPos3(ManipulatorRope *list[], int aleft, int aright,
	int acenter, SourceWeight *weight, const AV &aav)
	: ManPos(3,weight,aav)
{
	int i;

	for (i=0; i<3;i++) ropeList[i]=list[i];

	ileft = aleft; iright = aright; icenter = acenter;
	leftRope = list[ileft];
	rightRope = list[iright];
	centralRope = list[icenter];
	pulleyOffset = rightRope->getPulleyOffset();	// use right rope offset (left better be the same!)
	nPlane = rightRope->getAnchorExit()-leftRope->getAnchorExit();
	nPlane.normalize();
	rightPulleyOffset = pulleyOffset * nPlane;
			// neck extends to 10 cm below ring
	l3NeckCut = norm(centralRope->getBushing()-av.getNeckRingPosition())+10;
	nPerp = cross(kVector,nPlane);
	lmin = leftRope->getMinimumLength();
	rmin = rightRope->getMinimumLength();
	calcLengths(lmax,rmax,cmax,ThreeVector(0,0,-600));
	lmax += 10; rmax += 10; cmax += 10;

	mPosition = oVector;
	resetCount = 0;		// force full calculation the first iteration
}

/* validLengths returns true if l1,l2,l3 make sense.  It checks that the
lengths are within the allowed range, that the position is inside the vessel,
and that the three agree within 5 cm.
*/
int ManPos3::validLengths(float l1, float l2, float l3)
{
	int i = (lmin<=l1 && l1<=lmax && rmin<=l2 && l2<=rmax && cmax>=l3 && l3>0);
	if (i){
		if (calcPosition(l1,l2,l3,0.5)) return(0);	// get a reasonable position
		ThreeVector pos=mPosition;
		i=validPosition(pos);								// is position "inside vessel"
		if(fabs(mPositionError)>5) i=0;					// within 5 cm of a good position
	}
	return (i);
}

/* valid Position checks to see of pos is reasonable.  It is true for a position
within the neck, within the vessel, greater than 5 cm from the anchor points. It
considers positions above the "taut rope" to be valid */

int ManPos3::validPosition(const ThreeVector &pos)
{
	int iret=1;
	 if(norm(pos)>av.getVesselRadius()) iret=0;
   else if(norm(pos-leftRope->getAnchorExit()) < 5) iret=0;
   else if(norm(pos-rightRope->getAnchorExit())<5) iret=0;
	 return iret;
}



// calculates rope lengths, tensions and vectors
// from position and measured tensions
int ManPos3::calcState(const ThreeVector &pos,  const float tensions[])
{
	rightPulleyOffset = pulleyOffset * nPlane;	// re-initialize this

	mPosition = pos;// THIS IS NECESSARY! Lengths and position must agree - PH
	calcLengths(mLengths[ileft],mLengths[iright],
							mLengths[icenter],pos);

	return(calcPosition(mLengths[ileft],mLengths[iright],
							 mLengths[icenter],tensions[icenter]));
}

// calculates position, error and tensions from lengths and measured tensions
int ManPos3::calcState(const float lengths[], const float tensions[])
{
	return(calcPosition(lengths[ileft],lengths[iright],
										lengths[icenter],tensions[icenter]));
}

// calculates source velocity at current position with given rope speeds
// if flags are non-zero, neck motion is used. otherwise vessel motion
ThreeVector ManPos3::calcVelocity(const float ropeSpeeds[], const ThreeVector &pos,
																	int flags)
{
	ThreeVector temp(mPosition-pos);
	if(temp*temp>10){
		float ltemp[3];
		calcLengths(ltemp[0],ltemp[1],ltemp[2],pos);
		s1 = leftRope->getRopeSum();
		s2 = rightRope->getRopeSum();
		s3 = centralRope->getRopeSum();
		s12 = s1*s2;
		s11 = s1*s1;
		s22 = s2*s2;
		s23 = s2*s3;
		s13 = s1*s3;
		b = 1.0/(s11*s22-s12*s12);
	}
//	if (mLengths[icenter] < l3NeckCut) {
	if (flags) {	// PH 11/20/97
		// source in neck
		float b2 = 1.0/(s11-s13*s13);
		temp=b2*((s13*ropeSpeeds[icenter] - ropeSpeeds[ileft])*s1 +
						 (s13*ropeSpeeds[ileft]-s11*ropeSpeeds[icenter])*s3);
	} else {
		temp=b*((s12*ropeSpeeds[iright]-s22*ropeSpeeds[ileft])*s1 +
						(s12*ropeSpeeds[ileft] -s11*ropeSpeeds[iright])*s2);
	}
	return temp;
}

// calculates rope speeds at current position with given source velocity
int ManPos3::calcRopeSpeeds(float ropeSpeeds[], const ThreeVector &vel, const ThreeVector &position)
{
	ThreeVector temp(mPosition-position);
	if(temp*temp>10){
		float ltemp[3];
		calcLengths(ltemp[0],ltemp[1],ltemp[2],position);
	}
	for (int i=0; i<3; ++i) {
		ropeSpeeds[i] = vel*getRopeVector(i);
	}

	return(0);	// no errors
}

// calculates source position, error and rope lengths based on measured tensions
int ManPos3::calcState(const float tensions[])
{
	ThreeVector pos(0,0,0);
	ThreeVector oldPos;
	int		iter = 0;
	float c1,c2,c3,c4,c5;
	float tmp;
	do {
		if (++iter > 100) return(-1);
		oldPos=pos;
		c1=tensions[ileft]/norm(leftRope->getUpperEnd()-mPosition);
		c2=tensions[ileft]/norm(leftRope->getLowerEnd()-mPosition);
		c3=tensions[iright]/norm(rightRope->getUpperEnd()-mPosition);
		c4=tensions[iright]/norm(rightRope->getLowerEnd()-mPosition);
		c5=tensions[icenter]/norm(centralRope->getUpperEnd()-mPosition);
		pos=((c1*leftRope->getUpperEnd()
				 +c2*leftRope->getLowerEnd()
				 +c3*rightRope->getUpperEnd()
				 +c4*rightRope->getLowerEnd()
				 +c5*centralRope->getUpperEnd()
				 +calcUmbilicalForce()
				 -mWeight->getSourceWeight(mPosition)*kVector)/(c1+c2+c3+c4+c5));
		mPosition = pos;// THIS IS NECESSARY! Lengths and position must agree - PH
		calcLengths(mLengths[ileft], mLengths[iright],mLengths[icenter],pos);
		if (calcPosition(mLengths[ileft],mLengths[iright],mLengths[icenter],tensions[icenter])) {
			return(1);
		}
		tmp = norm(pos-oldPos);
	} while (tmp > 0.05);

	return(0);
}

// Finds the lengths of ropes.  rightPulleyOffset is an input, which tells the
// the orientation of the pulleys with respect to the center.
void ManPos3::calcLengths(float &l1, float &l2, float &l3, const ThreeVector &position)
{
	 ThreeVector leftPulleyAxle(position-rightPulleyOffset);
	 ThreeVector rightPulleyAxle(position+rightPulleyOffset);

	 l1 = leftRope->getLength(leftPulleyAxle);
	 l2 = rightRope->getLength(rightPulleyAxle);
	 l3 = centralRope->getLength(position);
}

void ManPos3::constrainPosition(ThreeVector &pos)
{
	if (fabs(leftRope->getAnchorExit()[X]) >
			fabs(leftRope->getAnchorExit()[Y])) {
		pos[Y] = 0;	// constrain the Y axis
	} else {
		pos[X] = 0;
	}
}

int ManPos3::calcPosition(float l1,	float l2, float l3, float t3)
{
	/* The algorithm will work by starting at the origin, and calculate rope
	lengths, and directions of motion when the ropes are moved.  We adjust
	the position until the actual rope lengths correspond to the desired rope
	lenghts */
	int 				i;
	ThreeVector offset;
	ThreeVector	n1, n2, n3;
	float				dl1,dl2,dl3;		// rope length differences
	ThreeVector dx, dxTension;
	ThreeVector	position1;
	float 			b2, len;
	enum ControlFlag {
		kLeftRight,
		kLeftCenter,
		kRightCenter
	};
	ControlFlag	controlRopes = (l3 < l3NeckCut) ? kLeftCenter : kLeftRight;

	// re-start iterations from center of vessel if it is time to do so
	if (--resetCount < 0) {
		resetCount = kResetCount;
		rightPulleyOffset = pulleyOffset * nPlane;	// re-initialize this too
		if (controlRopes != kLeftRight) {
			// start at z position given by rope in the center of the neck
			mPosition = ThreeVector(0,0,centralRope->getBushing()[Z]-l3);
		} else {
			mPosition = oVector;	// start in center of vessel
		}
		calcLengths(mLengths[ileft],mLengths[iright],
								mLengths[icenter],mPosition);
	}

	for (i=0; ;++i) {

		s1 = leftRope->getRopeSum();
		s2 = rightRope->getRopeSum();
		s3 = centralRope->getRopeSum();
		dl1 = mLengths[ileft]-l1;
		dl2 = mLengths[iright]-l2;
		dl3 = mLengths[icenter]-l3;
		s12 = s1*s2;
		s11 = s1*s1;
		s22 = s2*s2;
		s23 = s2*s3;
		s13 = s1*s3;

		// calculate denominator for side ropes
		b = 1.0 / (s11*s22-s12*s12);

		switch (controlRopes) {
			case kLeftRight:
				// use left and right ropes
				dx = b*(s22*dl1-s12*dl2)*s1 + b*(s11*dl2-s12*dl1)*s2;
				break;
			case kLeftCenter:
				// use left and center ropes in first neck iteration
				// (note: s33 = 1.0)
				b2 = 1.0/(s11-s13*s13);
				dx = b2*(dl1-s13*dl3)*s1 + b2*(s11*dl3-s13*dl1)*s3;
				break;
			case kRightCenter:
				// use right and center ropes in first neck iteration
				b2 = 1.0/(s22-s23*s23);
				dx = b2*(dl2-s23*dl3)*s2 + b2*(s22*dl3-s23*dl2)*s3;
				break;
		}

		len = norm(dx);

		// put this in here to avoid iterating too long at points where
		// the solution oscillates
		if (i>kMaxUndampedInterations) {
			dx *= kDampingFactor;
		}
#ifdef DEBUG_CALC_ERRS
if (i>=12) {
sprintf(display->outString,"%.2f %.2f %.2f (%d %d %d)\n",
mPosition[0],mPosition[1],mPosition[2],
ropeList[0]->hitNeckRing(),ropeList[1]->hitNeckRing(),ropeList[2]->hitNeckRing());
display->message();
}
#endif
		if (isNAN(len) || len>1e4 || i>=kMaxIterations) {
			static unsigned int errorCount=0;
			if (i>=kMaxIterations) {
				sprintf(display->outString,"Calc error: too many iterations. Count %u",++errorCount);
			} else {
				sprintf(display->outString,"Error calculating position. Count %u",++errorCount);
			}
			display->messageB(1,kErrorLine);
			resetCount = 0;	// force full calculation next time
			return(-1);	// return error code
		}
		mPosition +=dx;


		mPosition -= (mPosition*nPerp)*nPerp; // zero out perp. position
//
// Now calculate tensions so we can find the carriage tilt
//
		float weight = mWeight->getSourceWeight(mPosition);
		
		if (controlRopes != kLeftRight) {
			// set side tensions to constant
			mTensions[ileft]  = kSideRopeNeckTens;
			mTensions[iright] = kSideRopeNeckTens;
			mTensions[icenter]= (weight-kSideRopeNeckTens*(s1[Z]*s11+s2[Z]*s22))/s3[Z];
		} else {
			mTensions[ileft]  = (weight*(s22*s1[Z]-s12*s2[Z])-t3*(s13*s22-s12*s23))*b;
			mTensions[iright] = (weight*(s11*s2[Z]-s12*s1[Z])-t3*(s23*s11-s12*s13))*b;
			mTensions[icenter]= t3;
		}

		if (mUmbilical) {
			// correct for umbilical tension
			ThreeVector s4 = calcUmbilicalForce();

			if (controlRopes != kLeftRight) {
				mTensions[icenter]-= s4[Z] / s3[Z];
			} else {
				float s14 = s1 * s4;
				float s24 = s2 * s4;
				mTensions[ileft]  -= (s14*s22-s24*s12) * b;
				mTensions[iright] -= (s24*s11-s14*s12) * b;
			}
		}

		// limit tensions to a minimum value because calculations
		// will mess up for tensions <= 0
		if (mTensions[ileft]<kMinCalcTens) mTensions[ileft] = kMinCalcTens;
		if (mTensions[iright]<kMinCalcTens) mTensions[iright] = kMinCalcTens;
		if (mTensions[icenter]<kMinCalcTens) mTensions[icenter] = kMinCalcTens;

		// calculate the tilted pulley offset
		rightPulleyOffset=mTensions[iright]*s2-mTensions[ileft]*s1;
		rightPulleyOffset.normalize(); rightPulleyOffset*=pulleyOffset;
//
// Now calculate y axis position from tension sum.
// In principle this should be:
//
//	Sum( \vec{(bushings,anchors)_i}*T_i/L_i-\vec{position}*T_i/L_i) = 0
//
// However, we assume that the anchors and side bushings have
// y coordinate 0; thus
//
//	centerAnchor_y*T_2/L_2 = position_y*sum_i(T_i/L_i)
//
		float c1,c2,c3,c4,c5;
		c1=mTensions[ileft]/norm(leftRope->getBushing()-mPosition);
		c2=mTensions[ileft]/norm(leftRope->getAnchorExit()-mPosition);
		c3=mTensions[iright]/norm(rightRope->getBushing()-mPosition);
		c4=mTensions[iright]/norm(rightRope->getAnchorExit()-mPosition);
		c5=mTensions[icenter]/norm(centralRope->getBushing()-mPosition);

		dxTension=((c1*leftRope->getBushing()
							 +c2*leftRope->getAnchorExit()
							 +c3*rightRope->getBushing()
							 +c4*rightRope->getAnchorExit()
							 +c5*centralRope->getBushing()
							 +calcUmbilicalForce()
							 -weight*kVector)/(c1+c2+c3+c4+c5));
		dxTension=(dxTension*nPerp)*nPerp;
		mPosition += dxTension;

		// finally, re-calculate rope lengths at new position
		calcLengths(mLengths[ileft],mLengths[iright],
								mLengths[icenter],mPosition);

		// check for convergence
		if (len <= kConvergenceDistance) {
			switch (controlRopes) {
				case kLeftRight:
					mPositionError = l3 - mLengths[icenter];
          break;
				case kLeftCenter:
					mPositionError = l2 - mLengths[iright];
					break;
				case kRightCenter:
					mPositionError = l1 - mLengths[ileft];
					break;
			}
			return(0);	// success!!!
		}
	}
}

float ManPos3::calcCarriageTilt()	// return carriage tilt angle in radians
{
	if (rightPulleyOffset[Z]) {
		float		d2 = rightPulleyOffset[X]*rightPulleyOffset[X] +
								 rightPulleyOffset[Y]*rightPulleyOffset[Y];
		return(atan(rightPulleyOffset[Z]/sqrt(d2)));
	} else {
		return(0);
	}
}

//------------------------------------------------------------------
// ManPos1 methods - single rope manipulator
//

ManPos1::ManPos1(ManipulatorRope *rope, SourceWeight *weight, const AV &av)
		:ManPos(1,weight,av)
{
	ropeList[0]=rope;
	topBushing = rope->getBushing();
	mPosition = topBushing;  // Z position will be overwritten
	mPositionError = 0;
}

// calculates rope lengths, tensions and vectors from position and measured tensions
#pragma argsused
int ManPos1::calcState(const ThreeVector &pos,const float tensions[])
{
	mPosition[Z] = pos[Z];
	mLengths[0] = topBushing[Z] - pos[Z];
	mTensions[0] = tensions[0];
	return(0);
}

// calculates position, error, tensions and vectors from lengths and measured tensions
#pragma argsused
int ManPos1::calcState(const float lengths[], const float tensions[])
{
	mLengths[0] = lengths[0];
	mPosition[Z] = topBushing[Z] - lengths[0];
	return(0);
}

// calculates source velocity at current position with given rope speeds
#pragma argsused
int ManPos1::calcRopeSpeeds(float ropeSpeeds[], const ThreeVector &vel,
														const ThreeVector&pos)
{
	ropeSpeeds[0] = -vel[Z];
	return(0);
}

// calculates rope speeds at current position with given source velocity
#pragma argsused
ThreeVector ManPos1::calcVelocity(const float ropeSpeeds[], const ThreeVector &pos,
																	int flags)
{
	return(ThreeVector(0,0,-ropeSpeeds[0]));
}

// calculates position, error and rope lengths from measured tensions
#pragma argsused
int ManPos1::calcState(const float tensions[])
{
	return(-1);		// can't do it
}


#ifndef TEST_CODE
//------------------------------------------------------------------
// create a new ManPos object of the appropriate flavour
//
ManPos *createManPos(const AV &av, LList<Axis> &axisList, SourceWeight *weight,
										 Umbilical *um)
{
	LListIterator<Axis> li(axisList);
	int ileft, iright, icenter;
	ManPos	*man;

	switch (axisList.count()) {

		case 1:
			man = new ManPos1(li.obj()->getRope(), weight, av);
			break;

		case 3: {
			// make sure the proper axes are connected
			int	axisCheck = 0;
			int i=0; 	// index into ropeList
			ManipulatorRope *ropeList[3],*rope;
			ThreeVector temp;

			for (li=0; li<LIST_END; ++li) {
				rope = li.obj()->getRope();
				switch (rope->ropeType){
					case ManipulatorRope::centralRope:
						axisCheck |= 0x01;
						icenter = i++;
						ropeList[icenter] = rope;
						break;
					case ManipulatorRope::leftRope:
						axisCheck |= 0x02;
						ileft = i++;
						ropeList[ileft] = rope;
						break;
					case ManipulatorRope::rightRope:
						axisCheck |= 0x04;
						iright = i++;
						ropeList[iright] = rope;
						break;
				}
			}
			if (axisCheck == 0x07) {
				man = new ManPos3(ropeList, ileft, iright, icenter, weight, av);
				break;
			}
		}
			// fall through!

		default:
			// if all else fails, return a do-nothing base class object
			man = new ManPos(0,weight, av);
			break;
	}
	if (man) man->setUmbilical(um);		// set the umbilical

	return(man);
}

#endif