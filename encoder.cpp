#include "encoder.h"
#include "hwio.h"
#include "datain.h"
#include "global.h"
#include "ieee_num.h"

/* R. Ford 14/4/97 -- Added counter roll-over tracking. */

extern DataInput *dataInput;

const int		kMaxCountDifference	= 0x10 * ENCODER_SUB_STEP;
const int		kMaxCountRetries		= 3;

// static member declarations
Encoder::Read						Encoder::cmd_read;
Encoder::ReadLoop				Encoder::cmd_readloop;
Encoder::StopLoop				Encoder::cmd_stoploop;
Encoder::SetPosition		Encoder::cmd_setposition;

Command* Encoder::commandList[ENCODER_COMMAND_NUMBER] = {
	&cmd_read,
	&cmd_readloop,
	&cmd_stoploop,
	&cmd_setposition
};


Encoder::Encoder(char* name):          // encoder name
	Controller("Encoder",name) // H/w template
{
	float		theLength;

	// initialize for reading of parameters
	InputFile in("ENCODER",name,2.00);

	cm_per_count = in.nextFloat("SLOPE:","<cm/pulse>");
	theLength = in.nextFloat("POSITION:","<current encoder position>");

	char hwInStr[100];
	strcpy(hwInStr,name);
	char *a = strstr(hwInStr,"ENCODER");
	if (a) *a = '\0';

	strcat(hwInStr,"DIGITALHARDWAREIO");
	hwInput = dataInput->getHardwareIO(hwInStr);

	loopFlag = 0; // if loopFlag is non-zero position displayed in O/P location

	lastTime = RTC->time();		 // encoder not polled at all yet
	dbaseLength = theLength;   // length for database update test in poll()

	rollOver = 0;
	if (hwInput) {
    	uncalibrated = hwInput->read();		// read initial counter value
    } else {
        uncalibrated = 0;
    }
	setPosition(theLength);						// calculate initial zeroLength
}

void Encoder::monitor(char *outStr)
{
	outStr += sprintf(outStr, "Position: %.2f\n", length());
	outStr += sprintf(outStr, "Raw digital: 0x%.4x\n",uncalibrated);
	outStr += sprintf(outStr, "Status: 0x%.4x\n",getStatus());
//	outStr += sprintf(outStr, "Zero length: %.2f\n", zeroLength);
	hwInput->monitor(outStr);
}

void Encoder::poll(void) // poll encoder -- just does some maintenance on database
{
	int i;
	char *pt;
	unsigned counts[kMaxCountRetries];

    if (!hwInput) return;

	double t = RTC->time();  // get real time

	if ((t-lastTime) > 10) // update database once every ten seconds
	{
		lastTime = t;
		if (needUpdate()) updateDatabase(); // this writes new lengths and zeros to database
	}

	// save value of last readout
	unsigned last_count = uncalibrated;

	// added retries - PH 08/13/99
	for (int retry=0;;) {

		uncalibrated = hwInput->read();

		// calculate change in counts
		short diff = (int)(uncalibrated - last_count);

		if (diff < 0) diff = -diff;	// take absolute value

		if (diff < kMaxCountDifference) {
			if (retry) {
				pt = display->outString;
				pt += sprintf(pt,"%s %d retries (0x%.4x->0x%.4x",
											getObjectName(), retry, last_count, counts[0]);
				for (i=1; i<retry; ++i) {
					pt += sprintf(pt,",0x%.4x",counts[i]);
				}
				sprintf(pt,",0x%.4x)",uncalibrated);
				display->error();
			}
			break;
		}
		// record readings
		counts[retry] = uncalibrated;

		if (++retry >= kMaxCountRetries) {
			pt = display->outString;
			pt += sprintf(pt,"%s %d bad reads (0x%.4x->0x%.4x",
										getObjectName(), retry, last_count, counts[0]);
			for (i=1; i<retry; ++i) {
				pt += sprintf(pt,",0x%.4x",counts[i]);
			}
			sprintf(pt,")");
			display->error();
			break;
		}
	}

	// Added roll-over counter - R. Ford 14/4/97
	if((uncalibrated<0x4fff)&&(last_count>0xcfff)) rollOver += 0x10000L;
	if((uncalibrated>0xcfff)&&(last_count<0x4fff)) rollOver -= 0x10000L;

	if (loopFlag) // flag set by readloop command, cleared by stoploop
	{
//    digitalChannel->poll();
		sprintf(display->outString,"%s Length: %f",getObjectName(),length());
		display->message();
	}
}

void Encoder::setPosition(float length)
{
	// recalculate zero length for new position
	zeroLength = length + cm_per_count*(uncalibrated+rollOver);
}

float Encoder::length(void) // return last read encoder length
{
// Recoded R. Ford 14/4/97 to keep track of counter roll-overs
	lengthValue = zeroLength - cm_per_count*(uncalibrated+rollOver);
	return lengthValue;
}

void Encoder::updateDatabase(void)
{
	// this is a little bit simpler now :)  - PH 02/13/97

	OutputFile out("ENCODER",getObjectName(),2.00);	// create output object

	out.updateFloat("POSITION:", length());	// update length

	dbaseLength = length();	// save the database length
}
