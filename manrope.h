#ifndef MANROPE
#define MANROPE
#include "threevec.h"
#include "av.h"

//------------------------------------------------------------------

class ManipulatorRope{
public:
	// side rope constructor
	ManipulatorRope(const ThreeVector &abushing, const ThreeVector &aanchor,
			const float &aROC, const float &aHoleOff, const ThreeVector &aaxis,
			float apulleyOffset, float apulleyRadius, float aTopRadius, const AV &aav);

	 // central rope constructor
	ManipulatorRope(const ThreeVector &abushing,
			const ThreeVector &aaxis,	float aTopRadius, const AV &aav);

	void setPulleyRadius(float r)		{ pulleyRadius = r;	}
	int  hitNeckRing()							{ return hitUpper || hitLower; }
	ThreeVector getAnchorCenter();

	 float getLength(const ThreeVector &position );
	 float getMinimumLength();
	 ThreeVector &getn1(){return n1;}
	 ThreeVector &getn2(){return n2;}
	 ThreeVector &getn3(){return n3;}
	 ThreeVector &getRopeSum(){return ropeSum;}

	 enum {leftRope, rightRope, centralRope} ropeType;

	 const float getPulleyOffset() {return pulleyOffset;}
	 const ThreeVector &getBushing(){return bushing;}
	 const ThreeVector &getAnchorExit(){return anchorExit;}
	 const ThreeVector &getLowerEnd();
	 const ThreeVector &getUpperEnd();
private:
	 const ThreeVector bushing;		//bushing point where rope leaves manip. motor
	 const float bushingRadius;      // Radius of curvature of bushing
	 const ThreeVector bushingExit;	// unit vector in direction of bushing (top pulley) exit
						//  hand wrap of rope around pulley
	 const float &neckRingRadius;
	 const float &neckRingROC; // radius of curvature of rope, when it is rubbing.
	 const ThreeVector &neckRingCenter;  //position of center of NeckRing at the height equal
				// the the bottom of the neck Ring + the Ring radius of curvature.
	 const ThreeVector &neckRingAxis;  // unit vector along axis of NeckRing.
	 const ThreeVector &anchorExit;
	 const float anchorROC; 		  // Radius of curvature of anchor;
	 const float anchorHoleOffset; // distance from hole in anchor to ROC
	 ThreeVector pulleyAxle;         // position of pulley axle
	 const ThreeVector pulleyAxis; // unit vector- + direction corresponds "right
	 float pulleyRadius;		// radius of pulley on carriage
	 float pulleyOffset;		// offset of pulley on carriage
//    const AV &av;
	 int hitLower; // 1 if lower rope hit neck ring
	 int hitUpper;	  // 1 if either rope hit neck ring
	 ThreeVector intersect;	// point of intersection (upper or lower rope)
	 float length;
	 ThreeVector n1,n2,n3;  // direction vectors of 3 ropes - anchor block-pulley-neck
			// if hitneck is not = 1; n3 is not defined.
		ThreeVector ropeSum;   // sum of two ropes around the carriage pulley (Not a unit vector!)
	 void pulleyRopes(const ThreeVector &x1, const float r1, const ThreeVector &a1,
										const ThreeVector &x2, const float r2,
										float &distance, ThreeVector &ropeDirection,
										ThreeVector *firstPulleyPoint=NULL,
										ThreeVector *secondPulleyPoint=NULL);
	int intersection(const ThreeVector &x1, const ThreeVector &x2,
				 ThreeVector &intersect, float &roc)const;  // calc. intersection with Neck ring


};

#endif
