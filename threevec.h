#ifndef __THREEVECTOR_H
#define __THREEVECTOR_H

#include <math.h>				// trig. and fabs declarations
//#include <iostream.h>		// for << overloading

#define X   0							// ThreeVector index values
#define Y	1
#define Z	2
#define	TINY 1.e-20			// a small number
#define	ACON	57.29577951			// radians -> degrees


class ThreeVec
{
 private:
	float      	x[3];				// the three-vector data
 public:
 	/*  CONSTRUCTORS:							*/
	ThreeVec(void) { ; }					// do-nothing constructor
	ThreeVec(const float xyz0)			// initializing constructor
			{x[X]=xyz0; x[Y]=xyz0; x[Z]=xyz0;}	// 	with one arg.
	ThreeVec(const float x0, const float y0, const float z0) 	// initializing constructor
			{x[X]=x0; x[Y]=y0; x[Z]=z0;}	    	// 	with three args.
	ThreeVec(char* inString);	// initializing constructor with string arg
	ThreeVec(const ThreeVec &v)
			{x[X]=v[X]; x[Y]=v[Y]; x[Z]=v[Z];}

	/* Primary	OPERATORS:							*/
	float &operator[](const int i) {return x[i];}
	float operator[](const int i)const { return x[i];}
	ThreeVec &operator=(const ThreeVec & v)	// assignment operator
			{x[X]=v.x[X]; x[Y]=v.x[Y]; x[Z]=v.x[Z]; return *this;};

	/* Normal Vector Operators: */
	ThreeVec operator+(const ThreeVec &v)const;		// add and subtract vectors
		ThreeVec operator-(const ThreeVec &v)const;
	void operator+=(const ThreeVec &v);	// vector operations
	void operator-=(const ThreeVec &v); //	plus assignment


	/* Unary Operator */
	ThreeVec operator-(void)const;			// unitary minus

	 /* Scalar Operators */
	ThreeVec operator*(const float f)const
		{	return ThreeVec(x[X]*f,x[Y]*f,x[Z]*f); }
	ThreeVec operator/(const float f)const
		{	return ThreeVec(x[X]/f,x[Y]/f,x[Z]/f); }
	friend ThreeVec operator*(const float f,const ThreeVec &v);
	 void operator*=(const float f);
	void operator/=(const float f);

	/* Vector Comparisons */
		int operator==(const ThreeVec &v)const			// elemental comparisons
		{return((v[X]==x[X])&&(v[Y]==x[Y])&&(v[Z]==x[Z]));}
	int operator!=(const ThreeVec &v)const
		{return((v[X]!=x[X])||(v[Y]!=x[Y])||(v[Z]!=x[Z]));}

	/* Unconventional Vector Operations */
	 operator int() const { return(x[0]!=0 || x[1]!=0 || x[2]!=0); }
//	ThreeVec operator+(const float f)const;		// scalar operations
//	ThreeVec operator-(const float f)const;
	float operator*(const ThreeVec &v) const {return dot(v);};
	friend ThreeVec operator+(const float f,const ThreeVec &v);
	friend ThreeVec operator-(const float f,const ThreeVec &v);
	void operator+=(const float f);			// scalar operations
	void operator-=(const float f);			//  plus assignment
	int operator>=(const ThreeVec &v)const			// vector norm comparisons
		{return(norm()>=v.norm());}
	int operator<=(const ThreeVec &v)const
		{return(norm()<=v.norm());}
	int operator<(const ThreeVec &v)const
		{return(norm()<v.norm());}
	int operator>(const ThreeVec &v)const
		{return(norm()>v.norm());}
	int operator>=(const float f)const {return(norm()>=f);}	// scalar norm comparisons
	friend int operator>=(const float f, const ThreeVec &v);
	int operator<=(const float f) const {return(norm()<=f);}
	friend int operator<=(const float f, const ThreeVec &v);
	int operator>(const float f) const {return(norm()>f);}
	friend int operator>(const float f, const ThreeVec &v);
	int operator<(const float f)const {return(norm()<f);}
	friend int operator<(const float f, const ThreeVec &v);
	int operator==(const float f)const {return(norm()==f);}
	friend int operator==(const float f, const ThreeVec &v);
	int operator!=(const float f) const {return(norm()!=f);}
	friend int operator!=(const float f, const ThreeVec &v);

	/*	MEMBER FUNCTIONS:						*/
	float	norm(void) const;								// return length
	ThreeVec	&normalize(void);					// normalize
	ThreeVec	&zero(void);								// zero all elements
	ThreeVec	cross(const ThreeVec &v) const;		// cross product
	float		dot(const ThreeVec &v)const 					// dot product
		{return (x[X]*v[X] + x[Y]*v[Y] + x[Z]*v[Z]);}
//	friend ostream& operator<<(ostream& os, const ThreeVec &v);	// output

	void  print(void)const;
	void 	rotate(const ThreeVec &n, float phi); // rotate phi about n
	void	scatter(float theta);	// scatter through theta & randm. phi
	void	eulerCosine(float *phi, float* theta, float* psi); // Euler angles
	ThreeVec	&truncate(float epsilon);	// zero elements smaller than epsilon
	ThreeVec	&isotropic(void);		// generate isotropic TV
	ThreeVec	&uniform(void);			// generate uniform TV
	ThreeVec abs(void)
		{x[X] = fabs(x[X]);  x[Y] = fabs(x[Y]); x[Z] = fabs(x[Z]); return(*this);}
	ThreeVec &mask(const ThreeVec &v)			// mask one vector with another
		{x[X] *= v.x[X]; x[Y] *= v.x[Y]; x[Z] *= v.x[Z]; return(*this);}

	ThreeVec max(const ThreeVec &v)const ;
	ThreeVec min(const ThreeVec &v)const ;
	ThreeVec range(const ThreeVec &minV, const ThreeVec &maxV);
};		// end of ThreeVec class declaration

typedef ThreeVec ThreeVector;

/* Inline non-member function declarations */

inline ThreeVec cross(const ThreeVec &v1,const ThreeVec &v2){	return v1.cross(v2);};
inline float norm(const ThreeVec &v1){return v1.norm();};
/*
ostream& operator<<(ostream& os, const ThreeVec &v){
		return os << "(" << v[X] << " " << v[Y] << " " << v[Z]<< ")";
}

*/


/* Cartesian coordinate basis vectors and Origin*/

extern const ThreeVector iVector, jVector, kVector, oVector;
/* Inline member function definitions */

inline ThreeVec ThreeVec::operator-(void)const{
	return ThreeVec(-x[X],-x[Y],-x[Z]);
}

inline ThreeVec ThreeVec::operator+(const ThreeVec &v)const{
	return ThreeVec(x[X]+v[X],x[Y]+v[Y],x[Z]+v[Z]);
}

inline ThreeVec ThreeVec::operator-(const ThreeVec &v)const{
	return ThreeVec(x[X]-v[X],x[Y]-v[Y],x[Z]-v[Z]);
}

/*
ThreeVec ThreeVec::operator+(const float f)const{
	return ThreeVec(x[X]+f,x[Y]+f,x[Z]+f);
}

ThreeVec ThreeVec::operator-(const float f)const{
	return ThreeVec(x[X]-f,x[Y]-f,x[Z]-f);
}
*/

inline void ThreeVec::operator+=(const float f){
	x[X] += f;
	x[Y] += f;
	x[Z] += f;
}

inline void ThreeVec::operator-=(const float f){
	x[X] -= f;
	x[Y] -= f;
	x[Z] -= f;
}

inline void ThreeVec::operator+=(const ThreeVec &v){
	x[X] += v.x[X];
	x[Y] += v.x[Y];
	x[Z] += v.x[Z];
}

inline void ThreeVec::operator-=(const ThreeVec &v){
	x[X] -= v.x[X];
	x[Y] -= v.x[Y];
	x[Z] -= v.x[Z];
}

inline void ThreeVec::operator*=(const float f)
{
	x[X] *= f;
	x[Y] *= f;
	x[Z] *= f;
}
inline void ThreeVec::operator/=(const float f)
{
	x[X] /= f;
	x[Y] /= f;
	x[Z] /= f;
}

inline ThreeVec ThreeVec::cross(const ThreeVec &v)const // cross product
{
	return ThreeVec ( x[Y]*v[Z]-x[Z]*v[Y], x[Z]*v[X]-x[X]*v[Z],
			 x[X]*v[Y]-x[Y]*v[X]);
}
//ThreeVec operator+(const float f, const ThreeVec &v) {return (v + f);}
//ThreeVec operator-(const float f,const  ThreeVec &v) {return (v - f);}
inline ThreeVec operator*(const float f, const ThreeVec &v) {return (v * f);}

inline int operator>=(const float f, const ThreeVec &v) {return(f>=v.norm());}
inline int operator<=(const float f, const ThreeVec &v) {return(f<=v.norm());}
inline int operator>(const float f,  const ThreeVec &v)  {return(f>v.norm());}
inline int operator<(const float f,  const ThreeVec &v)  {return(f<v.norm());}
inline int operator==(const float f, const ThreeVec &v) {return(f==v.norm());}
inline int operator!=(const float f, const ThreeVec &v) {return(f!=v.norm());}

#endif
