#include "llsqr.h"

void linear_least_square::add_point(float x, float y, float sigmasqr)
{
	if (n == 10)
	{
//PH		sprintf(display->outString,"Only 10 calibration points allowed\n");
//PH		display->message();
		return;
	}

	xi[n] = x;  // used for later calculation
	yi[n] = y;
	si[n] = sqrt(sigmasqr);

	n++;
	sum   += 1.f/sigmasqr;
	xsum  += x/sigmasqr;
	ysum  += y/sigmasqr;
	finished=0;

}

void linear_least_square::calc()
{
	int i;
	float t;
	float ttsum = 0.f;

	if(finished)return;

	if (n > 1) {
		// set intercept only if just one point - PH 09/25/98
		b = 0.f;
		for(i = 0; i < n; i++)
		{
			t = (xi[i] - xsum/sum)/si[i];
			ttsum += t*t;
			b += t*yi[i]/si[i];
		}

		b /= ttsum;
	}
	a = (ysum - xsum*b)/sum;

	finished=1;
}

