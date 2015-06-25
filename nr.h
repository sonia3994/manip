/* A header file for nr routines */
#ifndef __NR_H
#define __NR_H

#include "nrutil.h"

void gaussj(float **a, int n, float **b, int m);
void covsrt(float **covar, int ma, int ia[], int mfit);


#endif
