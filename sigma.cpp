#include "sigma.h"

float sigma::sig(){
	if(n>2){
		sqrtarg=(x2sum-xaverage*xsum)/(n-1);
		if(sqrtarg<0){
			sqrtarg=0;
		}
		return sqrt(sqrtarg);
	}else return 0.0;
}
