#ifndef __GLOBAL_H
#define __GLOBAL_H

#include <stdio.h>

#include "clock.h"
#include "display.h"

#define	GRAVITY	9.81f			// acceleration of gravity (m/s^2)
#define	D2O_DENSITY	1.10e-3 	// density of D2O (kg/cm^3)
#define H2O_DENSITY	1.0e-3		// density of H2O (kg/cm^3)


extern Clock 	 	*RTC;      // global real-time clock
extern Display	*display;

void writeCmdLog(char *host=NULL, char *msg=NULL);

#endif

