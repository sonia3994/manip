//
// For getting the hardware config into the manipulator control code.
// The hardware wiring is noted by the user in DATACON.DAT. (See notes
// in that file for particulars.) This file describes a class called data
// input which is only instantiated ONCE. This causes the file to be read
// and arrays of pointers to HWInput, DCChannel, HWDigInput, and HWAnInput
// to be created and filled according to the file. The HWDigInput labels
// a suffix "ENCODER"; the analog objects have a suffix "LOADCELL".
// There are a couple of public routines to get the pointer to the class
// given the label.
//
//	Chris Jillings, May, 1996
//

#ifndef __DATAIN_H
#define __DATAIN_H
#include "hwio.h"
#include "motor.h"
#include "pulser.h"
#include "avr32.h"

enum CBparamLabels {
    eAVRName,
	eCounterName,
	eADCName,
	eMotorName,
	eDigital,
	kMaxParams
};


class DataInput {
	void ReadFile(void);               // hard coded to be DATACON.DAT
	int parser(char* line,char* s); // handles one line of the file
	void cb(FILE* fin, char *name);  // handles read of cb info if
	void clearParams();
	// These are for the hardware inputs
	AVR32 *theAVR;
	HardwareIO **hardwareList;    // The list of stuff that can be read
	CBAnalogIO **cbAnalogList;      // ditto
	CBDigitalIO **cbDigitalList;    // ditto
	CBbcdIO **cbbcdList;    // BCD is binary coded decimal
	NBitWrite **nBitWriteList;
	BitSwitch **bitSwitchList;
	CBLaserPowerIO **cbLaserPowerList;
	CBLaserTriggerIO **cbLaserTriggerList;
	AVR32 **avrList;

	int numHL; 						// the number of elements in hardwareList
	int numAn;                 //									  DCAnList
	int numDig;                //									  DCDigList
	int numBCD;                //                   DCbcdList
	int numNBitWrite;
	int numBitSwitch;
	int numCBLT;
	int numCBLP;
	int numAVR;

	Motor **motorList;
	Hardware **otherList;
	int numMot;
	int numOther;

	// These are used for both motors and data concentrators
	char *label;
	char params[kMaxParams][100];

	unsigned mWritePCL750;

public:

	HardwareIO* getHardwareIO(char *s); // returns pointer to obj labelled by s
	DataInput(void);                // constructor; also reads file
															// exit to system on failure
	void digitalWriter(HardwareCard aCardUsed, unsigned aOutput,
										 int mNumberOfBits, int *mChannelsUsed);
	void bitWrite(HardwareCard cardUsed, int aBitNumber, int aBitValue);
	void outputDigitalToCards(void);
	int initAVRs(int newMsg=1);     // initialze AVR devices
	void doneInit();
	void keepAlive();
	~DataInput();
};




#endif
