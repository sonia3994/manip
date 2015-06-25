#ifndef __SIGMA_H
#define __SIGMA_H

#include <math.h>

class sigma
{
public:

	long n;
	double xsum,x2sum,xaverage;
	float sqrtarg;
	sigma(){reset();}
	float sig();
	void reset(){n=0;xsum=x2sum=xaverage=0;}
	void add(float x){	xsum+=x; x2sum += x*x;n++; xaverage=xsum/n;}

};
#endif

