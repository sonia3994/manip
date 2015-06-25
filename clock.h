#ifndef __CLOCK_H
#define __CLOCK_H

#include <stdlib.h> // for exit() prototype
#include <time.h>

enum EClockFlags {
	kClockDate				= 0x01,
	kClockTenths 			= 0x02,
	kClockHundredths	= 0x04,
	kClockDST					= 0x08, 		// show daylight savings time flag
};

struct WideTime {
	unsigned long		low;
	unsigned long		high;
};


class Clock                     // real-time clock using TIO10 card
{
private:
	WideTime			lastTime;				// last time the clock was read
	time_t				zeroTime;				// offset to real time
	static Clock *current;

public:
	Clock();

	double        time(void);     // return real time
	char *				timeStr(double theTime, short flags=kClockTenths);	// convert time to string
	static Clock *getClock()	{ return current;	}
	static double	convertTime(const WideTime *tm) {
										return((tm->high * 4294967296.0 + tm->low) * 2.0e-7);
								}

};

#endif
