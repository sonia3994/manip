// Revisions: Jan/Feb 97 PH - Major reworking
//            02/28/97 PH - hacked out fuzzy logic code
//            03/31/97 PH - removed name/tag confusion
//                        - component names now read from data file
//            05/14/97 PH - modified axis to run without an encoder
//            08/15/97 PH - fixed problem with meanEncoderError

//            11/10/97 PH - changed to subtract mean error in GetEncoderError()
//
// Notes: Encoder drift occurs because the amount of rope the motor plays
//        out relative to its position depends on the tension history of
//        the rope on the drum.  We allow a slow drift in encoder error
//        because we are just using this to watch for motor stalls.
#include "axis.h"
#include "av.h"
#include "datain.h"
#include "global.h"
#include "ieee_num.h"
#include "manrope.h"

extern DataInput *dataInput;

const float kMotorCorrScale   = 0.4;// relative speed to run motor for fine corrections
const float kMinMotorScale    = 0.1;// minimum motor speed scaling factor
const float kAllowedDrift     = 0.005;// allow 0.5% encoder drift without causing alarm
const short kMaxEncoderError  = 10; // maximum encoder error
const short kMaxTensionError  = 4;  // maximum tension error

const float kTVCoef           = 0.8;// coefficient for constant tension mode
const float kTensionThreshold = 0.3;// min. tension to start motor in constant tension mode (N)
const float kMaxTensionScale  = 0.1;// scale factor for tension error (N/N)
const short kBadTensionTime   = 1;  // time period for multiple bad tensions (s)
const short kBadTensionCount  = 10; // max number of bad tensions before stop
const short kBadEncoderCount  = 5;  // max number of bad encoders before stop
const short kBadEncoderTime   = 1;  // time period for multiple bad encoders (s)

// static member declarations
Axis::Up        Axis::cmd_up;
Axis::Down      Axis::cmd_down;
Axis::To        Axis::cmd_to;
Axis::Calibrate Axis::cmd_calibrate;
Axis::Point     Axis::cmd_point;
Axis::Speed     Axis::cmd_speed;
Axis::Stop      Axis::cmd_stop;
Axis::Tension   Axis::cmd_tension;
Axis::Reset     Axis::cmd_reset;
Axis::Sticky    Axis::cmd_sticky;
Axis::MinLength Axis::cmd_minLength;
Axis::AlarmReset Axis::cmd_alarmReset;
Axis::AlarmAction Axis::cmd_alarmAction;
Axis::AlarmTension Axis::cmd_alarmTension;
Axis::ErrorStop Axis::cmd_errorStop;
Axis::TensionLimits Axis::cmd_tensionLimits;

Command* Axis::commandList[AXIS_COMMAND_NUMBER] = {
  &cmd_up,
  &cmd_down,
  &cmd_to,
  &cmd_calibrate,
  &cmd_point,
  &cmd_speed,
  &cmd_stop,
  &cmd_tension,
  &cmd_reset,
  &cmd_sticky,
  &cmd_minLength,
  &cmd_alarmReset,
  &cmd_alarmAction,
  &cmd_alarmTension,
  &cmd_errorStop,
  &cmd_tensionLimits,
};

char* Axis::sFlagTypeStr[kNumFlagTypes] = {
  "LOW_FLAG",
  "HIGH_FLAG",
  "STOP_FLAG"
};


Axis::Axis(char* axisName,  // constructor from database
     AV *av,
     FILE *fp,              // file to read data from (or null)
     FILE **fpOut) :
     Controller("Axis",axisName),
     xbot(av->getBlockPosition(axisName))
{
  const int kMaxLen = 128;
  char componentName[kMaxLen];  // name of a subcomponent of an axis
  char *namePtr;
  char* key;                // pointer to character *AFTER* keyword

  stickyFlag = 0;
  tensionUpFlag = 0;
  numReversals = 0;
  desiredPosition = 0;
  desiredVelocity = 0;
  desiredTension = 0;
  highTension = 0;
  mErrorStopOn = 1;
  mErrorStopStatus = NO_CAUSE;
  mBadTensionCount = 0;
  mBadTensionTime = RTC->time() + kBadTensionTime;
  mBadTensionValue = 0;
  mBadEncoderCount = 0;
  mBadEncoderTime = RTC->time() + kBadEncoderTime;

  mFlagMask = 0;
  mFlagError = 0;
  for (int i=0; i<kNumFlagTypes; ++i) {
    mStopFlag[i] = 0;
  }

  meanEncoderErr = 0;

  // open file and find object
  InputFile in("AXIS", axisName, AXIS_VER, NL_FAIL, NULL, fp);

  key = in.nextLine("MOTOR:", "<motor name>");   // get motor name
  strncpy(componentName,key,kMaxLen);
  namePtr = noWS(componentName);
  motor = (Motor *)signOut(namePtr);    // get motor and sign it out
  if (!motor) quit(namePtr,"Axis");

  key = in.nextLine("LOADCELL:", "<loadcell name>");  // get loadcell name
  strncpy(componentName,key,kMaxLen);
  namePtr = noWS(componentName);
  // create new load cell and sign it out
  loadcell = (Loadcell *)signOut(new Loadcell(namePtr));
  if (!loadcell) quit(namePtr,"Axis");

  in.setErrorFlag(NL_QUIET);
  key = in.nextLine("ENCODER:"); // get encoder name
  in.setErrorFlag(NL_FAIL);
  if (key) {
    strncpy(componentName,key,kMaxLen);
    namePtr = noWS(componentName);
    encoder = (Encoder *)signOut(new Encoder(namePtr));
    if (!encoder) quit(namePtr,"Axis");
    encoderStep = fabs(encoder->getStepSize());// get cm per step
  } else {
    encoder = NULL;
    encoderStep = 0.5;
  }
  setAccuracy();

  // look for stop flags
  in.setErrorFlag(NL_QUIET);
  for (;;) {
    int flag_type;

    // look for a flag specification
    for (flag_type=0; flag_type<kNumFlagTypes; ++flag_type) {
      char buff[64];
      strcpy(buff,sFlagTypeStr[flag_type]);
      strcat(buff,":");
      key = in.nextLine(buff);
      if (key) break;
    }
    if (!key) break;

    int n = atoi(noWS(key));
    if (!encoder) {
      qprintf("Axis %s needs encoder to use flags!\n",
              getObjectName());
      quit("aborting");
    } else if (n<0 || n>=4) {
      qprintf("Invalid flag number %d for Axis %s %s",
              n, getObjectName(), sFlagTypeStr[flag_type]);
      quit("aborting");
    } else if (mFlagMask & (0x01<<n)) {
      qprintf("Flag %d duplicated in Axis %s\n",
              n, getObjectName());
      quit("aborting");
    } else {
      // add this bit to the flag mask
      mFlagMask |= (0x01 << n);
      // set the individual flag type bit
      mStopFlag[flag_type] |= (0x01 << n);
    }
  }
  in.setErrorFlag(NL_FAIL);

  length = in.nextFloat("LENGTH:", "<unstretched length>");
  if (encoder) meanEncoderErr = in.nextFloat("ENCODER_ERROR:","<mean encoder error>");
  alarmStatus = (int)(in.nextFloat("ALARM_STATUS:","<0=none,1=warned,2=stopped>") + 0.5);
  alarmAction   = (int)(in.nextFloat("ALARM_ACTION:","0")+0.5);
  alarmTension  = in.nextFloat("ALARM_TENSION:","0");

  deadLength = in.nextFloat("DEADLENGTH:", "<unchanging length>");
  elasticity = in.nextFloat("ELASTICITY:", "<rope elasticity 1/N>");
  in.setErrorFlag(NL_QUIET);
  elasticity2= in.nextFloat("NON_LINEAR_ELASTICITY:","0");
  in.setErrorFlag(NL_FAIL);

  minTension    = in.nextFloat("MIN_TENSION:");
  maxTension    = in.nextFloat("MAX_TENSION:");
  conTension    = in.nextFloat("CONTROL_TENSION:");
  minConTension = in.nextFloat("MIN_CONTROL_TENSION:");
  maxConTension = in.nextFloat("MAX_CONTROL_TENSION:");

  // optional spring constants
  in.setErrorFlag(NL_QUIET);
  minSpringTension = in.nextFloat("MIN_SPRING_TENSION:","0");
  maxSpringTension = in.nextFloat("MAX_SPRING_TENSION:","0");
  springStretch    = in.nextFloat("SPRING_STRETCH:","0");

  // calculate spring constant
  if (!springStretch) {
    springConstant = 0;
  } else {
    if (minSpringTension > maxSpringTension) {
      quit("AXIS: MIN_SPRING_TENSION must be less than MAX_SPRING_TENSION");
    }
    float diff = maxSpringTension - minSpringTension;
    if (!diff) springConstant = 1e6;
    else springConstant = springStretch / diff;
  }

  dbaseLength = length;
  oldTension = loadcell->getTension();
  oldLength = (deadLength+length) * getStretchFactor(oldTension);
  oldRaw = 0;
  calcEffLength();    // calculate effective rope length

  time = RTC->time();

  pulleyRadius = in.nextFloat   ("RADIUS:", "0");
  xoff         = in.nextThreeVec("OFFSET:", "0 0 0");
  topRadius    = in.nextFloat   ("TOP_RADIUS:","0");
  pulleyAxis   = in.nextThreeVec("PULLEY_AXIS:","0 1 0");

  in.setErrorFlag(NL_FAIL);
  xtop         = in.nextThreeVec("TOP:", "<pulley position>");

  // set flag indicating whether this axis in the the heavy water
  float neckRadius = av->getNeckRadius();
  heavyWaterFlag = (xtop[X]*xtop[X]+xtop[Y]*xtop[Y] < neckRadius*neckRadius);

  // construct the manipulator rope
  if (xbot) {
    manRope = new ManipulatorRope(
                        xtop, xbot,
                        av->getBlockRadius(axisName),
                        av->getBlockOffset(axisName),
                        pulleyAxis,
                        xoff.norm(),
                        pulleyRadius,
                        topRadius,
                        *av);
  } else {
    manRope = new ManipulatorRope(
                        xtop,
                        pulleyAxis,
                        topRadius,
                        *av);
  }
  if (!manRope) quit("manRope","Axis::Axis");

  if (fpOut) {
    in.leaveOpen(); // leave input file open if file is requested
    *fpOut = in.getFile();  // return file pointer
  }
} // end of Axis-from-database constructor

Axis::~Axis()
{
  signIn(motor);    // sign motor back in

  if (encoder) delete encoder;
  delete loadcell;
  delete manRope;
}

void Axis::monitor(char *outStr)
{
  static char *modeStr[] = { "IDLE","STEP","TENSION", "VELOCITY" };

  outStr += sprintf(outStr,"Status:  %s\n", modeStr[getAxisMode()]);
  outStr += sprintf(outStr,"Tension:        %7.2f (%7.2f)\n", getTension(), getDesiredTension());
  outStr += sprintf(outStr,"Rope Length:    %7.2f\n", getLength());
  if (encoder) outStr += sprintf(outStr,"Encoder error:  %7.2f\n", getEncoderError());
  if (encoder) outStr += sprintf(outStr,"Mean error:     %7.2f\n", meanEncoderErr);
  outStr += sprintf(outStr,"Motor velocity: %7.2f\n", motor->getVelocity());
  if (getAxisMode() == kAxisTension) {
    outStr += sprintf(outStr,"Desired velocity:%6.2f\n", desiredVelocity);
  }
  if (mFlagMask) {
    unsigned flagStatus = encoder->getStatus();
    outStr += sprintf(outStr,"Flags: %d %d %d %d\n",
                      (flagStatus) & 0x01,
                      (flagStatus>>1) & 0x01,
                      (flagStatus>>2) & 0x01,
                      (flagStatus>>3) & 0x01 );
  }
  outStr += sprintf(outStr,"Alarm status: %d (peak=%.2f N, limit=%.2f N)\n",
                    alarmStatus,highTension,alarmTension);
}

void Axis::tensionLimits()
{
  sprintf(display->outString,"Low Tension: %7.2f\n"
                             "High Tension: %7.2f",
                             getMinTension(), getMaxTension());
  display->message();
}

// get rope stretch factor for a given tension
// (all rope elasticity calculations are localized to this routine)
double Axis::getStretchFactor(double tension)
{
  double  factor = 1 + elasticity * tension;  // linear stretch factor

  // apply non-linear tension coefficient
  if (tension>0 && elasticity2) factor += elasticity2 * sqrt(tension);

  return(factor);
}

int Axis::atDesiredPosition()
{
  return(fabs(desiredPosition - getLength()) <= accuracy);
}

void Axis::rampDown() // ramp down speed to zero as quickly as possible
{
  stickyFlag = 0;     // cancel sticky mode
  scaleSpeed(1.0);    // restore original acceleration profile
  motor->setDesiredVelocity(0); // ramp down to zero velocity
  motor->setControlMode(VELOCITY_MODE); // set velocity mode
  setAxisMode(kAxisIdle); // make axis idle
}

void Axis::stop(void)
{
  stickyFlag=0;
  setAccuracy();  // reset accuracy to default
  motor->stop();
  // added to prevent double stops on axis errors 08/16/00 - PH
  setAxisMode(kAxisIdle);
}

void Axis::poll(void)
{
  time = RTC->time();                // get real time

  float curLength = getLength();

  // check digital flags to see if we must stop
  if (axisMode != kAxisIdle) {
    if (!motor->isOn()) {
      // make the axis idle if the motor has turned off for whatever reason
      setAxisMode(kAxisIdle);
    } else if (checkFlags(motor->getDirection())) {
      // stop motor if required by flags
      stop();
      sprintf(display->outString,"Flag stop: %s %s",
              sFlagTypeStr[mFlagError-1],getObjectName());
      display->error();
      writeCmdLog();
    }
  }

  // check for high tension alarm
  double curTension = getTension();
  if (curTension > highTension) {
    highTension = curTension;
    if (alarmAction && curTension>alarmTension && !alarmStatus) {
      alarmStatus = alarmAction;    // set alarm status
      // update database file

      sprintf(display->outString,"Tension alarm: %s %.1f N",
              getObjectName(), curTension);
      display->error();
      writeCmdLog();
      // stop axis (just in case this is lower than the maximum tension)
      if (axisMode!=kAxisIdle && alarmAction==2) {  // stop the axis
        stop();
        display->error("Alarm stop!");
      }
      OutputFile out("AXIS",getObjectName(),AXIS_VER);
      out.updateInt("ALARM_STATUS:", alarmStatus);
    }
  }
  // check for high/low tension
  if (time > mBadTensionTime) {
    mBadTensionCount = 0; // reset bad tension counter if no tension errors for 1 sec
  }
  if (curTension>maxTension || curTension<minTension) {
    if (mBadTensionCount < kBadTensionCount &&
      ++mBadTensionCount == kBadTensionCount)
    {
      // set bad tension value
      mBadTensionValue = curTension;
    }
    mBadTensionTime = time + kBadTensionTime; // save time of last bad tension reading
  }
  // check for encoder errors
  if (time > mBadEncoderTime) {
    mBadEncoderCount = 0;
  }
  if (isEncoderError()) {
    if (mBadEncoderCount < kBadEncoderCount) {
      ++mBadEncoderCount;
    }
    mBadEncoderTime = time + kBadEncoderTime;
  }
  checkErrors(0);
/*
** step mode
*/
  if (axisMode == kAxisStep) {
    if (encoder) {
      float positionError = desiredPosition - curLength;

      if (motor->isRunning()) {
        motor->setDesiredPosition(motor->getCurrentPosition() + positionError);
      } else if (fabs(positionError) > accuracy) { // we moved off endpoint
        // set motor running at a slower speed to do small corrections
        scaleSpeed(kMotorCorrScale);
        // start up motor again
        moveMotor(positionError);
      } else if (!stickyFlag) {
        stop(); // stop the motor if we aren't in sticky mode
      }
    } else if (!motor->isRunning() && !stickyFlag) {
      stop();
    }
  }
/*
** velocity mode
*/
  else if (axisMode == kAxisVelocity)
  {
    motor->setDesiredVelocity(desiredVelocity);
    if (!stickyFlag && !motor->isRunning()) stop();
  }
/*
** tension mode
*/
  else if (axisMode == kAxisTension)
  {
    float tensionError = getTension() - desiredTension;
    if (tensionUpFlag) {
      // ignore tension errors if tension is high
      if (tensionError > 0) tensionError = 0;
      // otherwise reset tension flag
      else tensionUpFlag = 0;
    }
    if (fabs(tensionError) > fabs(getTension()) * kMaxTensionScale + kTensionThreshold) {
      float theVelocity = desiredVelocity + kTVCoef * tensionError;
      // if no desired velocity, then limit to cruise speed
      if (!desiredVelocity) {
        float theSpeed = fabs(theVelocity);
        if (theSpeed > motor->getCruiseSpeed()) {
          theSpeed = motor->getCruiseSpeed();
          if (theVelocity < 0) theVelocity = -theSpeed;
          else theVelocity = theSpeed;
        }
      }
      motor->setDesiredVelocity(theVelocity);
    } else {
      motor->setDesiredVelocity(desiredVelocity);
      // stop motor if it isn't running and our tension mode isn't sticky
      if (!stickyFlag && !motor->isRunning()) stop();
    }
  }
}

// set rope to minimum length
void Axis::minLength()
{
  if (getRope()->ropeType == ManipulatorRope::centralRope) {
    setLength(0);
  } else {
    setLength(getRope()->getMinimumLength());
  }
}

// reset alarm status
void Axis::alarmReset()
{
  if (alarmStatus) {
    alarmStatus = 0;
    display->message("Alarm reset");
    OutputFile out("AXIS",getObjectName(),AXIS_VER);
    out.updateInt("ALARM_STATUS:", alarmStatus);
  } else {
    display->message("No alarm present - tension reset only");
  }
  sprintf(display->outString,"Peak tension was .2f N", highTension);
  display->message();
  highTension = 0;
}

void Axis::alarmActionProc(char *cmd)
{
  int val;
  int n = sscanf(cmd,"%d",&val);
  static char *action_str[] = { "no action","warn","stop" };

  if (n == 1) {
    if (val>=0 && val<=2) {
      alarmAction = val;
      sprintf(display->outString,"Alarm action changed to %d (%s)",
              val, action_str[val]);
      display->message();
      OutputFile out("AXIS",getObjectName(),AXIS_VER);
      out.updateLine("ALARM_ACTION:", cmd);
    } else {
      display->message("Invalid action code -- must be 0, 1 or 2");
    }
  } else {
    sprintf(display->outString,"Current alarm action is %d (%s)",
            alarmAction, action_str[alarmAction]);
    display->message();
    display->message("Use \"<Axis> alarmAction #\" to set the alarm action");
  }
}

void Axis::alarmTensionProc(char *cmd)
{
  float t;
  int n = sscanf(cmd,"%f",&t);
  if (n==1) {
    alarmTension = t;
    OutputFile out("AXIS",getObjectName(),AXIS_VER);
    out.updateFloat("ALARM_TENSION:", t);
    sprintf(display->outString,"Alarm tension changed to %f",t);
  } else {
    sprintf(display->outString,"Alarm tension currently set to %f",alarmTension);
  }
  display->message();
}

void Axis::errorStopProc(char *cmd)
{
  static char *delim = " \t\n\r";
  char *pt = strtok(cmd, delim);
  if (!pt) {
    sprintf(display->outString,"Error stop is %s",mErrorStopOn ? "on" : "off");
  } else if (!strcasecmp(pt,"on")) {
    if (mErrorStopOn) {
      sprintf(display->outString,"Error stop is already on");
    } else {
      sprintf(display->outString,"Error stop turned back on");
      mErrorStopOn = 1;
    }
  } else if (!strcasecmp(pt,"off")) {
    if (!mErrorStopOn) {
      sprintf(display->outString,"Error stop is already off");
    } else {
      sprintf(display->outString,"Error stop turned off!");
      mErrorStopOn = 0;
    }
  } else {
    sprintf(display->outString,"Must specify either ON or OFF");
  }
  display->message();
}

void Axis::setLength(float len) // set lengths
{
  float previousLength = length;

  // set total streached rope length
  oldLength = len + deadLength;
  // calculate unstreached length past pulleys
  length = oldLength / getStretchFactor(oldTension) - deadLength;

  if (encoder) {
    encoder->setPosition(oldLength);  // update the encoder
    resetEncoderError();  // reset the encoder error if lengths set
  } else {
    motor->initPosition(oldLength);
  }
  calcEffLength();      // update our last length calculation
  updateDatabase();     // update our database with new value

  sprintf(display->outString,"%s length changed by %.2f",
          getObjectName(),length-previousLength);
  display->error();
  writeCmdLog();
}

void Axis::resetEncoderError() // force agreement between motor/encoder/loadcell
{
  if (mErrorStopStatus) {
    sprintf(display->outString,"%s error stop status reset",getObjectName());
    display->error();
    mErrorStopStatus = NO_CAUSE;
  }
  float oldError = getEncoderError();
  motor->initPosition(oldLength - getSpringLength(oldTension));
  meanEncoderErr = 0;
  sprintf(display->outString,"%s encoder error reset (was %.2f)",
          getObjectName(), oldError);
  display->error();
  writeCmdLog();
}

void Axis::toggleSticky(int showMsg)
{
  stickyFlag = !stickyFlag;

  if (showMsg) {
    if (stickyFlag) {
      sprintf(display->outString,"Sticky mode inverted for next axis command");
    } else {
      sprintf(display->outString,"Sticky mode reset to normal");
    }
    display->message();
  }
}

// get extra rope length due to spring stretch at given tension
float Axis::getSpringLength(float tension)
{
  if (tension >= maxSpringTension) {
    return(springStretch);
  } else if (tension < minSpringTension) {
    return(0);
  } else {
    return((tension-minSpringTension) * springConstant);
  }
}

float Axis::getEncoderError(float tension)
{
  if (!encoder) return(0);
  return(encoder->length() - motor->getPosition() - getSpringLength(tension)

          - meanEncoderErr);
}

int Axis::isEncoderError()  // is the encoder error too large?
{
  if (encoder) {
    float encoderError = getEncoderError() ;
    // encoder error is too large
    if (fabs(encoderError) > kMaxEncoderError) {
      // check for tension error within limits
      if (encoderError > 0) {
        encoderError = getEncoderError(oldTension + kMaxTensionError);
      } else {
        encoderError = -(getEncoderError(oldTension - kMaxTensionError));
      }
      if (encoderError > kMaxEncoderError) {
        return(1);  // error too large
      }
    }
  }
  return(0);  // acceptable amount of error
}

// calculate effective rope length
void Axis::calcEffLength()
{
  float tmp = (deadLength+length) * getStretchFactor(oldTension) - deadLength;

  effLength = tmp;
}

double Axis::getLength(void) // get tension-corrected length
{
  double curLength;
  double curTension = loadcell->getTension();

  if (encoder) curLength = encoder->length();
  else curLength = motor->getPosition();

  if (curLength != oldLength || curTension != oldTension) {

    double movement = curLength - oldLength;
    double averageTension = (oldTension + curTension) * 0.5;
    double dl = movement / getStretchFactor(averageTension);

    if (fabs(movement) > 10) {
      sprintf(display->outString,"WARNING: %s length jumped by %.1f cm (was %.1f, 0x%.4x->0x%.4x)",
              getObjectName(),movement, oldLength, oldRaw, encoder->getUncalibrated());
      display->error();
    }
    length += dl;               // increment unstretched length
    oldLength = curLength;      // update stretched length
    oldTension = curTension;  // save tension
    if (oldTension < 0) oldTension = 0;      // ignore low readings

    calcEffLength();
//
// Allow slow drift of encoder error because of movement
//
    if (encoder) {
      float allowedDrift = fabs(movement) * kAllowedDrift;
      if (getEncoderError() > 0) {
        meanEncoderErr +=allowedDrift;
      } else {
        meanEncoderErr -= allowedDrift;
      }
    }
  }

  oldRaw = encoder->getUncalibrated();

  return(effLength);
}


void Axis::scaleSpeed(float scale)
{
  if (scale < kMinMotorScale) scale = kMinMotorScale;
  motor->setCruiseSpeed(scale * motor->getNormSpeed());
  motor->setAcceleration(scale * motor->getNormAccel());
}

void Axis::decommand(void)  // force Hardware subunits to refuse commands
{
  commandOff();
  motor->commandOff();
  if (encoder) encoder->commandOff();
  loadcell->commandOff();
}

void Axis::recommand(void)  // enable Hardware subunit command processing
{
  commandOn();
  motor->commandOn();
  if (encoder) encoder->commandOn();
  loadcell->commandOn();
}

void Axis::tensionActivate(int upOnly) // activate for tension control
{
  if (checkErrors()) {
    tensionUpFlag = upOnly; // flag to never decrease tension until setpoint reached
    desiredVelocity = 0;    // initialize desired velocity to zero
    setAxisMode(kAxisTension);  // set axis control mode
    motor->setControlMode(VELOCITY_MODE);  // set in velocity control mode
    motor->setDesiredVelocity(0);   // set initial desired velocity to zero
    motor->start();   // turn the motor on and make it controllable
  }
}

void Axis::velocityActivate() // activate for velocity control
{
  if (checkErrors()) {
    desiredVelocity = 0;    // initialize desired velocity to zero
    setAxisMode(kAxisVelocity); // set axis control mode
    motor->setControlMode(VELOCITY_MODE);  // set in velocity control mode
    motor->setDesiredVelocity(0);   // set initial desired velocity to zero
    motor->start();   // turn the motor on and make it controllable
  }
}

void Axis::up(char* commandString)
{
  if (checkErrors()) {
    setAxisMode(kAxisStep); // set step mode
  
    float distance=atof(commandString);
    sprintf(display->outString," Distance up: %6.3f cm\n            ",distance);
    display->message();
  
    desiredPosition=getLength()-distance;
    startPosition = getLength();
    moveMotor(desiredPosition-startPosition);
  }
};

void Axis::down(char* commandString)
{
  if (checkErrors()) {
    setAxisMode(kAxisStep);   // set step mode
  //  decommand();              // lock out commands for sub-objects
    motor->setCurrentHsteps(0L);
  
    float distance=atof(commandString);
    sprintf(display->outString," Distance down: %6.3f cm\n           ",distance);
    display->message();
  
    desiredPosition=getLength()+distance;
    startPosition = getLength();
    moveMotor(desiredPosition - startPosition);
  }
}


int Axis::isBadTension()
{
  return(mBadTensionCount == kBadTensionCount);
}

int Axis::isLowTension()
{
  return(mBadTensionCount == kBadTensionCount &&
         mBadTensionValue < minTension);
}

int Axis::isHighTension()
{
  return(mBadTensionCount == kBadTensionCount &&
         mBadTensionValue > maxTension);
}

int Axis::checkErrors(int show_msg)
{
  int allOK;

  // make sure our hardware is live
  if (!motor->avrLive(0) || !encoder->avrLive(0) || !loadcell->avrLive(0)) {
    if (axisMode!=kAxisIdle) {
      stop();
      show_msg = 1; // always show message if we stopped our motor
    }
    if (show_msg) {
      display->error();
      writeCmdLog();
    }
    allOK = 0;
  // check for outside tension limits - PH 05/26/00
  } else if (mBadTensionCount==kBadTensionCount && mErrorStopOn) {
    // stop the motor if necessary
    if (axisMode!=kAxisIdle) {
      stop();
      show_msg = 1; // always show message if we stopped our motor
    }
    if (show_msg) {
      // set tension stop status
      if (mBadTensionValue < minTension) {
        mErrorStopStatus = LOW_TENSION_STOP;
      } else {
        mErrorStopStatus = HIGH_TENSION_STOP;
      }
      // print message if we were running or going to start
      sprintf(display->outString,"Tension stop: %.2f %s",mBadTensionValue,getObjectName());
      display->error();
      writeCmdLog();
    }
    
    allOK = 0;  // tension is NOT OK -- return zero
  } else if (mBadEncoderCount==kBadEncoderCount && mErrorStopOn) {
    // stop the motor if necessary
    if (axisMode!=kAxisIdle) {
      stop();
      show_msg = 1; // always show message if we stopped our motor
    }
    if (show_msg) {
      // set encoder stop status
      mErrorStopStatus = AXIS_ERROR_STOP;
      // print message if we were running or going to start
      sprintf(display->outString,"Encoder error stop: %s",getObjectName());
      display->error();
      writeCmdLog();
    }
    allOK = 0;  // encoder is NOT OK -- return zero
  } else {
    allOK = 1;  // all is OK -- return non-zero
  }
  return(allOK);
}

void Axis::setAxisMode(AxisMode mode)
{
  if (axisMode != mode) {
    if (axisMode == kAxisIdle) {
      mErrorStopStatus = NO_CAUSE;    // reset tension stop status when leaving idle mode
    }
    axisMode = mode;
    if (axisMode != kAxisIdle) {
      decommand();              // lock out commands for sub-objects
    } else {
      recommand();
      mErrorStopOn = 1;       // turn tension stop back on
    }
  }
  numReversals = 0;   // zero reversal counter
  reverseFlag = 0;    // initialize reverse flag
}

void Axis::to(float arg)
{
  setAxisMode(kAxisStep);

  desiredPosition = arg;
  startPosition = getLength();
  moveMotor(desiredPosition - startPosition);
}

// return non-zero if motor should NOT run in specified direction
int Axis::checkFlags(int dir)
{
  unsigned flagStatus = encoder->getStatus();

  if (flagStatus & mFlagMask) {
    // check the stop flag first
    if (flagStatus & mStopFlag[kStopFlag]) {
      mFlagError = kStopFlag + 1; // motor must stop
    } else if ((dir > 0) && (flagStatus & mStopFlag[kHighFlag])) {
      mFlagError = kHighFlag + 1; // stop moving in positive direction
    } else if ((dir < 0) && (flagStatus & mStopFlag[kLowFlag])) {
      mFlagError = kLowFlag + 1;  // stop moving in negative direction
    } else {
      mFlagError = 0;
    }
  } else {
    mFlagError = 0;
  }
  return(mFlagError); // return error code
}

void Axis::moveMotor(float dist)
{
  // don't move on a 'stop' alarm
  if (alarmStatus > 1) {
    sprintf(display->outString,"Axis has alarm status %d -- can not move",
            alarmStatus);
    display->message();
    return;
  }
  if (checkErrors()) {
    int revFlag = (dist > 0) ? 1 : -1;
  
    if (checkFlags(revFlag)) {
      sprintf(display->outString,"Flag stop: %s %s",
              sFlagTypeStr[mFlagError-1],getObjectName());
      display->error();
      writeCmdLog();
    } else {
      if (reverseFlag != revFlag) {
        if (reverseFlag) ++numReversals;
        reverseFlag = revFlag;
      }
      motor->changePosition(dist);
    }
  }
}

void Axis::setMaxSpeed(char* commandString)
{
  motor->setCruiseSpeed(commandString);  // pass speed setting on to motor
}

void Axis::updateDatabase(void)
{
  // open output database file
  OutputFile out("AXIS",getObjectName(),AXIS_VER);

  // this is a little bit simpler now :)  - PH 01/31/97

  out.updateFloat("LENGTH:", length);
  if (encoder) out.updateFloat("ENCODER_ERROR:", meanEncoderErr);

  dbaseLength = length;
}
