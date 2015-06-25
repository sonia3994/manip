#ifndef __PIA_H
#define __PIA_H

#include "doslib.h"
#include <math.h>
class pia
{
	unsigned int data,control; //pia data and control registers
	unsigned int controlbyte;
	unsigned int databyte;
	unsigned int directionmask;
	unsigned int checkmask;
	void mask(){
		outp(control,controlbyte&0xfb); checkmask=inp(data);
		outp(data,directionmask);
		outp(control, controlbyte |4);};
public:
	pia(unsigned int ba){ data=ba;control=ba+1;
		directionmask=databyte=controlbyte=0;};
	unsigned int getdirection(){mask(); return checkmask;};
	void outbit(int bit){directionmask= directionmask|(1<<bit); mask();};
	void inbit(int bit){directionmask = directionmask& ~(1<<bit); mask();};
	void setdirection(unsigned int inmask){ directionmask=inmask; mask();};
	void out(int n){outp(data,n);};
	int in(){return inp(data);};
	void maskon(int n){databyte |= n; out(databyte);}
	void maskoff(int n){databyte &= ~n; out(databyte);}
};

#endif
