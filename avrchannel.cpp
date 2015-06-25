//--------------------------------------------------------------
// File:        avrchannel.cpp
//
// Description: Mix-in class for AVR devices
//
// Created:     2012/10/09 - P. Harvey
//
#include "avrchannel.h"
#include "avr32.h"
#include "display.h"
#include "ioutilit.h"

AVRChannel::AVRChannel(AVR32 *avr, char *aChan) : mAVR(avr)
{
    if (avr && aChan) {
      strcpy(mChannel, aChan);
      strLC(mChannel);
    } else {
      strcpy(mChannel, "(none)");
    }
}

int AVRChannel::avrCommand(const char *cmd)
{
    if (!mAVR) return(-999);
    if (cmd) {
        sprintf(mResponse,"%s %s",mChannel,cmd);
    } else {
        strcpy(mResponse,mChannel);
    }
    return(mAVR->sendCommand(mResponse, kResponseLen));
}

int AVRChannel::avrLive(int showMsg)
{
    int isLive;
    if (!mAVR) {
        strcpy(display->outString, "ERROR: AVR doesn't exist");
        if (showMsg) display->message();
        isLive = 0;
    } else if (!mAVR->isLive()) {
        sprintf(display->outString, "ERROR: %s is dead", mAVR->getObjectName());
        if (showMsg) display->message();
        isLive = 0;
    } else {
        isLive = 1;
    }
    return isLive;
}

void AVRChannel::monitor(char *outStr)
{
    if (mAVR) {
        sprintf(outStr, "Channel: %s %s (%s)\n", mAVR->getObjectName(),mChannel,
            mAVR->isLive() ? "live" : "dead");
    } else {
        strcpy(outStr, "Channel: (none)\n");
    }
}

