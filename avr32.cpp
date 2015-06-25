//--------------------------------------------------------------
// File:        avr32.cpp
//
// Description: Controller object for the AVR32 device
//
// Created:     2012/09/24 - P. Harvey
//
#include <unistd.h>
#include "avr32.h"
#include "clock.h"
#include "PInterfaceUSB.h"

//#define DEBUG_USB     // uncomment this to print USB read/write strings

const int    kVendor = 0x03eb;
const int    kProduct = 0x2300;
const int    kEndpointIn = 1;
const int    kEndpointOut = 2;
const double kKeepAliveTime = 0.5; // send commands with this maximum period (s) to keep AVR alive

AVR32::Init   AVR32::cmd_init;
AVR32::Any    AVR32::cmd_any;
int           AVR32::sNumFreeAVR = -1;
FreeAVR       AVR32::sListFreeAVR[kMaxFreeAVR];

Command* AVR32::commandList[AVR32_COMMAND_NUMBER] = {
    &cmd_init,
	&cmd_any
};


AVR32::AVR32(char *label, char *serial)
    : Controller("AVR32", label), mSerial(NULL), mUSB(NULL)
{
    mSerial = new char[strlen(serial) + 1];
    mLastCmdTime = 0;
    mIsLive = 0;
    mDidInit = 0;
    if (!mSerial) quit("out of memory for AVR32");
    strcpy(mSerial, serial);
}

AVR32::~AVR32()
{
    if (mUSB) {
        sendCommand("halt;wdt 0");  // make sure motors are shut down
        close();
    }
    delete mSerial;
}

// close USB connection
void AVR32::close()
{
    if (mUSB) {
        delete mUSB;
        mUSB = NULL;
    }
}

void AVR32::poll()
{
   // do nothing
}

void AVR32::monitor(char *outStr)
{
    sprintf(outStr, "Live: %s\nSerial: %s", mIsLive ? "YES" : "NO", mSerial);
}

// returns error code, or 0 on success
// (must call findFree() before calling this routine)
int AVR32::init()
{
    close();

    for (int i=0; i<sNumFreeAVR; ++i) {
        if (!strcasecmp(sListFreeAVR[i].serial, mSerial)) {
            // take ownership of the USB device
            mUSB = sListFreeAVR[i].usb;
            // remove free AVR object from the list
            memmove(&sListFreeAVR[i], &sListFreeAVR[i+1], (sNumFreeAVR-i-1)*sizeof(FreeAVR));
            --sNumFreeAVR;
            // re-enable our watchdog timer and go live
            sendCommand("wdt 1");
            mDidInit = 1;
            return 0;
        }
    }
    return -1;
}

// look for new AVR devices on USB interface
// returns number of new AVR's found
int AVR32::findFree()
{
    int n;
    const int kBuffSize = 256;
    char buff[kBuffSize+1];

    if (sNumFreeAVR < 0) {
        sNumFreeAVR = 0;
    } else if (sNumFreeAVR >= kMaxFreeAVR) {
        return 0;
    }
    int num = sNumFreeAVR;
    // loop through all USB devices looking for AVR32's
    for (int i=0; ;++i) {
        PInterfaceUSB *usb = new PInterfaceUSB;
        usb->SetReadEndpoint(kEndpointIn);
        usb->SetWriteEndpoint(kEndpointOut);
        int result = usb->Open(kVendor, kProduct, i);
        if (result == 1) {
            // get AVR32 serial number:
            // Send 4 commands separately because for some reason the AVR32
            // sometimes won't return responses for the first few commands
            // after a disconnected/reconnect.  Don't send any more commands
            // because these 4 reponses just fit inside the 64-byte limit.
            if (usb->Write("nop\n") > 0 &&
                usb->Write("nop\n") > 0 &&
                usb->Write("wdt 0\n") > 0 &&
                usb->Write("5.ser\n") > 0)
            {
                char *pt = NULL;
                // read the rest of the responses
                for (int j=0; j<4; ++j) {
                    n = usb->Read(buff, kBuffSize);
                    if (n <= 0) break;
                    buff[n] = '\0';     // null terminate the string
                    // scan the string (a packet may contain more than one response)
                    pt = strstr(buff,"5.OK ");
                    if (pt) break;
                }
                if (pt && (buff+n-pt >= 35)) {
                    pt[35] = '\0';    // terminate at end of 30-digit serial number
                    // see if this AVR was already in the free list
                    int found = 0;
                    for (int j=0; j<sNumFreeAVR; ++j) {
                        if (!strcmp(sListFreeAVR[j].serial, pt+5)) {
                            delete sListFreeAVR[j].usb; // delete old (probably disconnected) USB object
                            sListFreeAVR[j].usb = usb;  // replace with this one
                            found = 1;
                            break;
                        }
                    }
                    if (found) continue;
                    // add new AVR to the free list
                    sListFreeAVR[sNumFreeAVR].serial = new char[31];
                    if (sListFreeAVR[sNumFreeAVR].serial) {
                        strcpy(sListFreeAVR[sNumFreeAVR].serial, pt+5);
                        sListFreeAVR[sNumFreeAVR].usb = usb;
                        if (++sNumFreeAVR >= kMaxFreeAVR) break;
                        continue;
                    }
                }
            }
        }
        delete usb;
        if (result == 0) break; // no more devices
    }
    return(sNumFreeAVR - num);
}

// must be called regularly to keep AVR32 alive
// returns false if we have just died
int AVR32::keepAlive()
{
    if (mIsLive &&
       (Clock::getClock()->time() - mLastCmdTime > kKeepAliveTime) &&
        sendCommand("wdt"))
    {
        mIsLive = 0;
        return 0;
    } else {
        return 1;
    }
}

// send command to AVR32 (adds trailing newline to command if necessary)
// returns error code, and response in cmd string if buffSize was non-zero
// returns USB error code, or -1003/-1004 if write/read returns 0 bytes,
// or -1 if cmd doesn't return "OK" response
// (command/response synchronization is ensured by adding a command
//  identification prefix, and matching this with the response ID)
int AVR32::sendCommand(char *cmd, int buffSize)
{
    static char id = 'A'-1;         // ID of previous command
    const int kBuffSize = 2048;
    char buff[kBuffSize+1];

    if (!mUSB) return -1000;                    // ERROR: not connected
    int cmdLen = strlen(cmd);
    if (!cmdLen) return -1001;                  // ERROR: empty command
    if (cmdLen + 4 >= kBuffSize) return -1002;  // ERROR: command too long
    // copy into our local buffer and add command ID
    if (++id > 'Z') id = 'A';
    buff[0] = id;
    buff[1] = '.';
    strcpy(buff+2, cmd);
    // add terminating newline character if necessary
    if (buff[cmdLen+1] != '\n') {
        buff[cmdLen+2] = '\n';
        buff[cmdLen+3] = '\0';
    }
    // send the command
#ifdef DEBUG_USB
    dprintf("%s Write: %s",getObjectName(),buff);
#endif
    int n = mUSB->Write(buff);
    if (n <= 0) return(n ? n : -1003);           // ERROR: USB write error
    // update time of last successful command
    mLastCmdTime = Clock::getClock()->time();
    mIsLive = 1;
    // get command response
    int len = 0;
    for (;;) {
        n = mUSB->Read(buff+len, kBuffSize-len);
        if (n <= 0) return(n ? n : -1004);      // ERROR: USB read error
        len += n;
#ifdef DEBUG_USB
        buff[len] = '\0';
        dprintf("%s Read %d: %s",getObjectName(),n,buff+len-n);
#endif
        // check for the null terminator at the end of the response
        if (buff[len-1] != '\0') {
            if (len >= kBuffSize) return -1005; // ERROR: response too long for rcv buffer
            // continue reading packets in this response
            continue;
        }
        // only accept the response from the command with the correct ID
        if (len >= 2 && buff[0] == id && buff[1] == '.') break;
        // see if we got more than one response
        // (this is possible, but unlikely)
        char *pt;
        char *end = buff + len - 2; // (don't scan last 2 chars)
        for (pt=buff; pt<end; ++pt) {
            // look for the start of the next response
            if (*pt != '\0' && *pt != '\n') continue;
            if (pt[1]==id && pt[2]=='.') {
                // move the good response to the start of the buffer
                len -= pt + 1 - buff;
                memmove(buff, pt+1, len);
                break;
            }
        }
        // break if we found this response later in the packet
        if (pt < end) break;
    }
    // return null-terminated response (without ID) if requested
    if (buffSize) {
        if (len-2 > buffSize) return -1006;     // ERROR: response too long for user buffer
        memcpy(cmd, buff+2, len-2);
    }
    if (memcmp(buff+2,"OK",2)) return -1;       // ERROR: response not "OK"
    return 0;   // success!
}
