#include "rotation.h"

void RotationMatrix::truncate(float t)
{
  int	i,j;
  
  for(i = 0; i < 3; i++)
    for(j = 0; j < 3; j++) 
      if (fabs(m[i][j]) < t) m[i][j] = 0.f;
}

void RotationMatrix::setUnit()
{
	int	i,j;

	for(i = 0; i < 3; i++)
		for(j = 0; j < 3; j++)
			if (i==j) m[i][j] = 1;
			else m[i][j] = 0;
}

RotationMatrix RotationMatrix::operator*(RotationMatrix matrix)
{
		int	i,j,k;
		RotationMatrix product;

		for(i = 0; i < 3; i++)
		{
			for(j = 0; j < 3; j++)
			{
	product.m[i][j] = 0.f;
	for(k = 0; k < 3; k++) product.m[i][j] += m[i][k]*matrix.m[k][j];
			}
		}

		return product;

}
ThreeVector RotationMatrix::operator*(const ThreeVector &v) const
{
	int i,j;                  /*  loop counters */
	ThreeVector vector(0.f);      /*  dummy variable */

	for( i = 0; i < 3; i++)
		for( j = 0; j < 3; j++) vector[i]=vector[i]+m[i][j]*v[j];

	return vector;
}


/********************************************************/
/*                                                      */
/*      Defines inverse rotation matrix.  See Goldstein,*/
/*      p. 128 ff.                                      */
/*                                                      */
/*                -- modified from FORTRAN 92-07-30 tjr */
/*                                                      */
/********************************************************/
void RotationMatrix::reverse(float phi, float theta, float psi)
{
    float cphi,sphi;
    float ctheta,stheta;
    float cpsi,spsi;
    
    cphi   = (float) cos(phi); 
    sphi   = (float) sin(phi); 
    ctheta = (float) cos(theta); 
    stheta = (float) sin(theta); 
    cpsi   = (float) cos(psi); 
    spsi   = (float) sin(psi); 
        
    m[0][0] = cphi*cpsi - ctheta*sphi*spsi;       
    m[0][1] = cpsi*sphi + cphi*ctheta*spsi; 
    m[0][2] = spsi*stheta; 
    
    m[1][0] = -cpsi*ctheta*sphi - cphi*spsi;       
    m[1][1] = cphi*cpsi*ctheta - sphi*spsi; 
    m[1][2] = cpsi*stheta; 
    
    m[2][0] = sphi*stheta;
    m[2][1] = -cphi*stheta; 
    m[2][2] = ctheta;
    
}
  
  
/********************************************************/
/*                                                      */
/*      Defines rotation matrix.  See Goldstein,        */
/*      p. 128 ff.                                      */
/*                                                      */
/*                -- modified from FORTRAN 92-07-30 tjr */
/*                                                      */
/********************************************************/
void RotationMatrix::forward(float phi, float theta, float psi)
{
    float cphi,sphi;
    float ctheta,stheta;
    float cpsi,spsi;
    
    cphi   = (float) cos(phi); 
		sphi   = (float) sin(phi);
    ctheta = (float) cos(theta); 
    stheta = (float) sin(theta); 
    cpsi   = (float) cos(psi); 
    spsi   = (float) sin(psi); 
        
    m[0][0] = cphi*cpsi-ctheta*spsi*sphi;       
    m[0][1] = -cphi*spsi-ctheta*cpsi*sphi; 
    m[0][2] = sphi*stheta; 
    
    m[1][0] = sphi*cpsi+ctheta*spsi*cphi;       
    m[1][1] = -spsi*sphi+ctheta*cphi*cpsi;
    m[1][2] = -cphi*stheta;

    m[2][0] = stheta*spsi;
    m[2][1] = stheta*cpsi;
    m[2][2] = ctheta;
}

///////////////////////////////////////////////////////////
//
//	Uses yaw, pitch and roll to build a rotation
//
//	Chris Jillings, Sept, 1998
//
///////////////////////////////////////////////////////////
void RotationMatrix::ypr(float yaw, float pitch, float roll)
{
	// yaw = phi
	// pitch = theta
	// roll = psi
	// see Goldstein page 608

	float cosThe = cos(pitch);
	float sinThe = sin(pitch);
	float cosPhi = cos(yaw);
	float sinPhi = sin(yaw);
	float cosPsi = cos(roll);
	float sinPsi = sin(roll);


	m[0][0] = cosThe*cosPhi;
	m[0][1] = cosThe*sinPhi;
	m[0][2] = -sinThe;

	m[1][0] = sinPsi*sinThe*cosPhi - cosPsi*sinPhi;
	m[1][1] = sinPsi*sinThe*sinPhi + cosPsi*cosPhi;
	m[1][2] = cosThe*sinPsi;

	m[2][0] = cosPsi*sinThe*cosPhi + sinPsi*sinPhi;
	m[2][1] = cosPsi*sinThe*sinPhi - sinPsi*cosPhi;
	m[2][2] = cosThe*cosPsi;

}








ThreeVector &solve(const RotationMatrix &matrix, const ThreeVector &v)
{
	float alpha[3][3];
	int i,j;

	for(i = 0; i < 3; i++)
		for(j = 0; j < 3; j++)
			alpha[i][j] = matrix.m[i][j];
    static ThreeVector sol = solve(alpha,v); //PH
    return(sol);

}

/********************************************************/
/*                                                      */
/*  Solves the equation [alpha]*dx = *this using        */
/*  Gaussian elimination with partial pivoting.         */
/*                                                      */
/********************************************************/
ThreeVector solve( float alpha[3][3],const ThreeVector &v)
{
	int     i,j,k,m,n;    // loop counters
	int     jpvt;     // pivot index
	int     ip1;      // dummy for i+1
	float   tmp;      // temporary value for swapping
	float   ratio;    // pivot ratio for reduction

	int   failFlag = 0; // flag to note failure

	int   rowZero[3]; // flags for rows with zero sums
	int   colZero[3]; // flags for columns with zero sums
	int   rZ,cZ;    // number of rows and columns with zeros

	ThreeVector rhs(v); // v is right-hand side of equation
	ThreeVector solution(0.0);    // solution vector

	rZ = 0;
	cZ = 0;
	for(i = 0; i < 3; i++)  // find zero rows and columns
	{
		rowZero[i] = 1;
		colZero[i] = 1;
		for(j = 0; j < 3; j++)
		{
			if (alpha[i][j]) rowZero[i] = 0;
			if (alpha[j][i]) colZero[i] = 0;
		}
		rZ += rowZero[i];
		cZ += colZero[i];
	}

	if ((cZ==1) && (rZ==1))   // one row and column zero
	{
		for(i = 0; i < 3; i++)  // check that rhs has zero in right place
			if ((rowZero[i])&&(rhs[i] == 0.f)) break;

		if (i == 3)     // no solution
		{
			solution = 0.f;
			return(solution);
		}
		else      // 2D system
		{
			float a[2][2];  // 2D system
			float b[2];

			m = 0;
			for(j = 0; j < 3; j++)  // set up 2D system
			{
				if (rowZero[j]) continue; // skip zero row
				n = 0;
				for(k = 0; k < 3; k++)
				{
					if (colZero[k]) continue; // skip zero column
					a[m][n] = alpha[j][k];
					n++;
				}
				b[m] = rhs[j];
				m++;
			} // done setting up 2D system

			if (a[0][0] != 0.f)
			{
				a[1][1] -= a[0][1]*a[1][0]/a[0][0];
				if (a[1][1] == 0.f)   // no solution
				{
					solution = 0.f;
					return(solution);
				}
				b[1] = (b[1] - b[0]*a[1][0]/a[0][0])/a[1][1];
				b[0] = (b[0] - b[1]*a[0][1])/a[0][0];
			}
			else    // a[0][0] == 0.f
			{
				if ((a[1][1] == 0.f)||(a[0][1] == 0.f))   // no solution
				{
					solution = 0.f;
					return(solution);
				}

				b[0] /= a[0][1];
				b[1] = (b[1] - b[0]*a[1][0])/a[1][1];

			}

			m = 0;        // reconstruct 3D solution vector
			for(j = 0; j < 3; j++)
			{
// changed PH 01/31/97
//			if (rowZero[j]) solution[j] = 0.f;
				if (colZero[j]) solution[j] = 0.f;
				else solution[j] = b[m++];
			}

		}

		return(solution);   // return solution of 2D system
	}
	else if ((rZ==2)&&(cZ==2))  // two rows and columns zero
	{
		// re-written by PH 01/30/97
		for (i=0; i<3; ++i) if (!colZero[i]) break;	// find non-zero col
		for (j=0; j<3; ++j) if (!rowZero[j]) break;	// find non-zero row

		for (k=0; k<3; ++k) {		// check that rhs is zero where required
			if (rowZero[k] && rhs[k]!=0.f) {
				solution = 0.f;
				return(solution);
			}
		}
		solution = 0.f;   // solve 1D equation

		solution[i] = rhs[j]/alpha[i][j];
		return(solution);
	}

	for(i = 0; i < 2; i++)  // solve 3D system: row-reduction loop
	{
		jpvt = i;       // trial pivot
		ip1  = i + 1;   // saves some adds later on
		for(j = ip1; j < 3; j++)  // look at the rest of the entries in column
		{
			if (fabs(alpha[jpvt][i]) < fabs(alpha[j][i])) jpvt = j;
		}

		if (fabs(alpha[jpvt][i]) < TINY)  // no solution
		{
			failFlag = 1;
			break;
		}

		if (jpvt != i)  // need to swap rows
		{
			for(j = i; j < 3; j++)
			{
				tmp     = alpha[i][j];
				alpha[i][j]   = alpha[jpvt][j];
				alpha[jpvt][j]  = tmp;

			}

			tmp   = rhs[i];         // no augmented matrix, so swap rhs by hand
			rhs[i]  = rhs[jpvt];
			rhs[jpvt] = tmp;
		}

		for(j = ip1; j < 3; j++)  // do row-reduction
		{
			if (alpha[j][i] == 0.f) continue; // no need to subract zero
			ratio = alpha[j][i]/alpha[i][i];
			for(k = ip1; k < 3; k++) alpha[j][k] -= ratio*alpha[i][k];
			rhs[j] -= ratio*rhs[i];

		}

	} // end of Gaussian elimination loop

	if (!failFlag)  // solution probably exists
	{
		if (fabs(alpha[2][2]) < TINY) // no solution
		{
			 solution = 0.f;      // send back zero solution vector
		}
		else          // do back-substitution
		{
			solution[2] = rhs[2]/alpha[2][2];
			solution[1] = (rhs[1] - solution[2]*alpha[1][2])/alpha[1][1];
			solution[0] = (rhs[0] - solution[1]*alpha[0][1] - solution[2]*alpha[0][2])/alpha[0][0];
		}
	}
	else            // no solution
	{
		solution = 0.f;
	}
	return(solution); // return solution vector

}

