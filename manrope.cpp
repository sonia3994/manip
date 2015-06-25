// calculates rope lengths for manipulator
//
// rope lengths are calculated from the top of the pulley
// or bushing to the source position, or AV-block exit hole
//

#include "manrope.h"

// silly version 3.1 compiler - PH
static ThreeVector	t1(0,0,0);
static ThreeVector	t2(0,0,0);
static ThreeVector	t3 = cross(t1,t2);

static inline double acos_(double x)
{
	// protect against range errors
	if (x >= 1.0) return(0.0);
	if (x <= -1.0) return(M_PI);
	return(acos(x));
}

//------------------------------------------------------------------
// ManipulatorRope methods
//

// Constructor for side Ropes //
ManipulatorRope::ManipulatorRope(const ThreeVector &abushing,
				const ThreeVector &aanchor,
				const float &aROC, const float &aHoleOff, const ThreeVector &aaxis,
				float apulleyOffset, float apulleyRadius, float abushingRadius, const AV &av)
			://	av(aav),
					bushing(abushing), bushingRadius(abushingRadius),
						bushingExit(cross(kVector, aaxis)),
					anchorExit(aanchor), anchorROC(aROC), anchorHoleOffset(aHoleOff),
						pulleyAxis(aaxis),pulleyOffset(apulleyOffset),
						neckRingRadius(av.getNeckRingRadius()),
            neckRingROC(av.getNeckRingROC()),
            neckRingCenter(av.getNeckRingPosition()),
            neckRingAxis(av.getNeckDirection())
{
	ropeType=(anchorExit[X]+anchorExit[Y] < 0)?leftRope:rightRope;
	pulleyRadius=apulleyRadius;
//	bushingRadius=abushingRadius;
//	anchorROC=aROC;
//	anchorHoleOffset=aHoleOff;
//	pulleyAxis=aaxis;
	getLength(ThreeVector(0,0,0));
}

// temporary stuff
float tempROC=1.27;
float tempHoleOffset=2.54;

// Constructor for central Rope
ManipulatorRope::ManipulatorRope(const ThreeVector &abushing,
			  	const ThreeVector &aaxis,float abushingRadius, const AV &av)

			:	//av(aav),
         	bushing(abushing), bushingRadius(abushingRadius),
            bushingExit(cross(kVector, aaxis)),
         	anchorExit(oVector), anchorROC(tempROC),
            anchorHoleOffset(tempHoleOffset),
            pulleyAxis(aaxis),neckRingRadius(av.getNeckRingRadius()),
            neckRingROC(av.getNeckRingROC()),
            neckRingCenter(av.getNeckRingPosition()),
            neckRingAxis(av.getNeckDirection())
{
//	bushingRadius = abushingRadius;
	ropeType = centralRope;
//	pulleyAxis = aaxis;
//	bushingExit = cross(kVector, aaxis);
	getLength(ThreeVector(0,0,0));
}

// ALH --CJJ  This should be moved into AV Class!
ThreeVector ManipulatorRope::getAnchorCenter()
{

	ThreeVector temp=cross(kVector, pulleyAxis);
	return(anchorExit+kVector*anchorROC-anchorHoleOffset*temp);
}


/* pulleyRopes calculates the rope vector running from pulley 1,  at position
	 x1, radius r1 to pulley 2, at x2 and radius r2.  a1 is the rotation axis of the
	 first pulley- it's direction is determined by the right hand rule of the rope wrap.
	 The two pulleys are assumed to be in a plane, although it might not be too difficult
	 to generalize this if it is necessary.  If  the "rope crosses" (ie. the rotation
	 senses of the two pulleys are opposite) use a negative radius for r2.

	 There is little difference between calculating between a point and a pulley or
	 two pulleys; for a "bushing", just set one of the radii to 0.

	 Outputs:
	 float distance: length of rope between two pulleys
	 ropeDirection: unit vector in direction of rope.
	 firstPulleyPoint: (if not NULL)vector to place where rope leaves first pulley
	 secondPulleyPoint:(if not NULL) vector to place where rope leaves second pulley
*/

void ManipulatorRope::pulleyRopes(
			const ThreeVector &x1, const float r1, const ThreeVector &a1,
			const ThreeVector &x2, const float r2,
			float &distance, ThreeVector &ropeDirection,
			ThreeVector *firstPulleyPoint, ThreeVector *secondPulleyPoint)
{
	ThreeVector centerLine (x2-x1);
	float x=norm(centerLine);
	ThreeVector n1temp(centerLine/x);
	ThreeVector n2temp(cross (a1, n1temp));
	float deltaRadius=r2-r1;
   if(x<deltaRadius)x=deltaRadius; // Unphysical- pulleys are closer than possible
   	// This change moves pulleys apart so they just touch
	distance=sqrt(x*x-deltaRadius*deltaRadius);
	float a=distance/x; float b=deltaRadius/x;
	ropeDirection=(a*n1temp+b*n2temp);
	ThreeVector nn(-b*n1temp+a*n2temp);
	if(firstPulleyPoint != NULL) *firstPulleyPoint=x1+r1*nn;
	if(secondPulleyPoint != NULL) *secondPulleyPoint=x2+r2*nn;
}

float ManipulatorRope::getMinimumLength()
{
	int hitNR;
	float length,l2,roc;
	ThreeVector intAnchor,intNR,intBushing,n1t,n2t;

	// the central rope has zero minimum length
	if (ropeType==centralRope) return 0;

	ThreeVector anchorCenter = getAnchorCenter();

	// MUST CHECK FOR INTERSECTION!!! - PH 03/31/97
	// (certain prototype geometries don't intesect)
	pulleyRopes(anchorCenter,anchorROC,pulleyAxis,
							bushing, bushingRadius, length, n1t, &intAnchor, &intBushing);
	if ((hitNR=intersection(intAnchor,intBushing,intNR,roc)) != 0) {
		pulleyRopes(anchorCenter,anchorROC,pulleyAxis,
								intNR,roc,length,n1t);
		pulleyRopes(intNR,roc,pulleyAxis,
								bushing,bushingRadius,l2,n2t);
		length += l2 + neckRingROC*acos_(n2t*n1t);
	}
	if (bushingRadius) {
		// calculate length to top of bushing (pulley)
		length += bushingRadius * acos_((hitNR?n2t:n1t) * bushingExit);
	}
	// add rope between anchor exit and hole
	length += anchorHoleOffset+asin(n1t[Z])*anchorROC;

	return(length);
}


float ManipulatorRope::getLength(const ThreeVector &position)
{
	ThreeVector centerLine,n1temp, n2temp;
	ThreeVector leaveAnchor, sidePulley, topPulley,bottomRing, topRing, leaveBushing;
	float roc, dlength;
	hitUpper=0;
	hitLower=0;

	pulleyAxle=position;  		// save- although this might not be needed!
	switch(ropeType){
		case leftRope:
		case rightRope: {
			ThreeVector anchorCenter = getAnchorCenter();
// Start by calculating length of rope to from anchor block to central pulley.
// zero is at the hole at the bottom of the block.
			centerLine=position-anchorCenter;           // center line between anchor & pulley centers
			int wrapsUp;
			wrapsUp=(centerLine[Z]-pulleyRadius+anchorROC > 0);		// rope wraps up around anchor block
			if(wrapsUp){
				pulleyRopes(anchorCenter,anchorROC,pulleyAxis,
										position, pulleyRadius, length, n1, &leaveAnchor, &sidePulley);
				hitLower=intersection(leaveAnchor, sidePulley,intersect,roc);
				if(hitLower){
					pulleyRopes(anchorCenter,anchorROC, pulleyAxis,
											intersect,0,length, n1, &leaveAnchor);
					pulleyRopes(intersect,0, pulleyAxis,
											position, pulleyRadius,	dlength, n2, &sidePulley);
					length += dlength;
					pulleyRopes(position, pulleyRadius, pulleyAxis,
											bushing, bushingRadius, dlength,	n3);
					length += dlength+pulleyRadius*acos_(n2*n3);
				}
// oops! - PH
//				length +=anchorHoleOffset+(M_PI*0.5-acos_(n1[Z])*anchorROC);
//				length +=anchorHoleOffset+(M_PI*0.5-acos_(n1[Z]))*anchorROC;
// why not do the following? isn't n1[Z] guaranteed to be positive?
// or is acos_() alot faster than asin()?
				length += anchorHoleOffset + asin(n1[Z]) * anchorROC;
			}else{
				pulleyRopes(anchorExit,0,pulleyAxis,
										position, pulleyRadius, length, n1, &leaveAnchor, &sidePulley);
			}


			if(!hitLower){
			// Assume that the rope doesn't hit the rubbing ring and calculate
				float dlength;
				pulleyRopes(position, pulleyRadius, pulleyAxis,
										bushing, bushingRadius, dlength, n2, &topPulley,&leaveBushing);

				// if rope hit rubbing ring, recalculate
				float roc;
				hitUpper = intersection(topPulley, leaveBushing,intersect,roc);
				if (hitUpper){
					pulleyRopes(position, pulleyRadius, pulleyAxis,
											intersect, roc,	dlength, n2, &topPulley);
					length += dlength;
					pulleyRopes(intersect, roc, pulleyAxis,
											bushing, bushingRadius, dlength, n3);
					length +=dlength;
					// add length of rope over neck ring and pulley
					length += neckRingROC*acos_(n2*n3) + pulleyRadius*acos_(n1*n2);
				}else{
					float tmp = n1 * n2;
					length += dlength + pulleyRadius *acos_(tmp);
				}
			}
			if (bushingRadius) {
				// calculate length to top of bushing (pulley)
				length += bushingRadius * acos_((hitUpper ? n3 : n2) * bushingExit);
			}
			ropeSum=(hitLower?(n3-n2):(n2-n1));

			break;  // end of sideRope calculation
        }
		case centralRope:
			float roc,dlength;
			pulleyRopes(position, 0, pulleyAxis,
									bushing, bushingRadius, length, n1,NULL,&leaveBushing);
			hitUpper = intersection(position, leaveBushing,intersect,roc);
			if (hitUpper) {
				pulleyRopes(position, 0, pulleyAxis,
										intersect, roc, length, n1);
				pulleyRopes(intersect, roc, pulleyAxis,
										bushing, bushingRadius, dlength, n2);
				length += dlength + neckRingROC * acos_(n1*n2);
			}
			if (bushingRadius) {
				length += bushingRadius * acos_((hitUpper ? n2 : n1) * bushingExit);
			}
			ropeSum=n1;	// set the rope sum vector
			break;
	}
	return length;
}

const ThreeVector &ManipulatorRope::getLowerEnd()
{
	if (hitLower) return(intersect);
	else return(anchorExit);
}

const ThreeVector &ManipulatorRope::getUpperEnd()
{
	if (hitUpper) return(intersect);
	else return(bushing);
}

/*
** intersection checks to see if the straight line from x1 to x2
** will hit the NeckRing.  If so, it calculates the intersection,
** assuming that there is no friction and returns it as the output
** intersection. The radius of curvature is returned in the variable
** roc; if the sense of the rope around the NeckRing is opposite to
** pulleyAxis, roc is set negative. The order of x1 and x2 is critical
** to determine the sense of the rope wrap.
*/
int ManipulatorRope::intersection(const ThreeVector &x1, const ThreeVector &x2,
				 ThreeVector &intersect, float &roc)const
{
	// make sure we cross the neck ring. Otherwise return zero. - PH
	// (quick and dirty comparison of z components)
	if (x1[Z] < neckRingCenter[Z]) {
		if (x2[Z] < neckRingCenter[Z]) return(0);
	} else {
		if (x2[Z] > neckRingCenter[Z]) return(0);
	}

	ThreeVector n1=x2-x1;
	float l=norm(n1);			// distance from x1 to x2;
	n1 /= l;
	float a=(neckRingCenter-x1)* neckRingAxis/(n1*neckRingAxis);
	intersect=x1+a*n1-neckRingCenter;			// intersection of line with plane of NeckRing
	float r=norm(intersect);
	int result=0;
	if(r > neckRingRadius){
		ThreeVector n2=n1*neckRingAxis;
		intersect *= neckRingRadius/r;
		float theta, oldtheta,R1,R2;
		ThreeVector RR;
			// Iterate to find theta that minimizes distance
		for(theta=0, oldtheta=1; fabs(theta-oldtheta)<0.001;){
				oldtheta=theta;
				R1=norm(x1-intersect); R2=norm(x2-intersect);
			 RR=R2*x1+R1*x2;
      	theta=( (RR*n2)/(RR*n1));
				intersect=neckRingRadius*(n1.operator*(cos(oldtheta))+n2.operator*(sin(oldtheta)));
      }
		// Calculate sense of "wrap" compared to pulleyAxis
			roc=(pulleyAxis* cross(x2-intersect,x1-intersect))>0?neckRingROC:-neckRingROC;
// oops! - PH
//		result=0;
			result=1;
// oops again!  caller expects to have the intersection returned in global coordinates
			intersect += neckRingCenter;	// added - PH
	 };
	 return result;
}
