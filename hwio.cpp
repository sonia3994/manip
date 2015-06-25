// See .h file for comments. CJJ, April, 1997

//PH#include <conio.h>
//#include <iostream.h>

#include "hwio.h"
#include "display.h"
#include "datain.h"
#include "global.h"


extern Display *display;
extern DataInput *dataInput;

const double kWatchdogErrorTime	= 0.250;	// maximum seconds between watchdog calls

int CBAnalogIO::sWatchdogErrorCount = 0;

// Comment out this line when the fast hardware is in place.
//#define DOUBLE_READ

//#define NO_STATUS_READ


HardwareIO::HardwareIO(const char *aLabel, AVR32 *avr, char *aChan)
    : AVRChannel(avr, aChan)
{
	mLabel = new char[strlen(aLabel)+1];
	strcpy(mLabel,aLabel);
	mReadable = 0;
	mWriteable = 0;
	mSwitchable = 0;
}

HardwareIO::~HardwareIO()
{
	delete[] mLabel;
}

unsigned int HardwareIO::read()
{
	unsigned int returnVariable;
	if(mReadable) returnVariable = read1();
	else {
		sprintf(display->outString,"Tried to read to an unreadable object, returning 0.");
		display->message();
		sprintf(display->outString,"Label: %s",mLabel);
		display->message();
		returnVariable = 0;
	}
	return returnVariable;
}

void HardwareIO::write(int aOutput)
{
	if(mWriteable) write1(aOutput);
	else {
		sprintf(display->outString,"Tried to write to an unwriteable object.");
		display->message();
		sprintf(display->outString,"Label: %s",mLabel);
		display->message();
	}
}

void HardwareIO::on(void)
{
	if(mSwitchable) on1();
	else {
		sprintf(display->outString,"Tried to turn on an unswitchable object.");
		display->message();
		sprintf(display->outString,"Label: %s",mLabel);
		display->message();
	}
}

void HardwareIO::off(void)
{
	if(mSwitchable) off1();
	else {
		sprintf(display->outString,"Tried to turn off an unswitchable object.");
		display->message();
		sprintf(display->outString,"Label: %s",mLabel);
		display->message();
	}
}




int HardwareIO::isLabel(const char* aLabel)
{
	int k = strcmp(aLabel,mLabel);
	if(k==0) return 1;   // strcmp is screwy. Read man page before changing.
	else return 0;
}

CBDigitalIO::CBDigitalIO(char* aLabel, AVR32 *avr, char *aChan)
    :HardwareIO(aLabel,avr,aChan)
{
	mReadable = 1;
	mWriteable = 0;
	mMask = 0;
	mMaster = NULL;
	mIsShared = 0;
	mStatus = 0;
	mWasError = 0;
	mNumDigital = 0;
	mDigital[0] = '\0';
}

// add digital i/o channel
int CBDigitalIO::addDigital(char *chan)
{
    if (strlen(chan) + strlen(mDigital) + 1 >= kMaxDigLen) return -1;
    if (mNumDigital) strcat(mDigital, ";");
    strcat(mDigital, chan);
    ++mNumDigital;
    return 0;
}
unsigned int CBDigitalIO::read1(void)
{
    const int kBuffSize = 1024;
    char buff[kBuffSize];
    int err = 0;
    unsigned val = 0;

    // prepare our command (ie. "c0;s00;s01;s02;s03");
    strcpy(buff, mChannel);
    if (mNumDigital) {
        if (mChannel[0]) strcat(buff,";");
        strcat(buff,mDigital);
    }
    if (!mAVR->sendCommand(buff,kBuffSize)) {
        // response should be something like this:
        // "OK c0 VAL=0 (0x0000)\nOK s00 VAL=1\nOK s01 VAL=0\nOK s02 VAL=1\nOK s03 VAL=0\n\0"
        char *pt = strstr(buff, "VAL=");
        if (pt) {
            val = atoi(pt+4);
            pt += 4;
        } else {
            err = 2;
        }
        // read digital inputs
        mStatus = 0;
        for (int i=0; i<mNumDigital; ++i) {
            pt = strstr(pt, "VAL=");
            if (!pt) {
                err = i + 3;
                break;
            }
            if (pt[4] == '1') {
                mStatus |= (1 << i);
            } else if (pt[4] != '0') {
                err = 3;
                break;
            }
            pt += 5;
        }
    } else {
        err = 1;
    }
    if (err) {
        if (!mWasError) {
            eprintf("%s read error %d", getLabel(), err);
            mWasError = 1;
        }
    } else {
        if (mWasError) {
            eprintf("%s OK again", getLabel());
            mWasError = 0;
        }
    }
    return val;
}


void CBDigitalIO::write1(int aNumberWritten)
{
	; // this is a dummy. Counterboards are not writeable.
		// Derive a class if you want it writeable. CJJ.
}

inline void CBDigitalIO::reset(void)
{
    // stub
}

CBbcdIO::CBbcdIO(char* aLabel, AVR32 *avr, char *aChan)
    :CBDigitalIO(aLabel,avr,aChan)
{
}

unsigned int CBbcdIO::read1(void)
{
    char *pt;
    if (!avrCommand() && (pt=strstr(avrResponse(),"VAL=")) != NULL) {
        return atoi(pt+4);
    } else {
        return 0;
    }
}



CBAnalogIO::CBAnalogIO(char* aLabel, AVR32 *avr, char *aChan)
    : HardwareIO(aLabel,avr,aChan)
{
	mWriteable = 0;
	mReadable = 1;
#ifdef DOUBLE_READ
	static int	gaveMsg = 0;
	if (!gaveMsg) {
		gaveMsg = 1;
		display->message("Double reads turned on.");
	}
#endif
}

void CBAnalogIO::write1(int aI)
{
	; // never used. unwriteable
}

unsigned int CBAnalogIO::read1(void)
{
    char *pt;
    if (!avrCommand() && (pt=strstr(avrResponse(),"VAL=")) != NULL) {
        return atoi(pt+4);
    } else {
        return 0;
    }
}

// The watchdog is now called once per polling loop: CJJ May, 1997
// Modified to work with rls io board, MHD August, 1997
void CBAnalogIO::watchdog(void)
{
}



NBitWrite::NBitWrite(char* aLabel,int aNumberOfBits,int * aPtrChannelsUsed,
					 HardwareCard aCardUsed): HardwareIO(aLabel)
{
	mReadable = 0;
	mWriteable = 1;
	mNumberOfBits = aNumberOfBits;
	mChannelsUsed = new int[mNumberOfBits];
	int i;
	for(i=0;i<mNumberOfBits;i++)
				mChannelsUsed[i] = aPtrChannelsUsed[i];
	mCardUsed = aCardUsed;

}

NBitWrite::~NBitWrite(void)
{
	delete[] mChannelsUsed;
}

unsigned int NBitWrite::read1(void)
{
	return 0; // a dummy. Unreadable
}

void NBitWrite::write1(int aOutput)
{
	dataInput->digitalWriter(mCardUsed,aOutput, mNumberOfBits, mChannelsUsed);
}


BitSwitch::BitSwitch(char* aLabel,int* aPtrChannelsUsed,HardwareCard aCardUsed)
					 :NBitWrite(aLabel,1,aPtrChannelsUsed,aCardUsed)
{
	mSwitchable = 1;
}

void BitSwitch::on1(void)
{
	write1(1);
}

void BitSwitch::off1(void)
{
	write1(0);
}


CBLaserPowerIO::CBLaserPowerIO(char* aLabel, AVR32 *avr, char *aChan)
							 :CBDigitalIO(aLabel,avr,aChan)
{
	mReadable = 0;
	mWriteable = 0;
	mSwitchable = 1;
}

void CBLaserPowerIO::on1(void)
{
    // stub
}

void CBLaserPowerIO::off1(void)
{
    // stub
}

CBLaserTriggerIO::CBLaserTriggerIO(char* aLabel,AVR32 *avr,char *aChan)
								 :CBDigitalIO(aLabel,avr,aChan)
{
	mReadable = 0;
	mWriteable = 0;
	mSwitchable = 1;
}

void CBLaserTriggerIO::on1(void)
{
    // stub
}

void CBLaserTriggerIO::off1(void)
{
    // stub
}

