#include <stdio.h>
#include <time.h>
#include "hwio.h"
#include "datain.h"
#include "ioutilit.h"

#include "loadcell.h"
#include "llsqr.h"

extern DataInput *dataInput;

const short	kMaxReadingDiff	= 0x20;	// max difference between successive readings


Command* Loadcell::commandList[LOADCELL_COMMAND_NUMBER] = {
	new Read,
	new ReadLoop,
	new StopLoop,
	new Calibrate,
	new CalibrationPoint
};


Loadcell::Loadcell(char* name):
		Controller("Loadcell",name) // H/w template
{
	// initialize for reading of parameters
	InputFile in("LOADCELL",name,1.00);

	maxLoad = in.nextFloat("MAXLOAD:","<maximum load>");
	slope = in.nextFloat("SLOPE:","<N/ADC bit>");
	intercept = in.nextFloat("OFFSET:","<N>");

	char *hwInStr = new char[100];
	strcpy(hwInStr,name);
	char *a = strstr(hwInStr,"LOADCELL");
	*a = '\0';
	strcat(hwInStr,"ANALOGHARDWAREIO");
	hwInput = dataInput->getHardwareIO(hwInStr);
	if (!hwInput) quit(hwInStr,"Loadcell");

	delete [] hwInStr;

	loopFlag = 0;               // not in loop mode
	calibrateFlag = 0;          // not in calibration mode
	lastReading = 0;

	failed_last_poll=last_i=0;
	set_tension_limits(10.0);   //default tension +/- 10 N

	poll();		// do our first poll
}

void Loadcell::monitor(char *outStr)
{
	outStr += sprintf(outStr, "Tension:   %.2f\n", getTension());
	outStr += sprintf(outStr, "ADC value: 0x%.3X (decimal %d)\n", lastReading, lastReading);
	hwInput->monitor(outStr);
}


void Loadcell::poll(void) // poll analog channel
{
	short curReading = hwInput->read();

	// PH 06/02/97 sign extend the 12 bit ADC
	//if (curReading & 0x0800) curReading |= 0xf000;

	short diff = curReading - (short)lastReading;

	if (diff<-kMaxReadingDiff || diff>kMaxReadingDiff) {
		// re-read hardware if the value changed too quickly
		// (this will avoid problems with a single read)
		curReading = hwInput->read();
		//if (curReading & 0x0800) curReading |= 0xf000;
		if (lastReading) {
			sprintf(display->outString,"Warning: %s reading [was %.3x, now %.3x] exceeds max diff",
						getObjectName(), lastReading, curReading);
			display->messageB(1,kErrorLine);
		}
	}
	lastReading = curReading;

	if (loopFlag) // flag set by readloop command, cleared by stoploop
	{
		sprintf(display->outString,"%s Tension: %f          ",getObjectName(),getTension());
		display->message();
	}
}


void Loadcell::set_tension_limits(float force)
{
	int 			i;
	long			avgReading = 0;
	const int kNumReadings = 50;

	for (i=0; i<kNumReadings; ++i) {
		lastReading = hwInput->read(); // must get new reading
		// PH 06/02/97 sign extend the 12 bit ADC
		//if (lastReading & 0x0800) lastReading |= 0xf000;
		avgReading += (long)getTension();		// gets reading from previous poll
	}
	avgReading = (avgReading + kNumReadings/2) / kNumReadings;

	minreading = avgReading - (long)force/(slope+1.e-8); // assume slope positive
	maxreading = avgReading + (long)force/(slope+1.e-8);

//	averages.reset();         // reset average tension
}

void Loadcell::calibrate(void)     // toggle calibration mode
{
	calibrateFlag = 1 - calibrateFlag; // toggle flag between 0 and 1

	if (calibrateFlag)  // must be starting calibration
	{
		sprintf(display->outString," %s entering calibration mode\n",getObjectName());
		display->message();
		fit.init();       // initialize fitting routine
	}
	else                // done calibration, output fit parameters
	{
		if (fit.pointNumber() >= 1)	// need at least one point to calibrate
		{
			if (fit.pointNumber() == 1) {
				display->message("Only one calibration point -- setting offset only");
				fit.setSlope(1/slope);
			} else {
				// need 2 points to calibrate slope
				slope = 1.f/fit.slope();
			}
			intercept = -slope*fit.intercept();
			sprintf(display->outString,"%s T = %f + %f x READING\n",getObjectName(),intercept,slope);
			display->message();
			writeCmdLog();
		}
		else                         // tell user a mistake has been made
		{
			sprintf(display->outString,"NEED AT LEAST TWO POINTS TO CALIBRATE\n");
			display->message();
			return;                    // prevent attempted database file update
		}

/* If get to here, must have successfully completed calibration,  */
/*  so try to update calibration constants in database file       */

		// create outpuf file object
		OutputFile	out("LOADCELL", getObjectName() ,1.00);

		out.updateFloat("SLOPE:", slope);		// update calibration slope
		out.updateFloat("OFFSET:", intercept);	// update intercept

		long tempTime=time((time_t*)NULL);
		char *dateStr=ctime(&tempTime); // This string is always 26 characters
																		// long, including a newline that will
																		// be removed.

		newLine2Null(dateStr);

		out.updateLine("DATE:", dateStr);

		sprintf(display->outString,"%s database entry updated\n",getObjectName());
		display->message();
	}
}

void Loadcell::calibrationPoint(char* string) // add a calibration point
{
		float force;
		char  units[15];    // units of force used
		const int kNumReadings = 10;

		if (!calibrateFlag)	// prevent bogus point addition
		{
			sprintf(display->outString,"Can't add point until in calibration mode");
			display->message();
			return;
		}

		strcpy(units,"--------------"); // default string

		sscanf(string," %f %s\n",&force,units);

		if (!strncmp(units,"kg",2)||!strncmp(units,"kilogram",8))
		{
			force *= 9.81f;
		}
		else if (!(strncmp(units,"lb",2)||!strncmp(units,"pound",5)))
		{
			force *= 21.58f;  // 21.58 ~ 2.2*9.81
		}
		else if (strncmp(units,"N",1)&&strncmp(units,"newton",6))
		{
			sprintf(display->outString,"Can't handle units: %s\n(need kg, lb or N)\n",units);
			display->message();
			return;
		}


		int reading[kNumReadings];              // get average value and variance
		float average  = 0.f;
		float sigmasqr = 0.f;
		int i;
		for(i = 0; i < kNumReadings; i++)
		{
//			analogChannel->poll();
			short rawValue = hwInput->read();
			// PH 06/02/97 sign extend the 12 bit ADC
			//if (rawValue & 0x0800) rawValue |= 0xf000;
			reading[i] = rawValue;
			average += reading[i];
		}
		average /= kNumReadings;
		for(i = 0; i < kNumReadings; i++)
		{
			sigmasqr += (average-reading[i])*(average-reading[i]);
		}
		sigmasqr /= kNumReadings;
		if (sigmasqr < 1.e-6) sigmasqr = 1.f;

		fit.add_point(force,average,sigmasqr);    // add point to fit

		sprintf(display->outString,"Reading: %f +/- %f",average,sqrt(sigmasqr));
		display->message();
		display->message("");
		sprintf(display->outString,"Old ADC value: %d", lastReading);
		display->message();
		char *pt = display->outString;
		pt += sprintf(pt,"Raw readings: ");
		for (i=0; i<kNumReadings; ++i) {
			pt += sprintf(pt," %d",reading[i]);
		}
		display->message();
}
