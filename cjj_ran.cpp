#include<math.h>
#include<stdio.h>

#ifndef HOME
#define HOME
#endif

#ifdef HOME
#include <ctype.h>
#include <stdlib.h>
#endif

#undef HOME

#include "cjj_hist.h"
#define PI 3.14159265358979323846264338328
#define TWOPI 6.28318530717958647692528677
#define TO_RAD 0.017453292521
#define TO_DEG 57.29577951

float mott_inte(float cutoff, float angle, float beta);
float mott(float angle, float beta);


/*********************************************
/*	These number defines are for the NR in C routine ran2() which I
/*	am calling cjj_ran()
/********************************************/
#define IM1 2147483563L
#define IM2 2147483399L
#define AM (1.0/IM1)
#define IMM1 (IM1-1)
#define IA1 40014L
#define IA2 40692L
#define IQ1 53668L
#define IQ2	52774L
#define IR1	12211L
#define IR2	3791L
#define NTAB 32
#define NDIV (1 + IMM1/NTAB)
#define EPS 1.2e-7
#define RNMX (1-EPS)  /* NR in C had (1.0 - EPS) */


/********************************************/
float cjj_ran(long* cjj_iseed_start)
/********************************************/
/*	Chris Jillings, July 1995
/*
/*	Returns a random float on the interval
/*  0 -> 1	exclusive of endpoints
/*	The observant reader will see this is
/*	simply NR in C's ran2() with some coding
/*	improvements (commented)
/*	First call must be with a negative number
/* 	to set up shuffle table
/*	Everywhere else, the integer argument is
/*	called cjj_iseed.
/*	cjj_iseed_start is used only in this routine
/*	The real seed (if cjj_iseed_start > 0 is
/*	stored in idum1
/*********************************************/
{
	int j;
	long k;
	long cjj_iseed;

	static long idum2 = 123456789L;
	static long idum1;		/*	this is the seed. If cjj_issed <1 then initialization,
								let idum1 = *cjj_iseed_start. Otherwise let it be!!!	*/
	static long iy = 0;
	static long iv[NTAB];
	float temp;


	/*	There is initialization to do if *cjj_iseed_start is <= 0 */
	/*	This should only occur on the first call where it
	/* 	must occur.	Also throw in an error check for zero (deadly) */
	/* 	I rewrote the control statements to make them clear */

	if (*cjj_iseed_start <=0)
	{
		if (*cjj_iseed_start == 0)	{			/* Don't want zero */
			*cjj_iseed_start = 1;
		}
		else	{
			*cjj_iseed_start  *= -1;			/* Flip sign */
		}
		idum2 = *cjj_iseed_start;
		for (j=NTAB + 7; j>=0; j--)	{
			k = (*cjj_iseed_start)/IQ1;
			*cjj_iseed_start = IA1* (*cjj_iseed_start - k*IQ1) - k*IR1;
			if (*cjj_iseed_start < 0) *cjj_iseed_start += IM1;
			if (j < NTAB) iv[j] = *cjj_iseed_start;
		}
		iy = iv[0];
		idum1 = *cjj_iseed_start;	/*If cjj_issed <=0, let idum1 = *cjj_iseed_start */
	}

	/*	This is the guts of the routine for all calls after the first */
	k = (idum1)/IQ1;
	idum1 = IA1 * (idum1 - k*IQ1) - IR1*k; 	/* get idum1 = (IA*idum1) % IM without overflow ... */
	if (idum1 < 0) idum1 += IM1;				/* ... by Schrage's method */
	k = idum2/IQ2;
	idum2 = IA2 * (idum2 - k*IQ2) - IR2*k; 	/* get idum2 = (IA*idum2) % IM without overflow ... */
	if (idum2 < 0) idum2 += IM2;				/* ... by Schrage's method */
	j = iy/NDIV;							/* will be in range 0..NTAB-1 */
	iy = iv[j] - idum2;								/* output prev value and refill shuffle table */
	iv[j] = idum1;
	if (iy < 1) iy += IMM1;


	/*	I rewrote the next lines to make  things more clear	*/
	/*	NR in C had two return statements and an assignemment in a control statement	*/
	temp = AM*iy;
	if (temp > RNMX)	{		/* prevent endpoints fromm being returned */
		temp = RNMX;
		}
	return temp;
}


/***********************************/
float throw_exp(float mean) 
/***********************************/
/*	Chris Jillings July 1995
/*	Throws to an exp decline with
/*      mean value mean
/***********************************/
{
	float retvar;
	float temp;
	long int i=2;
	
	temp = cjj_ran(&i);
	temp = log((double)temp);
	retvar = -1*mean*temp;
	return retvar;
}	

/**************************************/
float fgauss(float sigma)
/**************************************/
/*	Another routine from Tom Radcliffe
/*	Throws to a gaussian of mean 0 with
/*	a sigma supplied.
/*	Algorithm due to Kahn
/**************************************/
{
        float           y;
        float           z;
	long int i=1;

        do
        {
          y = -log(cjj_ran(&i));
          z = -log(cjj_ran(&i));
        } while ((y-1)*(y-1) >= 2*z);

        if (cjj_ran(&i) > 0.5f) y = -y;

        y *= sigma;

        return(y);
}

/******************************************/
int cjj_hist_ran(struct cjj_hist* source, struct cjj_hist* dest,
                 int number, char type)
/* The random number generator must be initialized before calling 
/* this routine
/******************************************/
{
    int i;  /* loop control */
    float max = 0.0; /* for finding max height of histo/spectrum */
    int ok=1; /* return variable, boolean */
    float x,y; /* the random numbers */
		float ys; /* the spectrum's y value at x */
    int good; /* boolean for accept/reject */
		int high_bin; /* for interpolating spectrum */
    int low_bin;  /* ditto */
    long seed = 1;

    for(i=0;i<source->num_bins;i++)  {
	if ( source->y[i] > max ) max = source->y[i];
    }
    if (max == 0.0 ) ok = 0;
    if ((type!='h')&&(type!='s')) ok = 0;

    if((ok)&&(type == 'h')) {
	for(i=0;i<number;i++){
	    good = 0;
	    while(!good) {
		x = source->x_low + (source->x_high-source->x_low)
                                     *  cjj_ran(&seed);
		y = cjj_ran(&seed) * max;
		if (y < source->y[cjj_hist_get_bin_index(source,x)]) good = 1;
	    }
	    cjj_hist_inc(dest,x);
	}
    }
    
    if ((ok)&&(type =='s')) {
	for(i=0;i< number;i++) {
	    good = 0;
	    while(!good) {
		x = source->x_low + 
                    (source->x_high - source->x_low - source->bin_width)
                                    * cjj_ran(&seed);
		y = cjj_ran(&seed) * max;
		low_bin = cjj_hist_get_bin_index(source,x);
		high_bin = low_bin +1;
		ys = (source->y[high_bin] - source->y[low_bin]) *
		    (x - source->x[low_bin] + (source->bin_width)/2.0)
		    + source->y[low_bin];
		if (y < ys) good = 1;
	    }
	    cjj_hist_inc(dest,x);
	}
    }
    return ok;
}





float get_mott_angle(float cutoff, float beta)
/**********************************
throws to an (approx) Mott angle
cutoff is in DEGREES
***********************************/
{
	static float F20,F30,F40,F60,F90,F180; 	/* this is the value of the cross section integrated 
                                                   from the cutoff to the angle in degrees */
	static float m20,m30,m40,m60,m90;  /* the value of the mott cross section, normed in a strange
					      way at 20, 30 ... degrees. This is used in the throw and 
					      reject part of the algorithm. These are functions of beta */
	float cutoffby2;
	static float cutoff_old;  /* to make sure it hasn't changed */
	static float beta_old = 0.5;
	static double norm20; /* the normalization for the cutoff to 20 degrees part */

	long int rs; /* random seed */
	float x,y,z; /* random numbers */

	float return_angle;  /* the return variable, in degrees */

	/* set the cutoff to 1 degree if it is too small */
	if (cutoff < 0.01) cutoff = 1.0;
	cutoff*=TO_RAD;
	/* the next line makes sure that either beta hasn't changed by too much or that the 
           cutoff hasn't changed. If they have changed, and cutoff is positive, make cutoff
           negative to reinitialize everything. */
	if( ((cutoff >0.0) && (abs(beta - beta_old)/beta_old > 0.05))  ||
                                ((cutoff > 0.0) && (cutoff_old != cutoff)) ) cutoff *=-1;
	cutoffby2 = cutoff/2.0;
	if (cutoff<0.0)
	{
		cutoff*=-1.0;
		cutoffby2 *= -1;
		F180 = mott_inte(cutoff,180,beta);
		F90  = mott_inte(cutoff,90,beta)/F180;
		F60  = mott_inte(cutoff,60,beta)/F180;
		F40  = mott_inte(cutoff,40,beta)/F180;
		F30  = mott_inte(cutoff,30,beta)/F180;
		F20  = mott_inte(cutoff,20,beta)/F180;
		beta_old = beta;  /* to make sure beta doesn't change much and the integrals are still valid */
		norm20 = 1.0/(pow(cutoff,-3.0) - pow(PI/9.0,-3.0));
		cutoff_old = cutoff;
		m20 = mott(20,beta);
		m30 = mott(30,beta);
		m40 = mott(40,beta);
		m60 = mott(60,beta);
		m90 = mott(90,beta);
	}
	
	/* Now we are initialized */
	x = cjj_ran(&rs);
	if ( x < F20 )
	{
		y = cjj_ran(&rs);
		return_angle = pow(cutoff,-3.0) - y/norm20;
		return_angle = pow(return_angle,-1.0/3.0);
		return_angle *= TO_DEG;		
	}
	else 	if (x < F30)
		{
			do
			{
				return_angle = 10*cjj_ran(&rs)+20.0;
				z = m20*cjj_ran(&rs);
			} while ( z > mott(return_angle,beta) ); 
		}
		else 	if (x < F40)
			{
				do
				{
					return_angle = 10*cjj_ran(&rs)+30.0;
					z = m30*cjj_ran(&rs);
				} while ( z > mott(return_angle,beta) ); 
			}
			else 	if (x < F60)
				{
					do
					{
						return_angle = 20*cjj_ran(&rs)+40.0;
						z = m40*cjj_ran(&rs);
					} while ( z > mott(return_angle,beta) ); 
				}
				else 	if (x < F90)
					{
						do
						{
							return_angle = 30*cjj_ran(&rs)+60.0;
							z = m60*cjj_ran(&rs);
						} while ( z > mott(return_angle,beta) ); 
					}
					else
					{
						do
						{
							return_angle = 90*cjj_ran(&rs)+90.0;
							z = m90*cjj_ran(&rs);
						} while ( z > mott(return_angle,beta) ); 
					}

	return return_angle;
}

float mott_mean_length(float cutoff,float beta,float gamma)
/*******************************
Returns the total Mott cross section integrated
from cutoff to PI and multiplies by some factors
to get the mean length in water
**************************************/
{
	float a;

	a = beta*beta*beta*beta * gamma*gamma;
	a /= (mott_inte(cutoff,180.0,beta) * 0.2757);
	return a;
}

float mott_inte(float cutoff, float angle, float beta)
/******************************
Integrates the Mott cross section between
cuttoff and angle (in degrees)
******************************/
{
	double a,b,angby2,cutoffby2;
	
	angle *= TO_RAD;
	cutoffby2 =cutoff*TO_RAD*0.5;
	angby2 =  angle *0.5;


	a = cos(angby2)*(-1-2*sin(angby2)*sin(angby2)+3*beta*beta*sin(angby2)*sin(angby2));
	a /= (sin(angby2)*sin(angby2)*sin(angby2));

	b = cos(cutoffby2)*(-1-2*sin(cutoffby2)*sin(cutoffby2)+3*beta*beta*sin(cutoffby2)*sin(cutoffby2));
	b /= (sin(cutoffby2)*sin(cutoffby2)*sin(cutoffby2));

	return(2.0*(a-b)/3.0);
}

float mott(float angle, float beta)
/*************************************************/
/* returns the cross section at a particular angle.
/* Normed in a funny way
/*************************************************/
{
	float a;
	angle *= TO_RAD;
	angle *= 0.5;
	
	a = (1.0 - beta * sin(angle) * sin(angle))/pow(sin(angle),4.0);

	return a; 
}


