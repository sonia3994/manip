#include "threevec.h" // class header
#include <stdio.h>
#include "random.h"
#define X 0
#define Y 1
#define Z 2

// Define Cartesian basis vectors as globals

const ThreeVector iVector(1,0,0);
const ThreeVector jVector(0,1,0);
const ThreeVector kVector(0,0,1);
const ThreeVector oVector(0,0,0);

ThreeVec::ThreeVec(char* inString)	// initializing constructor with string arg
	{	int i;
		if (3!=(i=sscanf(inString,"%f %f %f",&x[X],&x[Y],&x[Z])))
			for(int j = i; j < 3; j++) x[j] = 0.f;
	}




/********************************************************/
/*                                                      */
/*      Rotates a vector through an angle phi           */
/*      (radians) about a vector v.                     */
/*      The Euler angles for rotation about the z axis  */
/*      and the new y axis are infered from v, and      */
/*      the vector is transformed into a new vector     */
/*      relative to the z-axis using these angles.      */
/*      It is then transformed back into the old system */
/*      with a transformation that includes a final     */
/*      rotation about the new z-axis.                  */
/*                                                      */
/*                              -- written 93-06-28 tjr */
/*                                                      */
/********************************************************/
//void ThreeVec::rotate(const ThreeVector &v, const float phi)
//{
//	float etheta,epsi;      /*  Euler angles */
//	RotationMatrix euler;   /*  Euler matrix */

//	etheta = (float) acos(v[Z]);     /*  find Euler angles */
//	if ((v[X] == 0.f)&&(v[Y] == 0.f)) epsi = 0.f;
//	else epsi = (float) atan2(v[X],v[Y]);

//	euler.forward(0.,etheta,epsi);

//	*this *= euler;    /*  bring vector back */

//	euler.reverse(phi,etheta,epsi);

//	*this *= euler;          /*  rotate vector */

//}



/********************************************************/
/*                                                      */
/*  Zeros elements smaller than epsilon   */
/*              */
/********************************************************/
ThreeVec &ThreeVec::truncate(float epsilon)
{
	int i;  // loop counter

	for (i = 0; i < 3; i++) if (fabs((*this)[i]) < epsilon) (*this)[i] = 0.f;

	return (*this);
}

/********************************************************/
/*                                                      */
/*  Zero all elements                                   */
/*                                                      */
/********************************************************/
ThreeVec &ThreeVec::zero(void)
{
	int i;  // loop counter

	for (i = 0; i < 3; i++) (*this)[i] = 0.f;

	return(*this);
}

/********************************************************/
/*                                                      */
/*      Scatters a vector through an angle theta        */
/*      (radians) with an arbitrary azimuthal angle.    */
/*      The Euler angles for rotation about the z axis  */
/*      and the new y axis are infered from u, and      */
/*      a new vector choosen relative to the z axis     */
/*      and then rotated through these angles.  The     */
/*      third Euler angle is not needed as the new      */
/*      vector is choosen with arbitrary azimuth.       */
/*                                                      */
/*                              -- written 92-07-30 tjr */
/*                                                      */
/********************************************************/
/*id ThreeVector::scatter(float theta)
{
	float etheta,epsi;     //  Euler angles
	float phi;             //  azimuthal angle
	RotationMatrix euler;  //  Euler matrix
	extern int nseed;      //  RNG seed

	phi = 2*M_PI*randm(&nseed);       //  random azimuth

	etheta = (float) acos((*this)[Z]); //  find Euler angles
	if (((*this)[X] == 0.f)&&((*this)[Y] == 0.f)) epsi = 0.f;
	else epsi = (float) atan2((*this)[X],(*this)[Y]);

	euler.reverse(phi,etheta,epsi);  //  generate inverse rotation
					 //    matrix (random phi)

	(*this)[X] = (float) sin(theta);   //  choose new vector
	(*this)[Y] = 0.0F;
	(*this)[Z] = (float) cos(theta);

	*this *= euler;                  //  rotate it

}
*/
/********************************************************/
/*                                                      */
/*      Converts direction cosines into Euler angles    */
/*      that will either transform u into the z-axis    */
/*      or transform the z-axis into u.                 */
/*                                                      */
/*       reverse generates matrix for u -> z-axis       */
/*       forward generates matrix for z-axis -> u       */
/*                                                      */
/*                              -- written 93-10-23 tjr */
/*                                                      */
/********************************************************/
void  ThreeVec::eulerCosine(float *phi, float* theta, float* psi)
{
				float   ephi,etheta,epsi;

				etheta = acos(x[Z]);            /*  find Euler angles */

				ephi   = -ACON*atan2(x[X],x[Y]);/* NOTE: DEGREES */
				etheta = -ACON*etheta;
				epsi = 0.f;         /*  never rotate about new z-axis */

				*phi   = ephi;
				*theta = etheta;
				*psi   = epsi;

}

ThreeVec& ThreeVec::normalize(void)   // normalize a ThreeVector
{
	float   norm;

	norm = sqrt(x[X]*x[X] + x[Y]*x[Y] + x[Z]*x[Z]);
   if (fabs(norm) < 1.e-32) norm = 1.e-32;
   *this /= norm;
	return(*this);

}

float ThreeVec::norm(void)const /* returns length of ThreeVector  */{
	float   Norm = x[X]*x[X] + x[Y]*x[Y] + x[Z]*x[Z];
	return sqrt(Norm);
}

/* Sample isotropic distribution                                */
ThreeVec &ThreeVec::isotropic(void)
{
	extern int nseed;
	float   stheta;
	float   phi;

	(*this)[Z] = 2.f*(0.5f - randm(&nseed));

	stheta = sin(acos((*this)[Z]));
	phi    = 2*M_PI*randm(&nseed);

	(*this)[X] = sin(phi)*stheta;
	(*this)[Y] = cos(phi)*stheta;

	return(*this);
}

/* Sample uniform distriubtion                          */
ThreeVec &ThreeVec::uniform(void)
{
	extern int nseed;
	float   theta,stheta;
	float   phi;

	theta = M_PI*randm(&nseed);
	phi   = 2*M_PI*randm(&nseed);

	stheta = sin(theta);

	(*this)[X] = sin(phi)*stheta;
	(*this)[Y] = cos(phi)*stheta;
	(*this)[Z] = cos(theta);

	return(*this);
}


ThreeVec ThreeVec::max(const ThreeVec &v)const // take maximum components
{
	ThreeVec w;

	for (int i=0; i<3; ++i) {
		w[i] = x[i]>v[i] ? x[i] : v[i];
	}

	return w;
}

ThreeVec ThreeVec::min(const ThreeVec &v)const // take minimum components
{
	ThreeVec w;

	for (int i=0; i<3; ++i) {
		w[i] = x[i]<v[i] ? x[i] : v[i];
	}

	return w;
}

ThreeVec ThreeVec::range(const ThreeVec &minV, const ThreeVec &maxV)
{
	ThreeVec w;

	for (int i=0; i<3; ++i) {
		w[i] = x[i]>minV[i] ? x[i] : minV[i];
		w[i] = w[i]<maxV[i] ? w[i] : maxV[i];
	}

	return w;
}


/*  FRIEND FUNCTIONS:           */
//PHvoid  ThreeVector::print(void)const {
//PH	printw("%f\t%f\t%f\n",this->x[X],this->x[Y],this->x[Z]);}

