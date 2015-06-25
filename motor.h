/********************************************************************/
/*                                                                  */
/*  The motor class controls stepper motors.  The speed of a motor  */
/*  is controlled by setting the period of a time/counter channel   */
/*  in the AVR32 microcontroller.                                   */
/*                                                                  */
/*  MODES:                                                          */
/*                                                                  */
/*  The motor has two control modes: STEP_MODE and VELOCITY_MODE    */
/*                                                                  */
/*  STEP MODE:                                                      */
/*                                                                  */
/*  A motor is controlled by trying to make the actual number of    */
/*  half-steps taken equal to the desired number of half-steps.     */
/*                                                                  */
/*  VELOCITY MODE:                                                  */
/*                                                                  */
/*  Motor velocity is regulated by a set of fuzzy rules to follow   */
/*  a velocity profile determined by the controlling objects (Axis  */
/*  and PolyAxis.)                                                  */
/*                                                                  */
/*  CONTROL BITS:                                                   */
/*  As well as a clock channel each motor needs two control bits    */
/*  set.  These are the direction bit and the all windings off (awo)*/
/*  bit.  These are control bits are driven by programmable i/o     */
/*  channels on the AVR32, and these channels are defined by the    */
/*  AVR32 embedded software.                                        */
/*                                                                  */
/*                            -- tjr 95-08-21                       */
/*                            -- PH 2013-10-18 updated              */
/*                                                                  */
/********************************************************************/

#ifndef __MOTOR_H
#define __MOTOR_H

#include "ioutilit.h" // database input stuff

#include "hardware.h"
#include "controll.h"
#include "clock.h"
#include "avrchannel.h"

#include "global.h"

#define MOTOR_COMMAND_NUMBER 7  // number of commands accepted by motor


enum MotorMode {
  STEP_MODE     = 1,
  VELOCITY_MODE = 2,
};

enum MotorStatus {
  kMotorOff,
  kMotorRunning,
  kMotorOnButNotRunning
};

class AVR32;

class Motor:public Controller, public AVRChannel
{
protected:
  char tag[100];            // Motor tag, read from wiring file

  long desiredHsteps;       // number of half-steps to stop
  long startHsteps;         // number of half-steps from where we started
  long totalHsteps;         // total hsteps taken since program start
  long dbaseHsteps;         // last total hsteps in database

  char *unitString;         // string for name of user units

  WideTime lastStepTime;    // time of the last motor step

  int controlMode;          // STEP_MODE or VELOCITY_MODE
  int invertDir;            // dir output is inverted

  MotorStatus motorStatus;  // non-zero if motor is on
  int currentDir;           // current direction (-1 or +1)

  float minSpeed;           // minimum speed (units/s)
  float maxSpeed;           // crusing (maximum) speed (units/s)
  float startSpeed;         // starting speed (units/s)
  float cruiseSpeed;        // cruising speed (units/s)
  float normSpeed;          // normal cruising speed (units/s)
  float hstep_conv;         // twice the number of steps per user unit
  float currentVelocity;    // current motor velocity in units/s
  float desiredVelocity;    // cruising velocity in units/s
  float lastSpeed;          // the last motor speed that we set
  float maxAccel;           // maximum acceleration in units/s/s
  float normAccel;          // normal acceleration in units/s/s
  float curAccel;           // current acceleration setting units/s/s
  float cycleDist;          // distance to run back and forth
  double nextWindingsOffTime; // time to turn off windings

  void updateTotalSteps();

  void setMotorStatus(MotorStatus status);
  void windings(int on);

  void setVelocity();     // ramp motor to desired velocity
  void hold();            // hold motor at present location

public:

  Motor(char* name, AVR32 *avr, char *aChan);

  virtual void monitor(char *outString);
  virtual void updateDatabase();  // update our database
  virtual int  needUpdate()   { return(totalHsteps != dbaseHsteps); }

  virtual void start();   // turn on motor and start ramping toward desired velocity
  virtual void stop();    // stop motor from running and turn off windings
  virtual int  checkInit();
  virtual int  initChannel();
  void step(long steps);  // step motor specified # of steps

  void setDesiredHsteps(long dhsteps) { desiredHsteps = dhsteps;  }
  long getDesiredHsteps(void)         { return desiredHsteps;     }
  long getCurrentHsteps(void)         { return totalHsteps - startHsteps; }
  void setCurrentHsteps(long chsteps);
  void setDesiredPosition(float pos)  { desiredHsteps = (long)(pos * hstep_conv); }
  float getCurrentPosition(void)      { return (totalHsteps-startHsteps)/hstep_conv;  }
  float getPosition(void)             { return totalHsteps/hstep_conv;    }

  int getControlMode(void)            { return controlMode; }
  void setControlMode(int i)          { controlMode = i;    }

  int isOn(void)                      { return motorStatus != kMotorOff; }
  int isRunning(void)                 { return motorStatus == kMotorRunning; }

  void setCruiseSpeed(char* commandString);  // set cruise motor speed in user-units/s
  void setCruiseSpeed(float speed);         // set cruise motor speed in user_units/s
  float getCruiseSpeed()              { return cruiseSpeed; }
  float getNormSpeed()                { return normSpeed;   }
  float getMaxSpeed()                 { return maxSpeed;    }
  float getMinSpeed()                 { return minSpeed;    }
  float getNormAccel()                { return normAccel;   }

  float getVelocity(void)               { return currentVelocity;  }
  void  setDesiredVelocity(float v)     { desiredVelocity = v;     }
  void  changeDesiredVelocity(float dv) { desiredVelocity += dv;   }
  float getDesiredVelocity()            { return(desiredVelocity); }
  int   getDirection()                  { return(currentDir);      }
  char* getUnits()                      { return(unitString);      }
  float getStepsPerUnit()               { return(hstep_conv * 0.5);}

  void setAcceleration(char *commandString);  // set motor acceleration
  void setAcceleration(float acc);

  void showPosition();                  // get position in user units
  void initPosition(char *inString);  // initialize position
  void initPosition(float pos);
  void setPosition(char *inString);   // set position
  void setPosition(float pos);
  void changePosition(char *inString);// change position
  void changePosition(float pos);
  void cycle(char *inString);         // run back and forth
  void modulus(long num);             // modify total steps for rotary systems

  int isTag(char *s);             // Boolean on string tag
  virtual void poll();            // poll the motor

private:        // classes for converting member functions to Commands

  static class Speed: public Command      // set motor cruising speed
  {public:
    Speed(void):Command("setCruise"){ ; }
    char* subCommand(Hardware* hw, char* inString)
    {
      ((Motor*) hw)->setCruiseSpeed(inString);
      return "HW_ERR_OK";
    }
  } cmd_speed;
  static class SetAccel: public Command  // set motor acceleration
  {public:
    SetAccel(void):Command("setAccel"){ ; }
    char* subCommand(Hardware* hw, char* inString)
    {
      ((Motor*) hw)->setAcceleration(inString);
      return "HW_ERR_OK";
    }
  } cmd_setaccel;
  static class InitPos: public Command  // initialize position
  {public:
    InitPos(void):Command("initPos"){ ; }
    char* subCommand(Hardware* hw, char* inString)
    {
      ((Motor*) hw)->initPosition(inString);
      return "HW_ERR_OK";
    }
  } cmd_initpos;
  static class SetPos: public Command  // set current position
  {public:
    SetPos(void):Command("setPos"){ ; }
    char* subCommand(Hardware* hw, char* inString)
    {
      ((Motor*) hw)->setPosition(inString);
      return "HW_ERR_OK";
    }
  } cmd_setpos;
  static class ChangePos: public Command  // change current position
  {public:
    ChangePos(void):Command("changePos"){ ; }
    char* subCommand(Hardware* hw, char* inString)
    {
      ((Motor*) hw)->changePosition(inString);
      ((Motor*) hw)->avrLive();
      return "HW_ERR_OK";
    }
  } cmd_changepos;
  static class StopCmd: public Command  // stop motor
  {public:
    StopCmd(void):Command("stop",kAccessAlways){ ; }
    char* subCommand(Hardware* hw, char* inString)
    {
      ((Motor*) hw)->stop();
      ((Motor*) hw)->avrLive();
      return "HW_ERR_OK";
    }
  } cmd_stop;
  static class CycleCmd: public Command  // stop motor
  {public:
    CycleCmd(void):Command("cycle"){ ; }
    char* subCommand(Hardware* hw, char* inString)
    {
      ((Motor*) hw)->cycle(inString);
      ((Motor*) hw)->avrLive();
      return "HW_ERR_OK";
    }
  } cmd_cycle;

  virtual Command *getCommand(int num)  { return commandList[num]; }
  virtual int     getNumCommands()      { return MOTOR_COMMAND_NUMBER; }

private:
  static Command  *commandList[MOTOR_COMMAND_NUMBER];
};


#endif
