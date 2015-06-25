
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

#ifndef __MANPOS_H__
#define __MANPOS_H__

#include "manrope.h"
#include "threevec.h"
#include "av.h"

// interface class to retrieve source weight
class SourceWeight {
public:
	virtual float	getSourceWeight(ThreeVector &pos) = 0;
};

class Umbilical;

// interface class for manipulator position calculations
class ManPos {
public:
	ManPos(int numAxes, SourceWeight *weight, const AV &aav);
//
// The following virtual functions must be overridden
// by derived classes.  These functions should return zero
// if the calculations went OK.  Non-zero indicates an error.
//
	// calculates rope lengths, tensions and vectors from position and measured tensions
	virtual int		calcState(const ThreeVector &pos, const float tensions[]) { return(-1); }
	// calculates position, error, tensions and vectors from lengths and measured tensions
	virtual int		calcState(const float lengths[3], const float tensions[])	{ return(-1); }
	// calculates source velocity at current position with given rope speeds
	virtual ThreeVector calcVelocity(const float ropeSpeeds[3], const ThreeVector &pos,
																	 int flags) { return oVector; }
	// calculates rope speeds at current position with given source velocity
	virtual int		calcRopeSpeeds(float ropeSpeeds[], const ThreeVector &vel, const ThreeVector &pos) { return(-1); }
	// calculates position, error and rope lengths from measured tensions
	virtual int		calcState(const float tensions[])					{ return(-1); }
	// calculates carriage tilt
	virtual float	calcCarriageTilt() { return(0); }

	virtual void	constrainPosition(ThreeVector &pos)	{ pos = oVector;	}

	virtual void	reset() { }

	ThreeVector		calcNetForce(const float tensions[]);
	ThreeVector		calcUmbilicalForce();
	

	int						getNumAxes() const					{ return mNumAxes;				}
	const ThreeVector &	getPosition() const		{ return mPosition;				}
	ThreeVector 	getRopeVector(int n) const 	{ return ropeList[n]->getRopeSum();	}
	float	 				getLength(int n) const 	 		{ return mLengths[n];			}
	float					getTension(int n) const  		{ return mTensions[n];		}
	float					getPositionError() const 		{ return mPositionError;	}

	void					setUmbilical(Umbilical *um)	{ mUmbilical = um;				}

protected:
	int						mNumAxes;				// number of axes
	SourceWeight*	mWeight;				// source weight object
	const AV			&av;						// vessel Radius
	ThreeVector		mPosition;
	float					mLengths[3];    // calculated lengths
	float 				mTensions[3];		// calculated tensions
	float					mPositionError;
	class 				ManipulatorRope	*ropeList[3];
	Umbilical			*mUmbilical;		// umbilical (null if non attached)
};

//------------------------------------------------------------------

class AV;
class Axis;
class Umbilical;

class ManPos3 : public ManPos {
public:
	ManPos3(ManipulatorRope *list[], int aleft, int aright, int acenter,
					SourceWeight *weight, const AV &aav);

	virtual int		calcState(const ThreeVector &pos, const float tensions[]);
	// calculates position, error, tensions and vectors from lengths and measured tensions
	virtual int		calcState(const float lengths[3], const float tensions[]);
	// calculates source velocity at current position with given rope speeds
	virtual ThreeVector calcVelocity(const float ropeSpeeds[3], const ThreeVector &pos,int flags);
	// calculates rope speeds at current position with given source velocity
	virtual int		calcRopeSpeeds(float ropeSpeeds[], const ThreeVector &vel, const ThreeVector &pos);
	// calculates position, error and rope lengths from measured tensions
	virtual int		calcState(const float tensions[]);
	// calculates carriage tilt
	virtual float	calcCarriageTilt();

	virtual void	reset() { resetCount = 0; }

	virtual void	constrainPosition(ThreeVector &pos);

private:
	int						validPosition(const ThreeVector &pos);
	int						validLengths(float l1, float l2, float l3);
	int						calcPosition(float l1, float l2, float l3, float t3);
	void					calcLengths(float &l1, float &l2, float &l3,
														const ThreeVector &position);

	float 				pulleyOffset; // distance from carriage pivot to pulley shaft
	ThreeVector		rightPulleyOffset;  // vector from origin to right pulley
	ManipulatorRope *leftRope, *rightRope, *centralRope;
	float 				lmin,rmin,lmax,rmax,cmax;
	float 				l3NeckCut;		// length of central rope when source is at top of vessel

	int 					ileft, iright, icenter;
	ThreeVector		s1, s2, s3;			// rope sums
	float					s11,s12,s22,s13,s23; // rope dot products
	float					b;							// denominator for calculations
	ThreeVector		nPlane, nPerp;	// horizontal unit vectors in plane of motion and perpendicular
	int						resetCount;			// counter to force calculation from center of vessel
};

//------------------------------------------------------------------

class ManPos1 : public ManPos {
public:
			ManPos1(ManipulatorRope *arope, SourceWeight *weight,const AV &aav);
			~ManPos1() { }

			virtual int		calcState(const ThreeVector &pos,const float tensions[]);
			virtual int		calcState(const float lengths[], const float tensions[]);
			virtual int		calcRopeSpeeds(float ropeSpeeds[], const ThreeVector &vel,
										const ThreeVector &position);
			virtual ThreeVector	calcVelocity(const float ropeSpeeds[], const ThreeVector &pos,int flags);
			virtual int		calcState(const float tensions[]);
			virtual void	constrainPosition(ThreeVector &pos)
											{ pos[X]=topBushing[X]; pos[Y]=topBushing[Y]; }

private:
			ThreeVector		topBushing;	// coordinates of top bushing
};

//------------------------------------------------------------------

ManPos *createManPos(const AV &av, LList<Axis> &axisList, SourceWeight *weight,
										 Umbilical *um);


#endif //__MANPOS_H__
