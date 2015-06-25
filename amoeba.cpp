#include <stdio.h>
#include <math.h>
#include <stdlib.h>

#include "hwutil.h"		// for quit()
#include "threevec.h"
#include "display.h"

#define NMAX 500	// don't allow very many trials
#define ALPHA 1.0
#define BETA 0.5
#define GAMMA 2.0

#define GET_PSUM for (j=0;j<ndim;j++) { for (i=0,sum=0.0;i<mpts;i++)\
					sum += p[i][j]; psum[j]=sum;}

float (*func)(float *);

float amotry(float p[4][3], float y[3], int ndim, float (*func)(float*),
	int ihi, int *nfunk, float fac, ThreeVector freeze);

float *ptry;  /* file scope to save reallocation  */
float *psum;

void amoeba(float p[4][3],
						float y[4],
						int* nd,
						float* ft,
						float (*func)(float*),
						int* nfunk,
						int absFlag,
						int initFlag,
						ThreeVector freeze)
{
	int i,j,ilo,ihi,inhi,mpts,ndim;
	float ytry,ysave,sum,rtol,ftol;

	ndim = *nd;
	ftol = *ft;
	mpts = ndim + 1;

	if (initFlag) 
	{ 
		psum = (float*) malloc(ndim*sizeof(float));
		ptry= (float*) malloc(ndim*sizeof(float));
		if ((!ptry)||(!psum)) 
		{
			qprintf("Allocation failure in amoeba\n");
			quit();
		}
	}
		
	*nfunk=0;
	GET_PSUM
	for (;;) 
	{
		ilo=1;
		ihi = y[0]>y[1] ? (inhi=1,0) : (inhi=0,1);

		for (i=0; i<mpts; i++) 
		{
			if (y[i] < y[ilo]) ilo=i;
			if (y[i] > y[ihi]) 
			{
				inhi=ihi;
				ihi=i;
			} 
			else if (y[i] > y[inhi])
			{
				if (i != ihi) inhi=i;
			}

		}

		if (absFlag) 
		{
			rtol = y[ihi];
		}
		else
		{
			rtol=2.0*fabs(y[ihi]-y[ilo])/(fabs(y[ihi])+fabs(y[ilo])+1.e-8);
		}

		if (rtol < ftol) break;

		if (*nfunk >= NMAX) 
		{
			return;
		}
		ytry=amotry(p,y,ndim,func,ihi,nfunk,-ALPHA,freeze);
		if (ytry <= y[ilo])
		{
			ytry=amotry(p,y,ndim,func,ihi,nfunk,GAMMA,freeze);
		}
		else if (ytry >= y[inhi])
		{
			ysave=y[ihi];
			ytry=amotry(p,y,ndim,func,ihi,nfunk,BETA,freeze);
			if (ytry >= ysave) 
			{
				for (i=0; i<mpts; i++) 
				{
		if (i != ilo) 
		{
			for (j = 0; j < ndim; j++) 
			{
				psum[j]=0.5*(p[i][j]+p[ilo][j]);
				p[i][j]=psum[j];
				
			}
			y[i]=(*func)(psum);
		}

				}
				*nfunk += ndim;
				GET_PSUM
			}
		}

	}

		if (initFlag < 0)
	{
		free(psum);
		free(ptry);
	}
	
}

float amotry(float p[4][3], float y[3], int ndim, float (*func)(float*),
	int ihi, int *nfunk, float fac, ThreeVector freeze)
{
	int j;
	float fac1,fac2,ytry;

	fac1=(1.0-fac)/ndim;
	fac2=fac1-fac;
	for (j=0;j<ndim;j++)
	{
		if(freeze[j]!=0.f) ptry[j]=psum[j]*fac1-p[ihi][j]*fac2;
		else ptry[j] = p[ihi][j];
	}
	ytry=(*func)(ptry);
	++(*nfunk);
	if (ytry < y[ihi]) 
	{
		y[ihi]=ytry;
		for (j=0;j<ndim;j++) 
		{
			psum[j] += ptry[j]-p[ihi][j];
			p[ihi][j]=ptry[j];

		}
	}
	return ytry;
}

#undef ALPHA
#undef BETA
#undef GAMMA
#undef NMAX

