//
// Revisions:
//
// 01/13/97 PH  - Made period floating point
//              - Added SetAccel, InitPos, GetPos and SetPos cmds
//
// 01/17/97 PH  - Removed all knowledge of rope lengths
//              - Motor now goes to where you tell it
//              - Motor acceleration is now linear
//              - Cleaned up code and removed unnecessary stuff
//
// Notes:    1) A "half step" in software terminology is a single
//              transition of the stepper motor input.  But since
//              the stepper motor actually steps on a rising edge,
//              it takes two "half steps" to step the motor once.
//
//           2) All velocities are signed.  All speeds are not.
//              All Hstep variables are signed.
//
//           3) All positions are read out in user units, the conversion
//              for which is read from the input data file.
//
//           4) The 3 routines start(), hold() and setVelocity() are the
//              only routines that are allowed to update the driving
//              hardware (via source) while the motor is running.
//              If any other routines change source, then our software
//              step counter will lose synchronization.
//
//           5) For the rope motors, the number of steps per cm
//              (in the MOTOR.DAT file) is calculated from the gearing
//              ratio (96:1) times the number of motor steps per
//              revolution (400) divided by the number of cm per
//              revolution (pi times diameter of <~10 cm plus the pitch
//              of the threads on the drum of 0.3175cm).
//

#include "motor.h"
#include "display.h"
#include "ieee_num.h"
#include "avr32.h"

#define CLOCK_FREQ            5e6   // motor clock rate
#define MIN_HSTEPS_PER_SEC    23
#define MAX_HSTEPS_PER_SEC    10000

// turn windings off at regular intervals
const double        kWindingsOffInterval = 600; // every 10 minutes
const int           kBuffSize = 128;

extern Display *display; // defined in motorcon.cpp

// static member declarations
Motor::Speed      Motor::cmd_speed;
Motor::SetAccel   Motor::cmd_setaccel;
Motor::InitPos    Motor::cmd_initpos;
Motor::SetPos     Motor::cmd_setpos;
Motor::ChangePos  Motor::cmd_changepos;
Motor::StopCmd    Motor::cmd_stop;
Motor::CycleCmd   Motor::cmd_cycle;


Command *Motor::commandList[MOTOR_COMMAND_NUMBER] = {
  &cmd_speed,
  &cmd_setaccel,
  &cmd_initpos,
  &cmd_setpos,
  &cmd_changepos,
  &cmd_stop,
  &cmd_cycle,
};


Motor::Motor(char* name,                   // motor name
      AVR32 *avr,                     // timer card for source clock
      char *aChan):  // AVR motor channel name
             Controller("Motor",name), AVRChannel(avr, aChan)
{
  float position;
  int directionbit;                 // PIA output bit for motor direction
  int awobit;                       // PIA output bit for all-windings-off

  desiredHsteps    = 0;
  startHsteps      = 0;
  totalHsteps      = 0;
  currentDir       = 0;
  cycleDist        = 0;
  invertDir        = 0;

  motorStatus      = kMotorOff;
  currentVelocity  = 0;
  maxAccel         = 10;
  normAccel        = 5;
  curAccel         = 5;
  controlMode      = STEP_MODE;
  hstep_conv       = 1.0;
  maxSpeed         = 3000;
  minSpeed         = MIN_HSTEPS_PER_SEC;
  startSpeed       = MIN_HSTEPS_PER_SEC * 4;
  cruiseSpeed      = maxSpeed;
  normSpeed        = maxSpeed;

  desiredVelocity  = 0.f;
  currentVelocity  = 0.f;
  lastSpeed        = 0.f;
  lastStepTime.low = lastStepTime.high = 0;

  strncpy(tag,name,99);

  // initialize for reading of parameters
  InputFile in("MOTOR",name,3.00);

  // get all of our parameters from the data file
  position   = in.nextFloat("POSITION:","motor position");
  hstep_conv = 2.0 * in.nextFloat("STEPS_PER_UNIT:","motor steps per user unit");
  if (hstep_conv < 0) {
    hstep_conv = -hstep_conv;
    invertDir = 1;
  }
  char *pt = in.nextLine("UNITS:","units string");
  pt = noWS(pt);  // get rid of white space
  unitString = new char[strlen(pt)+1];
  if (!unitString) quit("unitString","Motor::Motor()");
  strcpy(unitString, pt);

  minSpeed   = in.nextFloat("MIN_SPEED:","minimum speed units/s");
  startSpeed = in.nextFloat("START_SPEED:","start speed units/s");
  normSpeed  = in.nextFloat("CRUISE_SPEED:","cruise speed units/s");
  maxSpeed   = in.nextFloat("MAX_SPEED:","maximum speed units/s");
  normAccel  = in.nextFloat("NORM_ACCEL:","normal accel units/sec/sec");
  maxAccel   = in.nextFloat("MAX_ACCEL:","maximum accel units/sec/sec");

  // check to make sure our constants are all within range
  if (hstep_conv <= 0) quit("invalid step conversion constant\n");
  if (minSpeed*hstep_conv < MIN_HSTEPS_PER_SEC) {
      qprintf("%s minimum motor speed is too low\nMinimum allowed motor speed is %f\n",
        getObjectName(), MIN_HSTEPS_PER_SEC/hstep_conv);
        quit();
  }
  if (maxSpeed*hstep_conv > MAX_HSTEPS_PER_SEC) {
      qprintf("%s maximum motor speed is too high\nMaximum allowed motor speed is %f\n",
        getObjectName(), MAX_HSTEPS_PER_SEC/hstep_conv);
    quit();
  }
  if (startSpeed < minSpeed || startSpeed>maxSpeed) {
      qprintf("%s start speed out of range\n", getObjectName());
    quit();
  }
  if (normSpeed < startSpeed || normSpeed>maxSpeed) {
      qprintf("%s cruise speed out of range\n",getObjectName());
    quit();
  }
  if (normAccel > maxAccel) {
      qprintf("%s normal acceleration is greater than maximum\n",getObjectName());
    quit();
  }
  if (normAccel <= 0) {
      qprintf("%s normal acceleration must be > 0\n",getObjectName());
    quit();
  }

  totalHsteps  = position * hstep_conv; // initialize current position
  dbaseHsteps  = totalHsteps; // update database half steps
  curAccel     = normAccel;
  cruiseSpeed  = normSpeed;

  windings(0);
}

void Motor::monitor(char *outStr)
{
  static char *status_str[] = { "OFF", "RUNNING", "ON" };
  static char *mode_str[] = { "<none>", "STEP", "VELOCITY" };

  outStr += sprintf(outStr,"Status:   %s\n", status_str[motorStatus]);
  outStr += sprintf(outStr,"Mode:     %s\n", mode_str[getControlMode()]);
  outStr += sprintf(outStr,"Velocity: %.2f %s/SEC\n", currentVelocity, unitString);
  outStr += sprintf(outStr,"Position: %.3f\n",getPosition());
  outStr += sprintf(outStr,"Cruising: %.2f %s/SEC\n", cruiseSpeed, unitString);
  AVRChannel::monitor(outStr);
}

void Motor::windings(int on)
{
    avrCommand(on ? "on 1" : "on 0");
    lastSpeed = 0;  // forget last speed whenever we turn on/off windings
}

int Motor::isTag(char *s)
{
  char instr[100];
  int i=0;
  strcpy(instr,s);
  while((i<100)&&(toupper(instr[i++])!='\0'));
  if (!strcmp(instr,tag)) return 1;
  else return 0;
}

void Motor::step(long steps)  //move steps
{
  if (steps == 0) return;     // do nothing if no steps to take

  startHsteps    = totalHsteps; // start from current position
  controlMode    = STEP_MODE; // set control mode
  desiredHsteps  = steps;     // save our desired endpoint

  if (steps > 0){
    setDesiredVelocity(cruiseSpeed); // set cruising velocity
  } else {
    setDesiredVelocity(-cruiseSpeed);
  }
  setVelocity();              // start motor running
}

//---------------------------------------------------------
// poll - motor polling routine
//
void Motor::poll(void)
{
  if (avrCommand("stat") < 0) return;    // try again later

  // get current speed/direction/position
  // (response is of the form "OK m0 SPD=+0 POS=0\n")
  char *pt = strstr(avrResponse(), "SPD=");
  if (!pt) return;
  pt += 4;  // skip "SPD="
  if (*pt == '+') {
    currentDir = 1;
  } else if (*pt == '-') {
    currentDir = -1;
  } else {
    return;
  }
  currentVelocity = atoi(pt) / hstep_conv;
  pt = strstr(pt, "POS=");
  if (!pt) return;
  totalHsteps = atoi(pt + 4);
  
  if (controlMode==STEP_MODE && motorStatus) // step mode set and motor running
  {
    long  stepsLeft = currentDir * (desiredHsteps - getCurrentHsteps());
    if (stepsLeft > 0) {
      // calculate desired velocity on acceleration curve
      float speedUp = currentVelocity * currentDir + curAccel * getAvgPollTime();
      // calculate desired velocity on deceleration curve
      float desiredSpeed = sqrt(2.0 * stepsLeft * curAccel / hstep_conv);
      // take the slower of the two speeds
      if (desiredSpeed > speedUp) desiredSpeed = speedUp;
      // don't go slower than minSpeed
      if (desiredSpeed < minSpeed) desiredSpeed = minSpeed;
      // set the desired velocity
      desiredVelocity = currentDir * desiredSpeed;
    } else {
      desiredVelocity = 0;    // done enough steps; stop the motor
    }

    setVelocity();  // change the motor velocity

    if (motorStatus != kMotorRunning) {
      if (cycleDist != 0) {
        cycleDist = -cycleDist;
        changePosition(cycleDist);
        return;
      }
    }
  }
  else if (controlMode==VELOCITY_MODE && motorStatus)       // velocity control
  {
    setVelocity();

  } else if (!motorStatus) {  // motor should be off
    if (RTC->time() > nextWindingsOffTime) {
      windings(0);
    }
  }
}

//---------------------------------------------------------
// turn on motor and start ramping toward desired velocity
//
void Motor::start()
{
  char buff[kBuffSize];

  // do nothing if we are already running
  if (motorStatus == kMotorRunning) return;

  float fv = fabs(desiredVelocity);

  if (fv == 0) {
    setMotorStatus(kMotorOnButNotRunning);
    return;
  } else if (fv < minSpeed) {
    fv = minSpeed;
  } else if (fv > startSpeed) {
    fv = startSpeed;
  }

  setMotorStatus(kMotorRunning);  // we are running now

  // set direction
  int outBit;
  if (desiredVelocity > 0) {
    currentDir = 1;
    outBit = 0;
  } else {
    currentDir = -1;
    outBit = 1;
  }
  sprintf(buff, "%s dir %d;%s on 1", mChannel, outBit, mChannel);
  mAVR->sendCommand(buff);

  currentVelocity = currentDir * fv;
}

//---------------------------------------------------------
// hold motor at current location
//
void Motor::hold()
{
  // if the motor was running, we must stop it and keep
  // track of the remaining steps that it took.
  if (motorStatus == kMotorRunning) {
    
    avrCommand("stop");

    // restore normal cruise speed and acceleration when motor stops
    cruiseSpeed = normSpeed;
    curAccel = normAccel;
    lastSpeed = 0;
  }

  setMotorStatus(kMotorOnButNotRunning);  // change status
}

//---------------------------------------------------------
// Set the velocity to the desired velocity.
// This routine limits the motor speed and acceleration,
// and will start or hold the motor as necessary if the
// requested velocity demands it.
//
void Motor::setVelocity()
{
  if (motorStatus != kMotorRunning) {

    // start the motor if speed is above the mininum
    if (fabs(desiredVelocity) >= minSpeed) {
        start();
        return; // wait until next time to actually run the motor
    }
  }

  float theVelocity = desiredVelocity;
  float theSpeed = desiredVelocity * currentDir; // desired speed relative to current direction

  if (fabs(currentVelocity) < startSpeed * 0.99) {

    // motor is  running slowly enough to stop it if required
    if (theSpeed<minSpeed || currentVelocity*theVelocity<0) {
      // hold motor if we are changing directions or stopping
      hold();
      return;   // wait until next poll to restart motor
    }

    if (theSpeed > startSpeed) {
      theSpeed = startSpeed;
      if (theVelocity < 0) theVelocity = -theSpeed;
      else theVelocity = theSpeed;
    }

  } else {

    // limit the maximum velocity
    float speedLimit = (controlMode==STEP_MODE ? cruiseSpeed : maxSpeed);
    if (theSpeed > speedLimit) {
      theSpeed = speedLimit;
      if (theVelocity < 0) theVelocity = -theSpeed;
      else theVelocity = theSpeed;
    }
  }

  if (lastSpeed != theSpeed) {
    // set the current motor step rate
    char buff[kBuffSize];
    // TEST MAYBE THIS SHOULD BE A "run", NOT "ramp" COMMAND
    // SINCE WE HANDLE THE ACCELERATION HERE?
    sprintf(buff,"%s ramp %d", mChannel, (int)(theSpeed * hstep_conv + 0.5));
    if (!mAVR->sendCommand(buff)) lastSpeed = theSpeed;
  }
}

//=========================================================


//---------------------------------------------------------
// stop - stop the motor and turn off windings
//
void Motor::stop()            // stop motor
{
  if (motorStatus != kMotorOff) {
    hold();                     // hold the motor motion
    setMotorStatus(kMotorOff);  // turn off the motor
    desiredVelocity = 0;        // reset desired velocity
    cycleDist = 0;                // reset run distance
  }
}

//---------------------------------------------------------
// set motor status
//
// the motor windings will be turned off if the status
// is kMotorOff, otherwise they will be turned on
//
void Motor::setMotorStatus(MotorStatus status)
{
  if (motorStatus != status) {
    motorStatus = status;
    if (status == kMotorOff) {
      windings(0);  // turn off motor windings
      commandOn();  // enable commands only when motor is not running
    } else {
      windings(1);  // energize motor windings
      commandOff();
    }
  }
}

//---------------------------------------------------------
// set current motor step position
void Motor::setCurrentHsteps(long chsteps)
{
    startHsteps = totalHsteps + chsteps;
}

//---------------------------------------------------------

// setCruiseSpeed(char *)
// - this also sets the normal cruising speed
void Motor::setCruiseSpeed(char* commandString)  // set motor cruising speed in user-units/s
{
  setCruiseSpeed(atof(commandString));

  sprintf(display->outString," Cruise Speed: %6.3f (%s/SEC)",
          cruiseSpeed, unitString);
  display->message();

  // when the user changes the cruising speed,
  // this is now our new normal speed
  normSpeed = cruiseSpeed;
}

// setCruiseSpeed(float)
// - this does not change the normal cruising speed
void Motor::setCruiseSpeed(float speed)  // set motor cruise speed in units/s
{
  cruiseSpeed=speed;
  if (cruiseSpeed > maxSpeed)
  {
    sprintf(display->outString,"SPEED SET TO ALLOWED MAXIMUM");
    display->message();
    cruiseSpeed = maxSpeed;
  }
}

void Motor::setAcceleration(char* commandString)  // set motor speed in units/s
{
  float acc = atof(commandString);

  if (acc > 0) {
    setAcceleration(acc);
    display->outString[0] = 0;
  } else {
    sprintf(display->outString,"acceleration not specified");
  }
  display->message();
  sprintf(display->outString," Acceleration: %6.3f", curAccel);
  display->message();

  // when the user sets the acceleration, it also changes the normal accel
  normAccel = curAccel;
}

void Motor::setAcceleration(float acc)  // set motor acceleration
{
  if (acc > maxAccel) {
    sprintf(display->outString,"ACCEL SET TO ALLOWED MAXIMUM");
    display->message();
    curAccel = maxAccel;
  } else if (acc <= 0) {
    sprintf(display->outString,"ACCEL MUST BE > 0");
    display->message();
  } else {
    curAccel = acc;
  }
}

void Motor::initPosition(char *inString)  // set current position
{
  initPosition(atof(inString));
}

void Motor::initPosition(float pos)  // set current position
{
  if (pos > 0) {
    totalHsteps = (long)(pos * hstep_conv + 0.5);
  } else {
    totalHsteps = (long)(pos * hstep_conv - 0.5);
  }
  // initialize step count stored in the AVR32
  initChannel();  
  desiredHsteps = getCurrentHsteps();
  updateDatabase();         // update database immediately
}

// perform necessary initialization for this AVR motor channel
int Motor::initChannel()
{
  char buff[kBuffSize];
  // set position and sense of direction/on signals
  sprintf(buff,"%s pos %d;%s dir %c;%s on -", mChannel,
         (int)totalHsteps, mChannel, invertDir ? '-' : '+', mChannel);
  int err = mAVR->sendCommand(buff);
  // close USB connection if we had an error initializing
  if (err) {
    eprintf("Error initializing %s",getObjectName());
    getAVR()->close();
  }
  return err;
}

int Motor::checkInit()
{
  int err = 0;
  // set up necessary AVR32 parameters if it was just re-initialized
  if (mAVR->didInit()) {
     err = initChannel();
  }
  return err;
}

void Motor::setPosition(char *inString) // drive motor to new position
{
  setPosition(atof(inString));
}

void Motor::setPosition(float pos)  // drive motor to new position
{
  step(pos * hstep_conv - totalHsteps);
}

void Motor::changePosition(char *inString)
{
  changePosition(atof(inString));
}

void Motor::changePosition(float pos)
{
  step(pos * hstep_conv);
}

void Motor::cycle(char *inString)
{
  cycleDist = atof(inString);
  changePosition(cycleDist);
}

void Motor::modulus(long num) // limit total steps to the range [0,num]
{
  num *= 2;   // convert to half steps
  if (totalHsteps >= num) {
    totalHsteps %= num;
  } else if (totalHsteps < 0) {
    totalHsteps = (totalHsteps % num) + num;
  }
}

void Motor::updateDatabase(void)
{
  // this is a little bit simpler now :)  - PH 01/31/97

  OutputFile  out("motor",getObjectName(),3.00);  // create output file object

  out.updateFloat("POSITION:", getPosition());

  dbaseHsteps = totalHsteps;  // keep track of what we have in database
}
