#ifndef __AXIS_H
#define __AXIS_H

#include "threevec.h" // three-vector geometry stuff
#include "loadcell.h"
#include "motor.h"
#include "encoder.h"

#define AXIS_COMMAND_NUMBER 16
#define AXIS_VER  2.01    // AXIS.DAT file version number

const int kRegularAxisType  = 0;

enum FlagType {
  kLowFlag,
  kHighFlag,
  kStopFlag,
  kNumFlagTypes
};

enum AxisMode {   // monitor() routine must be updated if modes are changed
  kAxisIdle,      // idle
  kAxisStep,      // step mode
  kAxisTension,   // constant tension mode
  kAxisVelocity   // velocity mode
};

enum StopCause {
  NO_CAUSE,             // initial value
  LOW_TENSION_STOP,     // tension too low so stopped
  HIGH_TENSION_STOP,    // tension too high so stopped
  ENDPOINT_STOP,        // reached endpoint so stopped
  STUCK_STOP,           // not getting any closer to endpoint
  NET_FORCE_STOP,       // single net force sample out of bounds
  AXIS_ERROR_STOP,      // an axis reported an error
  CALC_ERROR_STOP,      // a calculation error stopped the motion
  AXIS_FLAG_STOP,       // an axis digital flag stopped the motion
  AXIS_ALARM_STOP,      // an axis alarm stopped the motion
};


class ManipulatorRope;
class AV;

class Axis:public Controller
{
private:
  double time;          // current time in seconds
  double lastTime;      // time on last poll

  AxisMode  axisMode;   // control mode
  ManipulatorRope *manRope;
  int       stickyFlag; // sticky control mode (reset by stop())
  int       tensionUpFlag;

  int       heavyWaterFlag;

  double deadLength;    // unchanging part of rope length
  double elasticity;    // rope elasticity
  double elasticity2;   // rope non-linear elasticity coef (root of tension)
  double length;        // unstreched length of rope beyond pulleys
  double oldLength;     // encoder length on previous call to getLength
  unsigned oldRaw;      // old raw encoder value
  double dbaseLength;   // length stored in database file
  double oldTension;    // tension on previous call to getLength
  double effLength;     // effective rope length (return value from getLength)

  unsigned mFlagMask;   // mask of all active flag bits
  unsigned mStopFlag[kNumFlagTypes]; // masks for individual flag types
  int   mFlagError;     // flag error type
  
  int   mErrorStopStatus; // set to status if stopped due to tension error
  int   mErrorStopOn;   // non-zero if error stop is on 

  float desiredPosition;// position in cm along rope (from encoder)
  float desiredVelocity;// desired velocity for tension mode
  float desiredTension; // tension asked for by PolyAxis object

  float endPosition;    // where it will be at end of motion
  float startPosition;  // where it was at start of motion

  int   numReversals;   // number of times the motor reversed direction to find endpoint
  int   reverseFlag;    // flag used to monitor direction for reversals

  float springStretch;  // length of rope let out when spring stretches to maximum tension
  float minSpringTension;// minimum rope tension before spring hits stop
  float maxSpringTension;// maximum rope tension before spring hits stop
  float springConstant; // spring constant (cm of rope / rope tension in N)
  float meanEncoderErr; // time-averaged encoder error

  float minTension;     // minimum rope tension (N)
  float maxTension;     // maximum rope tension (N)
  float conTension;     // normal control tension (N)
  float minConTension;  // minimum control tension (N)
  float maxConTension;  // maximum control tension (N)

  int     mBadTensionCount;   // number of times tension was bad
  double  mBadTensionTime;    // time of last bad tension reading
  float   mBadTensionValue;   // value for bad tension
  int     mBadEncoderCount;   // number of times tension was bad
  double  mBadEncoderTime;    // time of last bad tension reading


  float alarmTension;   // tension for high-tension alarm (N)
  int   alarmAction;    // action for high tension alarm (0=none, 1=warn, 2=stop)
  int   alarmStatus;    // alarm status (0=no alarm, 1=warned, 2=stopped)
  float highTension;    // high tension value

  float pulleyRadius;   // radius of pulley on carriage
  float topRadius;      // radius of top pulley (will be zero for final bushing)
  ThreeVector pulleyAxis; // axis direction for pulleys

  ThreeVector xtop;   // top pulley position (cm, relative to vessel centre)
  const ThreeVector &xbot;  // bottom point (cm, relative to vessel centre)
  ThreeVector xoff;   // carriage pulley offset (cm, relative to carriage centre)

  Motor     *motor;             // Axis Hardware components
  Encoder   *encoder;
  Loadcell  *loadcell;

  float     encoderStep;        // encoder calibration (cm per step)
  float     accuracy;           // accuracy of final position

  void  calcEffLength();        // calculate effective rope length

  void  decommand(void);        // force Hardware subunits to refuse commands
  void  recommand(void);        // enable Hardware subunit command processing

  void  moveMotor(float dist);  // move motor by specified distance
  void  initLength();           // initialize encoders with new length
  float getSpringLength(float tension); // get spring length at given tension

public:

  Axis(char* axisName,        // constructor from database
       AV *av,
       FILE *fp=NULL,         // file to read data from
       FILE **fpOut=NULL);

  ~Axis(void);                 // deletes Axis object and subunits

  virtual void  monitor(char *outString);
  virtual void  updateDatabase(void);   // helper function to update database
  virtual int   needUpdate()  { return(dbaseLength!=length);  }

  void      setAxisMode(AxisMode mode); // set axis control mode
  AxisMode  getAxisMode()          { return axisMode;    }
  virtual int getAxisType()        { return kRegularAxisType;  }
  void      setSticky(int stick=1) { stickyFlag = stick; }
  void      toggleSticky(int showMsg=0);
  void      tensionActivate(int upOnly=0);  // turn on constant tension mode
  void      velocityActivate();     // turn on velocity mode
  void      scaleSpeed(float scale);        // scale motor speed and accel
  void      minLength();                  // initialize rope to minimum length
  int       checkErrors(int show_msg=1);  // check to see if errors is OK

  ManipulatorRope *getRope()           { return manRope;        }

  void      setDesiredTension(float f) { desiredTension = f;    }
  float     getDesiredTension(void)    { return desiredTension; }

  void      setDesiredVelocity(float v){ desiredVelocity = v;   }
  float     getDesiredVelocity()       { return desiredVelocity;}

  float     getEncoderStep()           { return encoderStep;      }
  float     getEncoderError()          { return getEncoderError(oldTension);  }
  float     getEncoderError(float tension); // get error at specified tension
  void      resetEncoderError();  // set encoder error to zero
  int       isEncoderError();     // is error too large?
  void      setAccuracy(float ac=0)    { accuracy = (ac ? ac : encoderStep);  }

  float     getMinTension()            { return minTension;     }
  float     getMaxTension()            { return maxTension;     }
  float     getControlTension()        { return conTension;     }
  float     getMinControlTension()     { return minConTension;  }
  float     getMaxControlTension()     { return maxConTension;  }
  int       getAlarmStatus()           { return alarmStatus;    }
  int       getErrorStopStatus()       { return mErrorStopStatus; }
  void      resetErrorStopStatus()     { mErrorStopStatus = NO_CAUSE; }

  void      poll();                // polls Axis and controls it

  ThreeVector getXtop(void) const {return xtop;} // return top pulley position
  ThreeVector getXbot(void) const {return xbot;} // return bottom point

  ThreeVector getPulleyOffset(void) const {return xoff;} // return carriage offset
  float       getPulleyRadius(void) const {return pulleyRadius;}
  float       getTopRadius(void) const    {return topRadius;}

  double      getLength(void);              // tension-corrected length
  void        setLength(float len);         // set unstreched length

  float       getTension(void) {return loadcell->getTension();} // tension in N
  void        setTension(float tension);  // set tension hold mode

  int         isLowTension();
  int         isHighTension();
  int         isBadTension();
  float       badTensionValue() {return mBadTensionValue;}

  float       getVelocity(void) {return motor->getVelocity();}  // motor velocity
  float       getNormSpeed()    {return motor->getNormSpeed();}
  int         getNumReversals() {return numReversals; }

  void        setDesiredPosition(float L) {desiredPosition = L;}
  float       getDesiredPosition() { return desiredPosition;  }
  void        setEndPosition(float L) {endPosition = L;}
  int         atDesiredPosition();

  void        up(char* cmd);          // up command
  void        down(char* cmd);        // down command
  void        to(float arg);          // to a given position
  void        setMaxSpeed(char*);     // set axis speed limit
  void        zero(void);             // zero axis encoder

  void        alarmReset();
  void        alarmActionProc(char *cmd);
  void        alarmTensionProc(char *cmd);

  void        tensionLimits();        // print tension limits
  void        errorStopProc(char *cmd);

  double      getStretchFactor(double tension);

  void        loadcellCalibrate(void) {loadcell->calibrate();}
  void        loadcellCalibrationPoint(char* inString) {loadcell->calibrationPoint(inString);}
  void        rampDown();   // ramp down speed immediately
  void        stop();

  int         checkFlags(int motorDir);   // check stop flags and set mFlagError
  int         getFlagError()    { return(mFlagError);         }

  int         isInHeavyWater()  { return(heavyWaterFlag);     }
  int         isRunning()       { return(motor->isRunning()); }
  int         isOn()            { return(motor->isOn());      }

private:        // classes for converting member functions to Commands

  static class Up: public Command
  {public:
    Up(void):Command("up","#"){ ; }
    char* subCommand(Hardware* hw, char* inString)
      {((Axis*) hw)->up(inString);return "HW_ERR_OK";}
  } cmd_up;
  static class Down: public Command
  {public:
    Down(void):Command("down","#"){ ; }
    char* subCommand(Hardware* hw, char* inString)
      {((Axis*) hw)->down(inString);return "HW_ERR_OK";}
  } cmd_down;
  static class To: public Command
  {public:
    To(void):Command("to","#"){ ; }
    char* subCommand(Hardware* hw, char* inString)
    {
      float arg = atof(inString);
      ((Axis*) hw)->to(arg);
      return "HW_ERR_OK";
    }
  } cmd_to;
  static class Calibrate: public Command
  {public:
    Calibrate(void):Command("calibrate"){ }
    char* subCommand(Hardware* hw, char* inString)
      {((Axis*) hw)->loadcellCalibrate();return "HW_ERR_OK";}
  } cmd_calibrate;
  static class Point: public Command
  {public:
    Point(void):Command("point"){ }
    char* subCommand(Hardware* hw, char* inString)
      {((Axis*) hw)->loadcellCalibrationPoint(inString);return "HW_ERR_OK";}
  } cmd_point;
  static class Speed: public Command
  {public:
    Speed(void):Command("maxSpeed","#"){ }
    char* subCommand(Hardware* hw, char* inString)
      {((Axis*) hw)->setMaxSpeed(inString);return "HW_ERR_OK";}
  } cmd_speed;
  static class Stop: public Command
  {public:
    Stop(void):Command("stop",kAccessAlways) { }
    char* subCommand(Hardware* hw, char* inString)
    {
      ((Axis*) hw)->stop();  // stop motor
      return "HW_ERR_OK";
    }
  } cmd_stop;
  static class Tension: public Command
  {public:
    Tension(void):Command("tension","#"){ ; }
    char* subCommand(Hardware* hw, char* inString)
    {
      ((Axis*) hw)->setDesiredTension(atof(inString));
      ((Axis*) hw)->toggleSticky(0); // make it sticky
      ((Axis*) hw)->tensionActivate(0);  // start tension mode
      return "HW_ERR_OK";
    }
  } cmd_tension;
  static class Reset: public Command
  {public:
    Reset(void):Command("reset"){ ; }
    char* subCommand(Hardware* hw, char* inString)
    {
      ((Axis*) hw)->resetEncoderError();
      return "HW_ERR_OK";
    }
  } cmd_reset;
  static class Sticky: public Command
  {public:
    Sticky(void):Command("sticky"){ ; }
    char* subCommand(Hardware* hw, char* inString)
    {
      ((Axis*) hw)->toggleSticky(1);
      return "HW_ERR_OK";
    }
  } cmd_sticky;
  static class MinLength: public Command
  {public:
    MinLength(void):Command("minLength"){ ; }
    char* subCommand(Hardware* hw, char* inString)
    {
      ((Axis*) hw)->minLength();
      return "HW_ERR_OK";
    }
  } cmd_minLength;
  static class AlarmReset: public Command
  {public:
    AlarmReset(void):Command("alarmReset"){ ; }
    char* subCommand(Hardware* hw, char* inString)
    {
      ((Axis*) hw)->alarmReset();
      return "HW_ERR_OK";
    }
  } cmd_alarmReset;
  static class AlarmAction: public Command
  {public:
    AlarmAction(void):Command("alarmAction"){ ; }
    char* subCommand(Hardware* hw, char* inString)
    {
      ((Axis*) hw)->alarmActionProc(inString);
      return "HW_ERR_OK";
    }
  } cmd_alarmAction;
  static class AlarmTension: public Command
  {public:
    AlarmTension(void):Command("alarmTension"){ ; }
    char* subCommand(Hardware* hw, char* inString)
    {
      ((Axis*) hw)->alarmTensionProc(inString);
      return "HW_ERR_OK";
    }
  } cmd_alarmTension;
  static class ErrorStop: public Command
  {public:
    ErrorStop(void):Command("errorStop"){ ; }
    char* subCommand(Hardware* hw, char* inString)
    {
      ((Axis*) hw)->errorStopProc(inString);
      return "HW_ERR_OK";
    }
  } cmd_errorStop;
  static class TensionLimits: public Command
  {public:
    TensionLimits(void):Command("tensionLimits",kAccessAlways){ ; }
    char* subCommand(Hardware* hw, char* inString)
    {
      ((Axis*) hw)->tensionLimits();
      return "HW_ERR_OK";
    }
  } cmd_tensionLimits;

protected:
  virtual Command *getCommand(int num)  { return commandList[num]; }
  virtual int     getNumCommands()      { return AXIS_COMMAND_NUMBER; }

private:
  static Command  *commandList[AXIS_COMMAND_NUMBER];  // list of commands
  static char     *sFlagTypeStr[kNumFlagTypes]; // flag type strings
};

#endif

