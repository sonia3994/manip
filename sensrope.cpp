// For comments see .h file. C. Jillings, Sept, 1996
#include "display.h"
#include "sensrope.h"
#include "datain.h"
#include "ioutilit.h"
//PH#include <dos.h>

extern Display *display;
extern DataInput *dataInput;

const float kMaxLengthChangePerPoll = 1.0; // centimeters


SenseRope::RawADC SenseRope::cmd_rawADC;
SenseRope::DisplaySlope  SenseRope::cmd_slope;
SenseRope::DisplayOffset SenseRope::cmd_offset;


Command *SenseRope::commandList[SENSEROPE_COMMAND_NUMBER] = {
	&cmd_rawADC,&cmd_slope,&cmd_offset
};


SenseRope::SenseRope(char* name):          // rope name
	Controller("SenseRope",name) // H/w template
{
	mName = new char[strlen(name)+1];
	strcpy(mName,name);
	// open input file and find entry
	InputFile in("SENSROPE", name, 1.00);

	// initialize variables from database (re-written PH 02/20/97)
	snout			 = in.nextLine("SNOUT:","<cm>");
//	neckAttach = in.nextLine("NECK_ATTACH:","<cm>");
//	delta 		 = in.nextLine("DELTA:","<cm>");
	ADCZero    = in.nextFloat("ADC_ZERO:","<chan>");
	twelveInch = in.nextFloat("TWELVE_INCH:","<chan>");
	twentyfourInch = in.nextFloat("TWENTYFOUR_INCH:","<chan>");

	// calculate the slope and zero offset
	if(strstr(name,"SROPE2")) {
		slope = (8)*2.54/(twentyfourInch - twelveInch);
	}
	else {
		slope = (11+5.8/16)*2.54/(twentyfourInch - twelveInch);
	}
	zeroOffset = 0.0;

	// Get the hardware

	char HWInStr[100];
	strcpy(HWInStr,name);
	strcat(HWInStr,"BCDHARDWAREIO");
	//hwInput = dataInput->getHardwareIO(HWInStr); // Do *not* reset
	originalLength = 0.0;
	// There must be a poll on construction to ensure data is ready
	poll();
} // end of database constructor for sense ropes.


ThreeVector SenseRope::getSnout()
{
	ThreeVector retvar(snout); // copy constructor
	return retvar;
}

void SenseRope::setSnout(ThreeVector aSnout) {
	snout = aSnout;

}

//ThreeVector SenseRope::getNeckAttach()
//{
//	ThreeVector retvar(neckAttach);
//	return retvar;
//}

//ThreeVector SenseRope::getDelta()
//{
//	ThreeVector retvar(delta);
//	return retvar;
//}

float SenseRope::getDeltaADC()
{
	return deltaADC;
}

float SenseRope::getDeltaL()
{
	return deltaL;
}

void SenseRope::poll() {
	// the hardware read function reorganizes the BCD into something sensible

}

void SenseRope::setZeroOffset(ThreeVector aNA) {

	float lLength = (aNA-snout).norm();
	originalLength = lLength;  // yeah, a waste of a line, but who cares?
	zeroOffset = lLength - slope*ADCZero;


}


unsigned SenseRope::getUncalibrated()
{
	return uncalibrated;
}

void SenseRope::setLength(float newLength)
{
	length = newLength;
	deltaL = length - originalLength;
}

float SenseRope::getLength()
{
	return length;
}

float SenseRope::getSlope(void)
{
	return slope;
}

float SenseRope::getADCZero()
{
	return ADCZero;
}

float SenseRope::getZeroOffset()
{
	return zeroOffset;
}

float SenseRope::getRawADC(float *outSigma)
{
    return 0;//TEST
}

void SenseRope::rawADC(char* aString)
{

/*
	int i = 0;
	for(i=0;i<24;i++) {
		printw("%d\n",hwInput->read());
		delay(1);
	}
*/
    float lSigma;
    float lAverage = getRawADC(&lSigma);
	sprintf(display->outString,
					"%s Average = %7.2f\t Standard Deviation = %7.3f  (20 trials)",
					this->mName,lAverage,lSigma);
	display->messageB(1,10);


}

void SenseRope::displaySlope(char* aString) {
	sprintf(display->outString,"%s slope is %f",this->mName,slope);
	display->messageB(1,10);
}

void SenseRope::displayOffset(char* aString) {
	sprintf(display->outString,"%s zero Offset is %f",this->mName,zeroOffset);
	display->messageB(1,10);
}

