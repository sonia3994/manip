//--------------------------------------------------------------
// File:        avrchannel.h
//
// Description: Mix-in class for AVR channel devices
//
// Created:     2012/10/09 - P. Harvey
//
#ifndef __AVRCHANNEL_H__
#define __AVRCHANNEL_H__

#include "controll.h"

class AVR32;

const int kResponseLen = 1024;

class AVRChannel
{
public:
	AVRChannel(AVR32 *avr, char *aChan);

    int     avrCommand(const char *cmd=NULL);
    char *  avrResponse()   { return mResponse; }
    int     avrLive(int showMsg=1);
    char *  getChannel()    { return mChannel; }
    AVR32 * getAVR()        { return mAVR; }
    virtual void monitor(char *outStr);
    virtual int initChannel() { return 0; }

protected:
    AVR32 * mAVR;
    char    mChannel[64];       // AVR channel name
    char    mResponse[kResponseLen];
};

#endif // __AVRCHANNEL_H__
