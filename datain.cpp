#include "hwio.h"
#include <stdio.h>
#include <stdlib.h>
#include "ioutilit.h"
//PH#include <conio.h>
#include <string.h>
//#include <iostream.h>
#include "motor.h"
#include "l_motor.h"
#include "pulser.h"
#include "lockout.h"
#include "datain.h"
#include <ctype.h>
#include "global.h"

const int listSize = 128;   // The size of the arrays
const int aboxSize = 16;	// The number of channels in Alvin's box
const int kInvalidParam = -0xaaaa; // all params are set to this
					 // before reading parameters for each hwio object

DataInput::DataInput(void) {
	label = new char[100];
	// Data concentrators
	hardwareList = new HardwareIO*[listSize];
	cbAnalogList = new CBAnalogIO*[listSize];
	cbDigitalList = new CBDigitalIO*[listSize];
	cbbcdList = new CBbcdIO*[listSize];
	cbLaserPowerList = new CBLaserPowerIO*[listSize];
	cbLaserTriggerList = new CBLaserTriggerIO*[listSize];
    avrList = new AVR32*[listSize];

	numHL = numAn = numDig = numBCD = numCBLP = numCBLT = numAVR = 0;
	numNBitWrite = numBitSwitch = 0;
	theAVR = NULL;
	// Motors
	motorList = new Motor*[listSize];
	otherList = new Hardware*[listSize];
	numMot = 0;
	numOther = 0;

	clearParams();

	// initialize the write words to 0.
	mWritePCL750 = 0L;

	ReadFile();
}



DataInput::~DataInput() {
	int i;
	for(i=0;i<numAn;i++) {  //numAn < 100 or constructor exits to system
		delete cbAnalogList[i];
	}
	for(i=0;i<numDig;i++) {  //numDig < 100 or constructor exits to system
		delete cbDigitalList[i];
	}
	for(i=0;i<numMot;i++)	{ // numMot < 100 or constructor exits to system
		delete motorList[i];
	}
	for(i=0;i<numOther;i++)	{ // numOther < 100 or constructor exits to system
		delete otherList[i];
	}
	for(i=0;i<numAVR;i++)	{
		delete avrList[i];
	}
	delete[] label;
	delete[] cbAnalogList;
	delete[] cbDigitalList;
	delete[] hardwareList;
	delete[] motorList;
	delete[] otherList;
	delete[] cbLaserPowerList;
	delete[] cbLaserTriggerList;
	delete[] avrList;

}

void DataInput::clearParams()
{
	for(int i=0;i<kMaxParams;++i) params[i][0] = '\0';
	theAVR = NULL;
}

// keep alive the necessary devices
void DataInput::keepAlive()
{
	for (int i=0; i<numAVR; ++i) {
        if (!avrList[i]->keepAlive()) {
            // halt the others immediately
            for (int j=0; j<numAVR; ++j) {
                if (i != j) avrList[j]->halt();
            }
            // now try sending a halt to this AVR (probably will timeout)
            avrList[i]->halt();
            eprintf("%s has died\n", avrList[i]->getObjectName());
            eprintf("All AVR's halted!\n");
            break;
        }
	}
}

// This is for handling the file if parser finds a counterboard entry.

void DataInput::cb(FILE* fin, char *name)
{
	char fileline[100];
	char substr1[100];
	char substr2[100];

    clearParams();
	theAVR = NULL;
	while(1)	{
		if(!fgets(fileline,99,fin))	{
			qprintf("Label, but not all data, read for %s", name);
			quit();
		}
		newLine2Null(fileline);
		stripComments(fileline);
		(void)strUP(fileline);
		char *pt = skipWS(fileline);
		if(strfirst(pt,"END;")) break;
		if(strfirst(pt,"AVR32")) {
			if (sscanf(pt,"%s %s ",substr1,params[eAVRName]) == 2) {
			    theAVR = (AVR32 *)Hardware::findObject(params[eAVRName]);
			    if (!theAVR) {
			        qprintf("%s does not exist for %s in WIRING.DAT\n", params[eAVRName], name);
			        quit();
			    }
			}
		} else if(strfirst(pt,"COUNTER"))  {
			sscanf(pt,"%s %s ",substr1,params[eCounterName]);
		} else if(strfirst(pt,"ADC")) {
			sscanf(pt,"%s %s \n",substr1,params[eADCName]);
		} else if(strfirst(pt,"DIGITAL")) {
			sscanf(pt,"%s %s \n",substr1,params[eDigital]);
		} else if (strfirst(pt,"MOTOR"))	{
			sscanf(pt," %s %s",substr1,params[eMotorName]);
		}
	}
	if (!theAVR) {
	    qprintf("No AVR32 for %s\n", name);
	    quit();
	}
}

int DataInput::parser(char* fileline, char* s) {
	int i;
	stripComments(fileline);
	newLine2Null(fileline);
	if(fileline[0]=='\0') return 0;
	i = strUP(fileline);
	char *pt = skipWS(fileline);
	char s1[100];
	char *s2;
	char *substr;

	substr=strfirst(pt,"MOTOR:");
	if(substr) 	{
		strcpy(s1,substr+6);
		s2 = noWS(s1);
		strcpy(s,s2);
		return 2;
	}
	substr=strfirst(pt,"PULSER:");
	if(substr) 	{
		strcpy(s1,substr+7);
		s2 = noWS(s1);
		strcpy(s,s2);
		return 10;
	}
	substr=strfirst(pt,"LOCKOUT:");
	if(substr) 	{
		strcpy(s1,substr+8);
		s2 = noWS(s1);
		strcpy(s,s2);
		return 11;
	}
	substr=strfirst(pt,"COUNTERBOARD:");
	if(substr)	{
		strcpy(s1,substr+13); // this is the start of the counterboard label
		s2 = noWS(s1);
		strcpy(s,s2);
		return 1;
	}
	substr = strfirst(pt,"TIO10_ADDRESS1:");
	if(substr) {
		strcpy(s1,substr+15);
		s2 = noWS(s1);
		strcpy(s,s2);
		return 4;
	}
	substr = strfirst(pt,"TIO10_ADDRESS2:");
	if(substr) {
		strcpy(s1,substr+15);
		s2 = noWS(s1);
		strcpy(s,s2);
		return 5;
	}
	substr = strfirst(pt,"TIO10_WIRING");
	if(substr)	{
		return 6;
	}
	substr = strfirst(pt,"SENSEROPE:");
	if(substr)	{
		strcpy(s1,substr+10); // this is the start of the sense rope label
		s2 = noWS(s1);
		strcpy(s,s2);
		return 7;
	}
	substr = strfirst(pt,"CBLASERPOWER:");
	if(substr)	{
		strcpy(s1,substr+13); // this is the start of the sense rope label
		s2 = noWS(s1);
		strcpy(s,s2);
		return 8;
	}
	substr = strfirst(pt,"CBLASERTRIGGER:");
	if(substr)	{
		strcpy(s1,substr+15); // this is the start of the sense rope label
		s2 = noWS(s1);
		strcpy(s,s2);
		return 9;
	}
	substr = strfirst(pt,"AVR32:");
	if(substr)	{
		strcpy(s1,substr+6); // this is the start of the label
		s2 = noWS(s1);
		strcpy(s,s2);
		return 12;
	}
	// this error checking is by no means exhaustive
	if (strfirst(pt,"ADDRESS"))	{
		qprintf("Error in DataInput::parser(). Address found before label.\n");
		quit();
	}
	if (strfirst(pt,"DCSLOT"))	{
		qprintf("Error in DataInput::parser(). DCSlot found before label.\n");
		quit();
	}
	if (strfirst(pt,"DCCHANNEL"))	{
		qprintf("Error in DataInput::parser(). DCChannel found before label.\n");
		quit();
	}
	if (strfirst(pt,"ANALOG"))	{
		qprintf("Error in DataInput::parser(). Analog found before label.\n");
		quit();
	}


    if (pt[0] && pt[0] != '\r') {
     return -1; // unknown entry
	}
	return 0; // blank line

}


void DataInput::ReadFile(void) {
	FILE* fin;
	fin=opener("WIRING.DAT","r");
	int i;  // loop control
	int k;
	int linetype;   // 0=ignorable, 1=counterboard, 2=motor
	char fileline[100];
	char str[100];
	char str1[100];
	char *str2;
	fgets(fileline,99,fin);
	stripComments(fileline);
	newLine2Null(fileline);
	str2 = noWS(fileline);
	strcpy(str,str2);

	if(strcmp(str,"VERSION3.00")) 	{
		qprintf("DATAIN.DAT is not verison 3.00.\n");
		qprintf("Check that string   VERSION3.00  is in top line\n");
		qprintf("of file. Exiting to system fromm DataInput::readFile()...\n");
		quit();
	}
	while(fgets(fileline,99,fin))	{
		linetype = parser(fileline,str); // parser makes sure str is valid
		clearParams();
		switch (linetype) {
			case 0:  // do nothing
				break;
			case 1:  // do the counterboard stuff
				cb(fin,str);
				strcpy(str1,str);
				if(numHL>= listSize-1)	{
					qprintf("Tried to make a list longer than %d hardware items.\n",
							 listSize);
					qprintf("Exiting to system from DataInput::ReadFile()...\n");
					quit();
				}
				if(params[eADCName][0]) {
					strcat(str1,"ANALOGHARDWAREIO");
					cbAnalogList[numAn] = new CBAnalogIO(str1,theAVR,params[eADCName]);
					hardwareList[numHL] = (HardwareIO*)cbAnalogList[numAn];
					numAn++;
					numHL++;
				}
				if(params[eCounterName][0] || params[eDigital][0]) {
					strcpy(str1,str);
					strcat(str1,"DIGITALHARDWAREIO");
					CBDigitalIO *dig = new CBDigitalIO(str1,theAVR,params[eCounterName]);
					if (!dig) {
					    qprintf("Error creating %s\n", str1);
					    quit();
					}
					cbDigitalList[numDig] = dig;
					hardwareList[numHL] = (HardwareIO*)dig;
					numDig++;
					numHL++;
					// add digital channels if specified
                    if (params[eDigital][0]) {
                        strLC(params[eDigital]);
                        char *pt = strtok(params[eDigital], ",");
                        while (pt) {
                            if (dig->addDigital(pt)) {
                                qprintf("Error adding digital channel\n");
                                quit();
                            }
                            pt = strtok(NULL, ",");
                        }
                    } else if (!params[eCounterName][0]) {
                        qprintf("%s has no COUNTER or DIGITAL channels\n", str1);
                        quit();
                    }
				}

			    break;
			case 2:  // do the motor stuff
				strcpy(label,str);
				cb(fin,str);
				if(numMot == listSize) 	{
					qprintf("The size of the motor array is too large.\n");
					qprintf("The maximum number of motors is %d",listSize);
					qprintf("Exiting to system from DataInput::readFile()...\n");
					quit();
				}
                motorList[numMot] = new Motor(label,theAVR,params[eMotorName]);
				numMot++;
				break;
			case 3:  // unused
			case 4:  // unused
			case 5:  // unused
			case 6:  // unused
				break;
			case 7: // the SENSEROPE boards
				cb(fin,str);// this is correct; the parameters are the same
													 // as for counterboards; the difference is in
													 // the hardware objects created
				strcpy(str1,str);
				if(numHL==listSize)	{
					qprintf("Tried to make a list longer than %d hardware items.\n",
							 listSize);
					qprintf("Exiting to system from DataInput::ReadFile()...\n");
					quit();
				}
				strcpy(str1,str);
				strcat(str1,"BCDHARDWAREIO");
				cbbcdList[numBCD] = new CBbcdIO(str1,theAVR,params[eCounterName]);

				hardwareList[numHL] = (HardwareIO*)cbbcdList[numBCD];
				numHL++;
				numBCD++;
				break;
			case 8:  // cbLaserPower
				cb(fin,str);   // Yes, params same as for counterboards
				strcpy(str1,str);
				cbLaserPowerList[numCBLP]
						 = new CBLaserPowerIO(str1,theAVR,params[eCounterName]);
				hardwareList[numHL] = (HardwareIO*)cbLaserPowerList[numCBLP];
				numHL++;
				numCBLP++;

				break;
			case 9:
				cb(fin,str);   // Yes, params same as for counterboards
				strcpy(str1,str);
				cbLaserTriggerList[numCBLT] =
							new CBLaserTriggerIO(str1,theAVR,params[eCounterName]);
				hardwareList[numHL] = (HardwareIO*)cbLaserTriggerList[numCBLT];
				numHL++;
				numCBLT++;

				break;
			case 10:		// Pulser stuff
				strcpy(label,str);
				cb(fin,str);
				if(numOther == listSize) 	{
					qprintf("The size of the harware array is too large.\n");
					qprintf("The maximum number of hardware items is %d",listSize);
					qprintf("Exiting to system from DataInput::readFile()...\n");
					quit();
				}
			    otherList[numOther] = new Pulser(label,theAVR,params[eMotorName]);
				++numOther;

				break;
			case 11: {		// Lockout stuff
				strcpy(label,str);
				if(numOther == listSize) 	{
					qprintf("The size of the harware array is too large.\n");
					qprintf("The maximum number of hardware items is %d",listSize);
					qprintf("Exiting to system from DataInput::readFile()...\n");
					quit();
				}
				float max_len = -1;
				int channel = -1;
				str[0] = 0;
				while (1) {
					if(!fgets(fileline,99,fin))	{
						qprintf("Not all required data read in for ");
						qprintf(" the motor.\n Exiting to system...");
						quit();
					}
					newLine2Null(fileline);
					stripComments(fileline);
					(void)strUP(fileline);
					char *pt = skipWS(fileline);
					if (strfirst(pt,"END;")) break;
					if ((str2=strfirst(pt,"PULSER:")) != 0) {
						str2 = noWS(str2+7);
						strcpy(str,str2);	// pulser name in 'str'
					}
					else if ((str2=strfirst(pt,"CHANNEL:")) != 0) {
						channel = atoi(noWS(str2+8));
					}
					else if ((str2=strfirst(pt,"MAX_LENGTH:")) != 0) {
						max_len = atof(noWS(str2+11));
					}
				}
				if (!str[0]) {
					qprintf("No pulser for lockout %s\n",label);
					quit();
				}
				if (channel<0) {
					qprintf("No channel for lockout %s\n",label);
					quit();
				}
				if (max_len<0) {
					qprintf("No maximum length specified for lockout %s\n",label);
					quit();
				}
				otherList[numOther] = new Lockout(label,str,channel,max_len);
				++numOther;
            
				break;
            }
            case 12: {  // AVR32 stuff
                char serial[256];
                serial[0] = '\0';
                strcpy(label,str);
				while (1) {
					if(!fgets(fileline,99,fin))	{
						qprintf("Not all required data read in for %s\n", label);
						quit();
					}
					newLine2Null(fileline);
					stripComments(fileline);
					(void)strUP(fileline);
					char *pt = skipWS(fileline);
					if (strfirst(pt,"END;")) break;
					if ((str2=strfirst(pt,"SERIAL:")) != 0) {
						str2 = noWS(str2+7);
						strcpy(serial,str2);
					}
				}
				if (serial[0]) {
				    AVR32 *avr = new AVR32(label,serial);
				    if (avr) avrList[numAVR++] = avr;
				} else {
				    qprintf("AVR32 %s doesn't have SERIAL number\n",label);
				    quit();
				}
				break;
            }
			default:
				(void)printw("Unknown linetype in DataInput::ReadFile().\n");
				// read to "END;"
				for (;;) {
					if(!fgets(fileline,99,fin))	break;
					(void)strUP(fileline);
					char *pt = skipWS(fileline);
					if (!strfirst(pt,"END;")) break;
				}
				break;
		}
	}
	fclose(fin);

    initAVRs(0);
}

int DataInput::initAVRs(int newMsg)
{
    int i;

    AVR32::findFree();  // find any new AVR's that were just plugged in

	// initialize all AVR's and print message about dead ones
    AVR32 *deadAVR[listSize];
    int numDead = 0;
    int numNew = 0;
    int numLive = 0;
    char buff[1024];
    int n = 0;
    buff[0] = '\0';
	for(i=0; i<numAVR; ++i)	{
	    if (avrList[i]->isLive()) {
	        // nothing extra to do
	    } else if (avrList[i]->init()) {
		    deadAVR[numDead++] = avrList[i];
		    continue;
		} else if (newMsg) {
		    dprintf("%s is now live", avrList[i]->getObjectName());
		    ++numNew;
		}
	    ++numLive;
        n += sprintf(buff+n, " %s", avrList[i]->getObjectName());
	}
	if (!numNew && newMsg) dprintf("No new AVR's found");
	dprintf("%2d live AVR%s -%s", numLive, numLive == 1 ? "  " : "'s", buff);
	if (numDead) {
    	n = sprintf(buff,"%2d dead AVR%s -", numDead, numDead == 1 ? "  " : "'s");
    	for (i=0; i<numDead; ++i) {
    	    if (n > 70) {
    	        dprintf("%s", buff);
    	        n = 0;
    	    }
    	    n += sprintf(buff+n, " %s", deadAVR[i]->getObjectName());
    	}
    	dprintf("%s", buff);
    }

	// print messages about AVR's that aren't used
	for (i=0; i<AVR32::sNumFreeAVR; ++i) {
	    dprintf("AVR not used: %s\n", AVR32::sListFreeAVR[i].serial);
	}
	return numNew;
}

void DataInput::doneInit()
{
    int i;

	for(i=0; i<numAVR; ++i)	{
	    if (avrList[i]->didInit()) {
    	    avrList[i]->doneInit();
    	}
	}
}

HardwareIO* DataInput::getHardwareIO(char* s) {
	int ok = 0;
	HardwareIO* a;
	char instr[100];
	strcpy(instr,s);
	int i=-1;
	while((++i<100)&&((instr[i] = toupper(instr[i]))!='\0'));
	for(i = 0;i< numHL;i++)	{
		a = hardwareList[i];
		if (a->isLabel(instr)) { // changed s -> instr, 96-05-28 tjr
			ok = 1;
			break;
		}
	}
	if (!ok) a = NULL;

	return a;
}

void DataInput::digitalWriter(HardwareCard aCardUsed,
															unsigned aOutput,
															int aNumberOfBits,
															int* aChannelsUsed)
{
	int lBitNumber;
	for(lBitNumber=0; lBitNumber<aNumberOfBits; lBitNumber++) {
		bitWrite(aCardUsed,lBitNumber,(int)(aOutput&(1<<lBitNumber)));
	}


}


void DataInput::bitWrite(HardwareCard aCardUsed,
												 int aBitNumber, int aBitValue)
{
	// aBitNumber starts counting at 0. CJJ A\y, 1997


	unsigned lWord;
	lWord = mWritePCL750;

	if (aBitValue==1) lWord  = lWord | (1<< (aBitNumber));
	if (aBitValue==0) lWord  = lWord & (~(1<< (aBitNumber)));

	mWritePCL750 = lWord;


}
