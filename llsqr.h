#ifndef __LLSQR_H
#define __LLSQR_H

#include <math.h>
#include "display.h"

class linear_least_square
{
	void linear_least_sqare() { b=0; }

private:

	int n;
	int finished;
	float sum,xsum,ysum;
	float a,b;
	float xi[10],yi[10],si[10];
	void  calc();

public:

	float slope()       {calc(); return b;};
	float intercept()   {calc();return a;};

	float setSlope(float s) { b=s; finished=0; }

	linear_least_square(){finished=n=0;sum=xsum=ysum=0.f;}
	void init(void) {finished=n=0;sum=xsum=ysum=0;}

	void add_point(float x, float y, float sigmasqr);
	int pointNumber(void) {return(n);}

};

#endif
