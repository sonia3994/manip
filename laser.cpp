// LASER.CPP   Code to run the N2 Laser.
// Richard Ford  29th March 1997
//
// Revisions:    25/1/98 RF  Added commands to set filter wheels.
//
//               21/7/98 R Ford.  Added code to use pulser trigger.
// Notes:
//
//
#include "laser.h"

extern DataInput *dataInput;

Command* Laser::commandList[LASER_COMMAND_NUMBER] = {
	new PowerOn,
	new PowerOff,
	new Start,
	new Stop,
	new HiPressure,
	new LowPressure,
	new GasFlow,
	new SetND,
	new Rate,
	new Block
};

const float	updateDatabasePeriod = 10.0;
const float	serviceLogicPeriod = 2.0;
const float	waitToTriggerPeriod = 90.0;
const int   maxTriggerAttempts = 2;
const float flowCoeff1 = 4.0;
const float flowCoeff2 = 10.0;
const float kMaxRate = 50.0;
const float kMinRate = 1.0;
const float kdefaultRate = 10.0;

Laser::Laser(char* name):
	Controller("Laser",name)
{
	const int kMaxLen = 128;
	char componentName[kMaxLen];
	char hwInStr[kMaxLen];
	char* key;            		// pointer to character *AFTER* keyword
	char *namePtr;

	// initialize for reading of parameters
	InputFile in("LASER",name,1.00);

	key = in.nextLine("LASERTYPE:","<type of laser>");

	key = in.nextLine("DYELASERNAME:","<dye laser name>"); // get dye laser name
	strncpy(componentName,key,kMaxLen);
	namePtr = noWS(componentName);
	dye = new Dye(namePtr);
	if (!dye) quit("dye","Laser");

	key = in.nextLine("FILTERWHEEL:","<filter wheel name>"); // get dye laser name
	strncpy(componentName,key,kMaxLen);
	namePtr = noWS(componentName);
	filterA = new Filter(namePtr);
	if (!filterA) quit("filterA","Laser");

	key = in.nextLine("FILTERWHEEL:","<filter wheel name>"); // get dye laser name
	strncpy(componentName,key,kMaxLen);
	namePtr = noWS(componentName);
	filterB = new Filter(namePtr);
	if (!filterB) quit("filterB","Laser");

	key = in.nextLine("PRESSURECELL:","<pressure transducer name>"); // get loadcell name
	strncpy(componentName,key,kMaxLen);
	namePtr = noWS(componentName);
	hiPressurecell = (Loadcell *)signOut(new Loadcell(namePtr));
	if (!hiPressurecell) quit("pressurecell","Laser");

	key = in.nextLine("PRESSURECELL:","<pressure transducer name>"); // get loadcell name
	strncpy(componentName,key,kMaxLen);
	namePtr = noWS(componentName);
	lowPressurecell = (Loadcell *)signOut(new Loadcell(namePtr));
	if (!lowPressurecell) quit("pressurecell","Laser");

	laserRate = in.nextFloat("RATE:","<pulse rate>");
	minGasFlow = in.nextFloat("MINGASFLOW:","<min gas flow>");
	minGasPressure = in.nextFloat("MINGASPRESSURE:","<min gas pressure>");
	absMinGasFlow = in.nextFloat("ABSMINGASFLOW:","<min gas flow>");
	absMinGasPressure = in.nextFloat("ABSMINGASPRESSURE:","<min gas pressure>");
	numFrequencies = (int)in.nextFloat("NUM_FREQUENCIES:","<num of frequencies>");
	laserStatus = (LaserStatus)in.nextFloat("LASERSTATUS:","<laser status>");

	key = in.nextLine("CONTROL_MASK:","<laser control mask>");
	controlMask = getMask(key);
	key = in.nextLine("LASEROFF:","<laser off state>");
	offState = getMask(key);
	key = in.nextLine("LASERWARMUP:","<laser warmup state>");
	warmupState = getMask(key);
	key = in.nextLine("LASERREADY:","<laser ready state>");
	readyState = getMask(key);
	key = in.nextLine("LASERRUNNING:","<laser running state>");
	runningState = getMask(key);

	strcpy(hwInStr,name);
	strcat(hwInStr,"POWERSWITCH");
	n2LaserPowerSwitch = dataInput->getHardwareIO(hwInStr);
	if (!n2LaserPowerSwitch) quit("hwInput","n2LaserPowerSwitch");

	strcpy(hwInStr,name);
	strcat(hwInStr,"TRIGGERSWITCH");
	n2LaserTriggerSwitch = dataInput->getHardwareIO(hwInStr);
	if (!n2LaserTriggerSwitch) quit("hwInput","n2LaserTriggerSwitch");

//
	strcpy(hwInStr,name);
	strcat(hwInStr,"DEBUG");
	strcat(hwInStr,"DIGITALHARDWAREIO");
	n2LaserDebug = dataInput->getHardwareIO(hwInStr);
	if (!n2LaserDebug) quit("hwInput","n2LaserDebug");
//
	strcpy(hwInStr,name);
	strcat(hwInStr,"STATUS");
	strcat(hwInStr,"DIGITALHARDWAREIO");
	n2LaserStatus = dataInput->getHardwareIO(hwInStr);
	if (!n2LaserStatus) quit("hwInput","n2LaserStatus");

	strcpy(hwInStr,name);
	strcat(hwInStr,"POWERSTATUS");
	strcat(hwInStr,"DIGITALHARDWAREIO");
	n2LaserPowerStatus = dataInput->getHardwareIO(hwInStr);
	if (!n2LaserPowerStatus) quit("hwInput","n2LaserPowerStatus");

	strcpy(hwInStr,name);
	strcat(hwInStr,"LOCKSTATUS");
	strcat(hwInStr,"DIGITALHARDWAREIO");
	n2LaserLockStatus = dataInput->getHardwareIO(hwInStr);
	if (!n2LaserLockStatus) quit("hwInput","n2LaserLockStatus");

	strcpy(hwInStr,name);
	strcat(hwInStr,"TRIGGERSTATUS");
	strcat(hwInStr,"DIGITALHARDWAREIO");
	n2LaserTriggerStatus = dataInput->getHardwareIO(hwInStr);
	if (!n2LaserTriggerStatus) quit("hwInput","n2LaserTriggerStatus");

	strcpy(hwInStr,name);
	strcat(hwInStr,"WARMUPSTATUS");
	strcat(hwInStr,"DIGITALHARDWAREIO");
	n2LaserWarmupStatus = dataInput->getHardwareIO(hwInStr);
	if (!n2LaserWarmupStatus) quit("hwInput","n2LaserWarmupStatus");

	strcpy(hwInStr,name);
	strcat(hwInStr,"READYSTATUS");
	strcat(hwInStr,"DIGITALHARDWAREIO");
	n2LaserReadyStatus = dataInput->getHardwareIO(hwInStr);
	if (!n2LaserReadyStatus) quit("hwInput","n2LaserReadyStatus");

	strcpy(hwInStr,name);
	strcat(hwInStr,"RUNNINGSTATUS");
	strcat(hwInStr,"DIGITALHARDWAREIO");
	n2LaserRunningStatus = dataInput->getHardwareIO(hwInStr);
	if (!n2LaserRunningStatus) quit("hwInput","n2LaserRunningStatus");

	strcpy(hwInStr,name);
	strcat(hwInStr,"_40V_STATUS");
	strcat(hwInStr,"DIGITALHARDWAREIO");
	n2Laser_40V_Status = dataInput->getHardwareIO(hwInStr);
	if (!n2Laser_40V_Status) quit("hwInput","n2Laser_40V_Status");

	strcpy(hwInStr,name);
	strcat(hwInStr,"_12V_STATUS");
	strcat(hwInStr,"DIGITALHARDWAREIO");
	n2Laser_12V_Status = dataInput->getHardwareIO(hwInStr);
	if (!n2Laser_12V_Status) quit("hwInput","n2Laser_12V_Status");

	strcpy(hwInStr,name);
	strcat(hwInStr,"DYEMOTOR1STATUS");
	strcat(hwInStr,"DIGITALHARDWAREIO");
	n2LaserDyeMotor1Status = dataInput->getHardwareIO(hwInStr);
	if (!n2LaserDyeMotor1Status) quit("hwInput","n2LaserDyeMotor1Status");

	strcpy(hwInStr,name);
	strcat(hwInStr,"DYEMOTOR2STATUS");
	strcat(hwInStr,"DIGITALHARDWAREIO");
	n2LaserDyeMotor2Status = dataInput->getHardwareIO(hwInStr);
	if (!n2LaserDyeMotor2Status) quit("hwInput","n2LaserDyeMotor2Status");

	strcpy(hwInStr,name);
	strcat(hwInStr,"DYEMOTOR3STATUS");
	strcat(hwInStr,"DIGITALHARDWAREIO");
	n2LaserDyeMotor3Status = dataInput->getHardwareIO(hwInStr);
	if (!n2LaserDyeMotor3Status) quit("hwInput","n2LaserDyeMotor3Status");

	strcpy(hwInStr,name);
	strcat(hwInStr,"DYEMOTOR4STATUS");
	strcat(hwInStr,"DIGITALHARDWAREIO");
	n2LaserDyeMotor4Status = dataInput->getHardwareIO(hwInStr);
	if (!n2LaserDyeMotor4Status) quit("hwInput","n2LaserDyeMotor4Status");


	pulser = (Pulser*)signOut("lasertriggerpulser");
	if (!pulser) quit("pulser","N2laser");

	if(setLaserStatus()==kRunning)
	{
	 laserRate = pulser->getRate();
	 if((laserRate<kMinRate)||(laserRate>kMaxRate))
	   rate(kdefaultRate);
	 pulser->run();
	 triggerSwitch = HIGH;
	}
	else
	{
		triggerSwitch = LOW;
		laserRate = 0.0;
		pulser->stop();
	}
	triggerAttempt = 0;
	triggerRequest = LOW;
	lastTime = lastTime2 = RTC->time();  // laser control not polled at all yet
	lastGasPressure = minGasPressure;
	lastGasFlow = minGasFlow;
	sprintf(errorString,"");
}

Laser::~Laser()
{
	delete hiPressurecell;
	delete lowPressurecell;
}

void Laser::monitor(char *outStr)
{
	static char *laserStatusStr[] = { "UNKNOWN","OFF","WARMUP","READY","WAIT","RUNNING","PROBLEM" };
	static char *filterStatusStr[] = { "IDLE","STEP_MODE","STOP_SELECT","TO_TAB_SWITCH","PROBLEM"};
	static char *dyeStatusStr[] = { "IDLE","STEP_MODE","DYE_SELECT","TO_LIMIT_SWITCH","PROBLEM"};
	static char *onOffStat[] = {"OFF","ON"};
	float ndvalue,nd1,nd2;
	int   stopNumA,stopNumB,cellNum;

	stopNumA = filterA->getStopNumber();
	stopNumB = filterB->getStopNumber();
	cellNum  = dye->getCellNumber();
	nd1 = filterA->getNDvalue(stopNumA);
	nd2 = filterB->getNDvalue(stopNumB);
	if((nd1<0.0)||(nd2<0.0)) ndvalue = -1.0;
	else ndvalue = nd1 + nd2;

	if(getLaserStatus()==kRunning)
	{
		if(dye->getCellNumber()==-1) sprintf(errorString,"No dye/N2 laser output aligned");
		else if((stopNumA==-1)||(stopNumB==-1)
					||(stopNumA==0 )||(stopNumB==0))
				sprintf(errorString,"Beam blocked at filter wheels(s)");
		else sprintf(errorString,"Beam on");
	}
	else if(getLaserStatus()==kWait)
		sprintf(errorString,"Trigger delayed - Standby");
	else if(getLaserStatus()==kWarmup)
		sprintf(errorString,"Thyratron warming up - Standby");
	else if(getLaserStatus()==kReady)
		sprintf(errorString,"Laser ready, HV not activated");
	else if(getLaserStatus()==kOff)
		sprintf(errorString,"No 120V power to laser");
	else if(getLaserStatus()==kUnknown)
		sprintf(errorString,"Laser status not determined");
// if laserStatus==kProblem then errorString is already set.

	outStr += sprintf(outStr, "Laser Status: <%.4x> %s \"%s\"\n",
		 iN2LaserDebug,laserStatusStr[getLaserStatus()],errorString);
	outStr += sprintf(outStr, "N2 Pressure: %6.1f ;Flow: %3.0f\n",getLowPressure(),getFlow());
	outStr += sprintf(outStr, "Power: 120V-%s 12V-%s 40V-%s\n",
		 onOffStat[iN2LaserPowerStatus],onOffStat[iN2Laser_12V_Status],onOffStat[iN2Laser_40V_Status]);
	outStr += sprintf(outStr, "Filter1: %d-%s ;Filter2: %d-%s ;ND: %5.2f\n",
		 stopNumA,filterStatusStr[filterA->getFilterMode()],stopNumB,filterStatusStr[filterB->getFilterMode()],ndvalue);
	outStr += sprintf(outStr, "Dye Laser Cell: %d ;Wavelength: %3.0f ;Status: %s\n",
		 cellNum,dye->getWavelength(cellNum),dyeStatusStr[dye->getDyeMode()]);
	outStr += sprintf(outStr, "Stir Motors: %s-%s-%s-%s\n",
		 onOffStat[iN2LaserDyeMotor1Status],onOffStat[iN2LaserDyeMotor2Status],
		 onOffStat[iN2LaserDyeMotor3Status],onOffStat[iN2LaserDyeMotor4Status]);
	if(pulser->isRunning())
		outStr += sprintf(outStr, "Trigger rate: %5.1f ON\n",laserRate);
	else
		outStr += sprintf(outStr, "Trigger rate: %5.1f OFF\n",laserRate);
}

void Laser::poll(void)
{
	double t = RTC->time();  // get real time

	if ((t-lastTime) > serviceLogicPeriod) // service trigger logic once every 2 seconds
	{
		 lastTime = t;
		 laserRate = pulser->getRate();
		 setLaserStatus();
/* This is just for debugging
		 static char *laserStatusStr[] = { "UNKNOWN","OFF","WARMUP","READY","WAIT","RUNNING","PROBLEM" };
		 sprintf(display->outString,"RStat: %04.4x, LStat: %s, N2P: %f, HiP: %f, N2F: %f",
				iN2LaserDebug,laserStatusStr[getLaserStatus()],getLowPressure(),getHiPressure(),getFlow());
		 display->messageB(1,20);
*/
// Check if laser is running, then if the gas pressure and flow are okay
		 if((getLaserStatus()==kReady)||(getLaserStatus()==kRunning))
		 {
// Lag values to smooth out occasssional dips
				lastGasPressure += 0.5*(getLowPressure()-lastGasPressure);
				lastGasFlow += 0.5*(getFlow()-lastGasFlow);
				if((getLowPressure()<absMinGasPressure)||(getFlow()<absMinGasFlow)||
				(    lastGasPressure<minGasPressure   )||(lastGasFlow<minGasFlow))
				{
					 triggerRequest = LOW;
					 triggerLaser(LOW);
					 laserStatus = kProblem;
					 sprintf(errorString,"%s low gas flow or pressure",getObjectName());
					 display->error(errorString);
				}
		 }

		 if(triggerRequest)
		 {
			 if(getLaserStatus()==kWait)
			 {
				 if ((t-waitTime) > waitToTriggerPeriod)
				 {
					 triggerLaser(HIGH);
					 waitTime = t;    //force a re-wait in case the trigger fails
				 }
			 }
		 }


	}

	if ((t-lastTime2) > updateDatabasePeriod) // update database once every ten seconds
	{
		lastTime2 = t;
		if (needUpdate()) updateDatabase(); // this writes new lengths and zeros to database
	}

}

int Laser::isBlocked()
{
	return(filterA->getStopNumber()==0 || filterB->getStopNumber()==0);
}

void Laser::block()
{
	filterA->position(0);
	filterB->position(0);
}

LaserStatus Laser::setLaserStatus(void)
{
	iN2LaserDebug           = n2LaserDebug->read();
	iN2LaserStatus          = n2LaserStatus->read();
	iN2LaserPowerStatus     = n2LaserPowerStatus->read();
	iN2LaserLockStatus      = n2LaserLockStatus->read();
	iN2LaserTriggerStatus   = n2LaserTriggerStatus->read();
//	iN2LaserWarmupStatus    = n2LaserWarmupStatus->read();
//	iN2LaserReadyStatus     = n2LaserReadyStatus->read();
//	iN2LaserRunningStatus   = n2LaserRunningStatus->read();
	iN2Laser_40V_Status     = n2Laser_40V_Status->read();
	iN2Laser_12V_Status     = n2Laser_12V_Status->read();
	iN2LaserDyeMotor1Status = n2LaserDyeMotor1Status->read();
	iN2LaserDyeMotor2Status = n2LaserDyeMotor2Status->read();
	iN2LaserDyeMotor3Status = n2LaserDyeMotor3Status->read();
	iN2LaserDyeMotor4Status = n2LaserDyeMotor4Status->read();

//If there is a problem with laser then don't change
//status until it changes to the off state
	if((laserStatus!=kProblem)||(iN2LaserStatus==offState))
	{
	 if     (iN2LaserStatus==offState)     laserStatus = kOff;
	 else if(iN2LaserStatus==warmupState)  laserStatus = kWarmup;
	 else if(iN2LaserStatus==readyState)   laserStatus = kReady;
	 else if(iN2LaserStatus==runningState) laserStatus = kRunning;
	 else                                  laserStatus = kUnknown;

//laser is warmed up and waiting to charge the trigger circuit
	 if((iN2LaserStatus==readyState)&&triggerRequest)
	 {
		 laserStatus = kWait;
	 }
	}

//if laser is running then trigger request can now be cancelled
	if((iN2LaserStatus==runningState)&&triggerRequest)
	{
		triggerAttempt = 0;
		triggerRequest = LOW;
	}

	return laserStatus;
}


unsigned int Laser::getMask(char *fileline)
{
		unsigned int mask;

		sscanf(fileline,"%s %s ",substr1,substr2);
		if((substr1[0]!='0')||(substr1[1]!='X')) {
			printw("Mask must be a hex number in LASER.DAT:  %s\n",fileline);
			quit();
		}
		substr1[1] = 'x';
		sscanf(substr1,"%i \n",&mask);
		return mask;
}


void Laser::powerOn(void)
{
// only allow power-up if laser is settled in the power-down state
		if((!iN2LaserPowerStatus)&&(getLaserStatus()==kOff))
		{
			n2LaserPowerSwitch->on();   //old		hwInput->active1();
			triggerSwitch = LOW;
			triggerAttempt = 0;
			triggerRequest = LOW;
		}
}

void Laser::powerOff(void)
{
// immediate power-down
	n2LaserPowerSwitch->off();      //old   hwInput->reset();
	triggerSwitch = LOW;
	triggerAttempt = 0;
	triggerRequest = LOW;
	pulser->stop();
}

void Laser::start(float pulserate=kdefaultRate)
{
//	n2LaserTriggerSwitch->on(); //For debugging only
		triggerRequest = HIGH;
		waitTime = RTC->time();
		if((pulserate>kMaxRate)||(pulserate<kMinRate)) pulserate=kdefaultRate;
		pulser->setRate(pulserate);
		pulser->run();
}

void Laser::stop(void)
{
//only toggle trigger if laser is definately in the running state,
//cancel any impending trigger request.
//	n2LaserTriggerSwitch->off();
		if(iN2LaserStatus==runningState) triggerLaser(LOW);
		triggerRequest = LOW;
		pulser->stop();
}

int Laser::triggerLaser(int trigger) //laser trigger toggles, so must keep track of position
{
// if trigger switch is already high then attempt re-trigger
	if((triggerSwitch==trigger)&&(triggerSwitch==HIGH))
	{
		triggerAttempt++;
		if(triggerAttempt==maxTriggerAttempts)
		{
			laserStatus = kProblem;
			powerOff();
			sprintf(errorString,"Cannot start %s after %d attempts",getObjectName(),maxTriggerAttempts);
			display->error(errorString);
		}
	}

/* toggle trigger switch on or Off.
	 Note: This may seem complicated, tracking the high and low
	 positions of a toggle switch when the actual function calls
	 are simply ON and OFF. Don't be fooled! You see the hardware
	 output through the counterboard really is a toggled bit.
	 So that the functions (in HWIO) n2LaserTriggerSwitch->on()
	 and n2LaserTriggerSwitch->off() actually do exactly the
	 same thing - they just toggle. So we DO need to keep track
	 of the switch as a toggled switch. In the future the ON and
	 OFF may actually be real ON and OFFs if we get new hardware.

	 For example, if you called ON twice, then the second call
	 to ON would actually be an OFF. And if the next call was
	 a call to OFF then it would really be turning on.
	 So be carefull -- use the triggerswitch, not the ON and OFF
	 (until such future date as the ON and OFF is not toggled).
*/

	if((triggerSwitch!=trigger)||(triggerSwitch==HIGH))
	{
// request trigger from low to high ie. turnon
		if(trigger==HIGH)
		{
			n2LaserTriggerSwitch->on();  //toggled ie. just the same as off
			triggerSwitch = HIGH;
		}
// request trigger from high to low ie. turnoff
		else
		{
			n2LaserTriggerSwitch->off(); //toggled ie. just the same as on
			triggerSwitch = LOW;
		}
	}
	return triggerSwitch;
}


float Laser::getFlow(void)
{
	 float PHi = getHiPressure();
	 float PLow = getLowPressure();
	 return flowCoeff1*(PHi-PLow)+flowCoeff2;
// I expected the flow to go like squareroot, but a linear fit
// is much better, so I'll use a linear fit instead!
//	 if(PHi>PLow)
//		 return flowCoeff*sqrt(PHi-PLow);
//	 else
//		 return -flowCoeff*sqrt(PLow-PHi);
}

void Laser::hiPressure(void)
{
		 sprintf(display->outString,"High Pressure: %f    ", getHiPressure());
		 display->message();
}

void Laser::lowPressure(void)
{
		 sprintf(display->outString,"Low Pressure: %f    ", getLowPressure());
		 display->message();
}

void Laser::gasFlow(void)
{
		 sprintf(display->outString,"Gas Flow: %f   ", getFlow());
		 display->message();
}

void Laser::setND(float ndvalue=0.0)
{
		 int numStopsA=filterA->getNumStops();
		 int numStopsB=filterB->getNumStops();
		 int i,stopA,stopA2,stopB;
		 float ndtest,ndrem=0.0,diff=0.0;

// Find closest filter on wheel A and closest that is smaller
		 for(i=0,stopA=-1,stopA2=0,diff=fabs(ndvalue-filterA->getNDvalue(0));i<numStopsA;i++)
		 {
				ndtest = filterA->getNDvalue(i);
				if(ndtest<=ndvalue)              //closest and smaller
				{
					if((stopA==-1)||((ndvalue-ndtest)<ndrem))
					{
						ndrem = ndvalue-ndtest;
						stopA = i;
					}
				}
				if(fabs(ndvalue-ndtest)<diff)    //closest
				{
					diff = fabs(ndvalue-ndtest);
					stopA2 = i;
				}
		 }
		 if(stopA==-1)   //if none smaller, take closest
		 {
				ndrem = diff;
				stopA = stopA2;
		 }
// Find closest filter on wheel B for the remainder
		 for(i=0,stopB=0,diff=fabs(ndrem-filterB->getNDvalue(0));i<numStopsB;i++)
		 {
				ndtest = filterB->getNDvalue(i);
				if(fabs(ndrem-ndtest)<diff)
				{
					 diff = fabs(ndrem-ndtest);
					 stopB = i;
				}
				filterA->position(stopA);
				filterB->position(stopB);
		 }
		 return;
}

void Laser::rate(float pulserate=kdefaultRate)
{
		 if((pulserate<=kMaxRate)&&(pulserate>=kMinRate))
			 pulser->setRate(pulserate);
		 laserRate = pulser->getRate();

		 updateDatabase();
}

void Laser::updateDatabase(void)
{
	// this is a little bit simpler now :)  - PH 02/13/97
	OutputFile out("LASER",getObjectName(),1.00);	// create output object

	out.updateFloat("LASERSTATUS:",(float)getLaserStatus());	// update length
	out.updateFloat("RATE:",laserRate);	// update length

	dbaseStatus = getLaserStatus();	// save the database length
}
