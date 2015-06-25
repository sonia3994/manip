#ifndef __ENCODER_H
#define __ENCODER_H

#include "ioutilit.h" // databasei input stuff

#define ENCODER_COMMAND_NUMBER  4
#define ENCODER_SUB_STEP        4  // counts per encoder step (for sub-stepping counters)

#include "display.h"
#include "global.h"
#include "hwio.h"
#include "datain.h"

extern  Display  *display;  // defined in motorcon.cpp

class Encoder:public Controller
{
private:
  float lengthValue;    // length in cm
  int   loopFlag;       // execute read loop
  float cm_per_count;   // calibration of encoder
  float zeroLength;     // length at counter zero
  float dbaseLength;    // length last time database updated

  double lastTime;      // time of previous database update
  unsigned int uncalibrated;  // the return from the poll function
  long rollOver;  // keeps track of high word of encoder rollover
  HardwareIO * hwInput;

public:
  Encoder(char* name);   // database constructor

  ~Encoder(void) {  }

  virtual void monitor(char *outString);
  virtual void updateDatabase(void);
  virtual int  needUpdate() { return(length() != dbaseLength);  }

  void poll(void); // poll encoder -- just does some maintenance on database

  float length();     // return last read length

  float getCalibration(void)      {return cm_per_count;} // gets calibration (cm/count)
  float getStepSize(void)         {return cm_per_count * ENCODER_SUB_STEP;} // gets cm per encoder step
  unsigned  getUncalibrated(void) {return uncalibrated;} // get raw bytes, needed for laser control from the special counter board
  unsigned  getStatus(void)       {return hwInput->getStatus();}
  int   avrLive(int showMsg)      {return hwInput->avrLive(showMsg);}

  void  setLoopFlag(void) {loopFlag = 1;}
  void  clearLoopFlag(void) {loopFlag = 0;}
  void  setPosition(float length); // set current length

private:  // classes for accessing member functions via Command class

  static class Read: public Command        // read encoder length in cm
  {public:
    Read(void):Command("read"){;};
    char* subCommand(Hardware* hw, char* inString)
    {
      ((Encoder*) hw)->poll();
      sprintf(display->outString,"%s Length: %f          ",hw->getObjectName(),((Encoder*) hw)->length());
      display->message();
      return "HW_ERR_OK";
    }
  } cmd_read;
  static class ReadLoop: public Command    // enter a continuous read loop
  {public:
    ReadLoop(void):Command("readloop"){;};
    char* subCommand(Hardware* hw, char* inString)
      {((Encoder*) hw)->setLoopFlag(); return "HW_ERR_OK";}
  } cmd_readloop;
  static class StopLoop: public Command
  {public:
    StopLoop(void):Command("stoploop"){;};
    char* subCommand(Hardware* hw, char* inString)
      {((Encoder*) hw)->clearLoopFlag(); return "HW_ERR_OK";}
  } cmd_stoploop;
  static class SetPosition: public Command // set length at co-ordinate origin
  {public:
    SetPosition(void):Command("setposition","#"){;};
    char* subCommand(Hardware* hw, char* inString)
    {
      float x = atof(inString);
      ((Encoder*) hw)->setPosition(x);
      return "HW_ERR_OK";
    }
  } cmd_setposition;

protected:
  virtual Command *getCommand(int num)  { return commandList[num]; }
  virtual int     getNumCommands()      { return ENCODER_COMMAND_NUMBER; }

private:
  static Command  *commandList[ENCODER_COMMAND_NUMBER]; // list of commands

};


#endif
