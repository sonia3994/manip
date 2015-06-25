//
//	The class HardwareIO is responsible for reading and writing. 
//	The read() and write(int i) member functions are puree virtual. For those
// 	hardware types that do not support one or the other use the Display class
//  to write to screen.
//
//	All hardware that does IO, except motors is to be derived from
//	HardwareIO. All objects are identified with a character string.
//	It is part of the base class along with int isLabel(const char *)
//
//	Chris Jillings, April, 1997
//

#ifndef __HWIO_H
#define __HWIO_H

//#include <dos.h>
#include <string.h>
//#include <process.h>
#include <stdio.h>
#include "avrchannel.h"

class AVR32;

const int kMaxDigLen = 64;  // maximum length of digital I/O channel list

enum HardwareCard {
	ePCL75,
	eTIO10_1,
	eTIO10_2
};


class HardwareIO: public AVRChannel {
private:
	char *mLabel;
protected:
	int mWriteable;
	int mReadable;
	int mSwitchable;
	virtual unsigned int read1(void)=0;
	virtual void write1(int i)=0;
	virtual void on1(void) {}
	virtual void off1(void) {}
public:
	void write(int aOutput);
	unsigned read(void);
	virtual unsigned getStatus(void) { return 0; }
	void on(void);
	void off(void);
	HardwareIO(const char *label, AVR32 *avr=NULL, char *aChan=NULL);
	~HardwareIO();
	int isLabel(const char *aLabel);
	char *getLabel() { return mLabel; }
	virtual void reset() {}
	virtual void monitor(char *outStr) { AVRChannel::monitor(outStr); }
};

class CBDigitalIO: public HardwareIO {
private:
	CBDigitalIO* mMaster; // if shared, the first in list.
protected:
	int mDCboard;      		// number of board in DC chassis, 0-7
	int mDataRegister;
	int mControlRegister;
	int mIsShared;
	int mReadRegister;
	int mWriteRegister;
	int mWasError;				// flag set if there was an error on the last read
	unsigned int mMask;
	unsigned mLastValueRead;
	unsigned mStatus;
	char mDigital[kMaxDigLen];
	int mNumDigital;
public:
	CBDigitalIO(char* aLabel, AVR32 *avr, char *aChan);
	int addDigital(char *name);
	unsigned int read1();
	void write1(int aOutput);
	virtual unsigned getStatus(void) { return mStatus; }
	void reset(void);
	unsigned int  getLVR(void);  // returns the value of the last read.
};

inline unsigned int CBDigitalIO::getLVR(void)
{
	return mLastValueRead;
}

class CBbcdIO: public CBDigitalIO {

public:
	CBbcdIO(char* aLabel,AVR32 *avr,char *aChan);
	unsigned int read1();


};


class NBitWrite:public HardwareIO {
private:
	int mNumberOfBits;
	int* mChannelsUsed;
	HardwareCard mCardUsed;
	int mCardAddress;

public:
	NBitWrite(char* aLabel,int aNumberOfBits,int * aPtrChannelsUsed,
						HardwareCard aCardUsed);
	~NBitWrite(void);
	void write1(int aOutput);
	unsigned int read1(void);

};




class BitSwitch: public NBitWrite {
public:
	BitSwitch(char *aLabel, int* aPtrChannelUsed, HardwareCard aCardUsed);
	void on1(void);
	void off1(void);
};




class CBLaserPowerIO: public CBDigitalIO
{
private:
	int mIsOn; // is the laser powered up?
	void on1(void);
	void off1(void);
public:
	CBLaserPowerIO(char* aLabel, AVR32 *avr, char *aChan);
};

class CBLaserTriggerIO: public CBDigitalIO
{
private:
	int mIsOn; // is the laser powered up?
	void on1(void);
	void off1(void);
public:
	CBLaserTriggerIO(char* aLabel, AVR32 *avr, char *aChan);
};



class CBAnalogIO: public HardwareIO {
private:
	int mDCboard;      		// number of board in DC chassis, 0-7
	int mDataRegister;
	int mAddressRegister;
	int mControlRegister;
  int mWriteRegister; // rls io board
	int mWatchdogRegister;
	static int sWatchdogErrorCount;
public:
	void watchdog(void);

	CBAnalogIO(char* aLabel, AVR32 *avr, char *aChan);
	unsigned int read1(void);
	void write1(int aI);

	static int getWatchdogErrorCount()	{ return sWatchdogErrorCount; }
};



#endif // !HWIO_HH_
