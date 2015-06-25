//--------------------------------------------------------------
// File:        avr32.h
//
// Description: Controller object for the AVR32 device
//
// Created:     2012/09/24 - P. Harvey
//
#ifndef __AVR32_H
#define __AVR32_H

#include "controll.h"
#include "command.h"
#include "display.h"

#define AVR32_COMMAND_NUMBER    2

const int   kMaxFreeAVR = 100;

class PInterfaceUSB;

struct FreeAVR
{
    PInterfaceUSB * usb;
    char          * serial;
};

class AVR32 : public Controller
{
    int     index;      // index for this avr
public:
	AVR32(char *label, char *serial);
	~AVR32();

    int     init();
    void    close();
    int     sendCommand(char *cmd, int buffSize=0);
    int     halt()          { return sendCommand("halt\n"); }
    char    *getSerial()    { return mSerial; }
    int     isLive()        { return mIsLive; }
    int     didInit()       { return mDidInit; }
    int     keepAlive();
    void    doneInit()      { mDidInit = 0; }

    static int      findFree();

	virtual Command *getCommand(int num)	{ return commandList[num]; }
	virtual int 	 getNumCommands() 	{ return AVR32_COMMAND_NUMBER; }
    virtual void     poll();
	virtual void     monitor(char *outStr);

private:
	static class Init: public Command
	{public:
		Init(void):Command("init",kAccessCode1){ ; }
		char* subCommand(Hardware* hw, char* inString)
		{
		    if (((AVR32*) hw)->isLive()) {
		        dprintf("But %s is live!",hw->getObjectName());
		    } else {
		        // find any new devices that came online
		        AVR32::findFree();
		        if (((AVR32*) hw)->init()) {
                    dprintf("Error initializing %s", hw->getObjectName());
                } else {
		            dprintf("%s initialized OK",hw->getObjectName());
		        }
		    }
			return "HW_ERR_OK";
		}
	} cmd_init;
	static class Any: public Command
	{public:
		Any(void):Command("<any>",kAccessExpert){ ; }
		char* subCommand(Hardware* hw, char* inString)
		{
		    char buff[1024];
		    sprintf(buff,"%s\n",inString);
			int result = ((AVR32*) hw)->sendCommand(buff, sizeof(buff));
			if (result < -1) dprintf("Error %d sending command:\n", result);
			display->message(buff);
			return "HW_ERR_OK";
		}
	} cmd_any;

	static Command	*commandList[AVR32_COMMAND_NUMBER];	// list of commands

    char            *mSerial;
    PInterfaceUSB   *mUSB;
    double          mLastCmdTime;
    int             mIsLive;
    int             mDidInit;

public:
    static int      sNumFreeAVR;
    static FreeAVR  sListFreeAVR[kMaxFreeAVR];
};

#endif
