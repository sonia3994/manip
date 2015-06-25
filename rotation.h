#ifndef	__ROTATIONMATRIX_H
#define __ROTATIONMATRIX_H

//#include <iostream.h>
#include "threevec.h"

class RotationMatrix	// matrix for rotations in three-dimensions
{
 private:
	float	m[3][3];

 public:
	RotationMatrix operator*(RotationMatrix matrix);	// matrix * matrix
	ThreeVector operator*(const ThreeVector &v) const;
	void reverse(float phi, float theta, float psi);	// inverse rot. mat.
	void forward(float phi, float theta, float psi);	// forward rot. mat.
	void ypr(float yaw, float pitch, float roll);
	void truncate(float t);
	void set(int i, int j, float value) {m[i][j] = value;}
	float get(int i, int j) {return(m[i][j]);}
	void setUnit();

	friend ThreeVector &solve(const RotationMatrix &matrix, const ThreeVector &v);
//  friend ostream& operator<<(ostream& os, RotationMatrix matrix); // output
};


// Related, non member functions

	ThreeVector &solve (const RotationMatrix &alpha, const ThreeVector &v);	// solve [alpha]*dx = v
  ThreeVector	solve(float alpha[3][3],const ThreeVector &v);			// solve [alpha]*dx = v

#endif
