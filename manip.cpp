//
// File:        manip.cpp
//
// Description: SNO calibration source manipulator control
//
// Revisions:   (see REVISION.DOC)
//

#define VERSION "5.3"       // current version of this software

//#define TEST_GEOM   // test geometry parameters from testgeom.in
//#define HACK_GEOMETRY // hack to recalculate positions for new geometry

#include "doslib.h"
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <signal.h> // used for signal handler stuff

#include "manrope.h"
#include "manip.h"
#include "polyaxis.h"

#include "keyboard.h" // keyboard input class (extremely dumb, but getting smarter)
#include "display.h"  // display class (even dumber)
#include "outfile.h"
#include "hwio.h"
#include "datain.h" // Reading the wiring from file
#include "r_source.h" // defines the rotating source
#include "umbil.h"
#include "laser.h"
#include "lockout.h"
#include "dispatch.h"
#include "server2.h"
#include "consolec.h"

#ifdef TEST_GEOM
#include "manpos.h"
#endif

/*NOTE: the following line may generate a linker warning.  Ignore the warning*/
//PH extern unsigned int _stklen = 64000u; // make stack bigger (4k is too small)

int nseed = -23423; // random number seed (used by ThreeVector class)

extern int debugging; // used by Hardware class to print latest poll

void Except(int);    // FPE exception handler
int  matherr(struct exception *a);
static void myCleanup();
static void createAllObjects(LList<PolyAxis>& polyList, AV &av);
static void setDisplayObject(PolyAxis *obj);

// global variables
DataInput *dataInput = NULL;
Display   *display   = NULL;
Clock     *RTC       = NULL;
Dispatcher *dispatcher;     // command dispatcher

static LList<PolyAxis>  polyList;   // list of available polyaxis objects (file scope for exception handling)
static PolyAxis         *thePoly;   // displayed polyaxis object
static int              numAxes = -1;// number of axes in current polyaxis object
static LListIterator<Axis>  iaxis;  // iterator for dislayed axis list

static char tmpBuff[BUFFER_LENGTH];
static int  did_mem = 0;

const int           kMaxServers = 5;
static int          numServers = 0;
static Server2    * serverList[kMaxServers];

#include <stdio.h>
//#include <alloc.h>

static ClientDev *getCurrentClient(ClientDev *defClient)
{
  for (int i=0; i<numServers; ++i) {
    ClientDev *theClient = serverList[i]->getCurrentClient();
    if (theClient) return(theClient);
  }
  return(defClient);
}
ClientDev *getCurrentClient()
{
  return(getCurrentClient(&ConsoleClient::console));
}

static void checkHeap(void)
{
/*PH   struct heapinfo hi;
   long   totu=0;
   long   totf=0;
   int    cu=0,cf=0;

   did_mem = 1;
   hi.ptr = NULL;
   while( heapwalk( &hi ) == _HEAPOK ) {
//     sprintf(display->outString,"%s%-7lu", hi.in_use ? "+" : "-", hi.size );
//     display->message();
     if (hi.in_use) {
      totu += hi.size;
      ++cu;
     } else {
      totf += hi.size;
      ++cf;
     }
   }
   sprintf(display->outString,"%7lu bytes free core",(unsigned long)coreleft());
   display->message();
   sprintf(display->outString,"%7lu bytes used in heap (%d blocks)",totu,cu);
   display->message();
   sprintf(display->outString,"%7lu bytes free in heap (%d blocks)",totf,cf);
   display->message();*/
}

static int startCommandFile(char *tok, FILE **cmdFilePt)
{
  // look for a command file of this name
  if (strlen(tok) <= 8) {
    char cmdName[13];
    strcpy(cmdName, tok);
    strcat(cmdName,".cmd");
    FILE *fp = fopen(cmdName,"r");
    if (fp) {
      if (*cmdFilePt) {
        display->message("-- Previous command file terminated --");
        fclose(*cmdFilePt);
      }
      *cmdFilePt = fp;
      return(1);
    }
  }
  return(0);
}

static void setLogging(OutputDev *aDev, char *onOff, char *type, OutputList *aList)
{
  if (onOff && !strcasecmp(onOff,"on")) {
    if (aList->find(aDev) < 0) {
      *aList += aDev;
      sprintf(display->outString,"%s logging turned ON",type);
    } else {
      sprintf(display->outString,"%s logging already ON",type);
    }
    display->message();
  } else if (onOff && !strcasecmp(onOff,"off")) {
    if (aList->find(aDev) >= 0) {
      *aList -= aDev;
      sprintf(display->outString,"%s logging turned OFF",type);
    } else {
      sprintf(display->outString,"%s logging already OFF",type);
    }
    display->message();
  } else {
    sprintf(display->outString,"%s logging to %d outputs",type,aList->count());
    display->message();
    for (LListIterator<OutputDev> iout(*aList); iout<LIST_END; ++iout) {
      sprintf(display->outString,"  %d) %s",iout+1,iout.obj()->getName());
      display->message();
    }
  }
}

//================================================================
// main program
//
// test for reading status word
/*void main()
{
  int i;
  display   = new Display("Test> ",1,kCommandLine,kFirstMessageLine,kLastMessageLine);

  CBAnalogIO *an = new CBAnalogIO("test",0,1,0x320,"NEW");
  CBDigitalIO *dig = new CBDigitalIO("dig",0,0xf4,0x320,"NEW");

  for (i=0; i<15000; ++i) {
    sprintf(display->outString,"0x%.2x\n",dig->readStatus());
    display->message();
  }
} */
static OutputList cmdLogList;   // list of command loggers


int main()
{
  dos_init();

  int         i, j;           // loop counters
  int         updateFlag = 1; // flag to update readouts
  Keyboard    keyboard;       // keyboard object for input
  double      ftime;          // time of this polling cycle
  double      lastTime;       // time of last update
  double      lastReset;      // time of last poll time reset
  double      lastUpdate;     // time of last update
  int         wx,wy;          // old cursor positions
  ThreeVector position;       // source position
  const char  *delim = " \t,:\n\r"; // command line token delimiters
  const double kUpdateTime = 0.1;   // interval for updating each object
  LListIterator<PolyAxis> ipoly(polyList);  // iterator for polyaxis list
  LListIterator<Hardware> ihard(Hardware::getHardwareList());
  FILE        *cmdFile = 0;
  ClientDev   *out;

  OutputList  dataLogList;  // list of data loggers
  OutputList  errorLogList; // list of error loggers

  OutputDevFile cmdLogFile("CMD.LOG",0);
  OutputDevFile dataLogFile("DATA.LOG",1);
  OutputDevFile errorLogFile("ERROR.LOG",0);

  const int   kMaxArgLen = 32;
  const int   kMaxArgs = 10;
  char        cmdArgs[kMaxArgs][kMaxArgLen];
  int         cmdArgNum = 0;
  int         cmdFlag = 0;
  int         autoexec = 1;
  int         cmdTryAgain = 0;
  double      cmdTime = 0;
  int         date_stamp;
  char        cmdCommand[BUFFER_LENGTH];
  OutputDev * cmdOutput = NULL;

  int         lastWatchdogErrorCount = 0;

  // moved all this stuff in from static declarations
  // now the order of construction is defined!!! - PH
  display   = new Display("Manip> ",1,kCommandLine,kFirstMessageLine,kLastMessageLine);
  dispatcher= new Dispatcher(display);
  dataInput = new DataInput;      // the object to read wiring from file
  RTC       = new Clock();

  // set our log file names - PH 12/20/99
  char buff[128];
  time_t thisTime = time(NULL);
  struct tm *tms = localtime(&thisTime);
  date_stamp = (tms->tm_year % 100) * 100 + tms->tm_mon + 1;
  sprintf(buff,"CMD%.4d.LOG", date_stamp);
  cmdLogFile.setName(buff);
  sprintf(buff,"DATA%.4d.LOG", date_stamp);
  dataLogFile.setName(buff);
  sprintf(buff,"ERR%.4d.LOG", date_stamp);
  errorLogFile.setName(buff);

  Laser       laser("N2Laser");    //Laser, dye laser and filter wheels
  AV          acrylicVessel("AV");  // acrylic vessel object

  display->setErrorOutput(&errorLogList);   // set error output list

  // pre-create all Axis, PolyAxis and Server objects
  createAllObjects(polyList, acrylicVessel);

  // set displayed object to the polyaxis with the most connections
  for (i=-1,ipoly=0; ipoly<LIST_END; ++ipoly) {
    j = ipoly.obj()->getNumAxes();  // number of axes in this poly
    if (i < j) {                    // we found a poly with more axes
      thePoly = ipoly.obj();        // use this poly for dislay
      i = j;                        // save the number of axes
    }
  }
  if (!thePoly) quit("No PolyAxis objects!");
  iaxis = thePoly->getAxisList(); // initialize iterator for axis list

  setCleanup(myCleanup);  // set cleanup routine now that our server is open

  debugging = 0;          // default state is no debugging

  cmdLogList += &cmdLogFile;  // write commands to local file by default
  errorLogList += &errorLogFile;  // write errors to local file by default

  writeCmdLog("----- MANIP started -----");

  tzset();  // set the time zone from the TZ environment string

  char commandString[BUFFER_LENGTH];
  char responseString[BUFFER_LENGTH];
  memset(commandString,0,BUFFER_LENGTH);
  memset(responseString,0,BUFFER_LENGTH);

  signal(SIGFPE,Except);      // load signal handler for FPEs

  display->home_cursor();  // prepare for input

  lastTime = lastUpdate = RTC->time();
  lastReset = lastTime - 56;

  ihard = 0;  // initialize hardware list update iterator


#ifdef TEST_GEOM

  FILE *fp = fopen("testgeom.in","rt");
  if (!fp) {
    display->message("Can't open testgeom.in - no geometry test done.");
  } else {
    char  buff[100];
    float lengths[4];
    float tensions[4];
    float t1,t2,t3;
    ThreeVector p1;
    int   count=0;
    ManPos *manpos = thePoly->newManPos();
#ifdef HACK_GEOMETRY
    float minLengthDiff[4];
    float oldMinLength[4] = { 0, 1568.107, 1567.936, 0 };
    for (iaxis=0; iaxis<LIST_END; ++iaxis) {
      minLengthDiff[iaxis] = iaxis.obj()->getRope()->getMinimumLength()
                             - oldMinLength[iaxis];
    }

#endif

    if (!manpos) quit("error");

    while (fgets(buff,100,fp)) {
      if (!memcmp(buff,"Position:",9)) {
        int n = sscanf(buff+10,"%f %f %f",&t1,&t2,&t3);
        if (n != 3) continue;
        p1 = ThreeVector(t1,t2,t3);
      } else if (!memcmp(buff,"Lengths:",8)) {
        int n = sscanf(buff+9,"%f %f %f",lengths,lengths+1,lengths+2);
        if (n != 3) continue;
      } else if (!memcmp(buff,"Tensions:",9)) {
        int n = sscanf(buff+10,"%f %f %f",tensions,tensions+1,tensions+2);
        if (n != 3) continue;
#ifdef HACK_GEOMETRY
        for (iaxis=0; iaxis<LIST_END; ++iaxis) {
          lengths[iaxis] += minLengthDiff[iaxis];
        }
        n = manpos->calcState(lengths,tensions);
        ThreeVector p2 = manpos->getPosition();
        float err = (p1-p2).norm();
        sprintf(display->outString,"Test point %d) %.2f %.2f %.2f (moved %.2f)\n",
                ++count, p2[X], p2[Y], p2[Z],err);
        display->message();
#else
        n = manpos->calcState(lengths,tensions);
        ThreeVector p2 = manpos->getPosition();
        float err = (p1-p2).norm();
        sprintf(display->outString,"Test point %d) err = %.2f ... %s",
                ++count, err, err>.02 ? "ERROR!!!!!" : "OK");
        display->message();
#endif
      }
    }
    delete manpos;
    fclose(fp);
  }
#ifdef HACK_GEOMETRY
  for (iaxis=0; iaxis<LIST_END; ++iaxis) {
    sprintf(display->outString,"%s minlength %.3f",iaxis.obj()->getObjectName(),
            iaxis.obj()->getRope()->getMinimumLength());
    display->message();
  }
  quit("done");
#endif // HACK_GEOMETRY

#endif // TEST_GEOM

  // initialize necessary AVR32 channels before starting polling loop
  Hardware::doCheckInit();
  dataInput->doneInit();

  for (;;) {       // main loop

    ftime = RTC->time();    // time at start of polling cycle
    Hardware::doPoll();     // poll all Hardware objects
    dataInput->keepAlive();     // keep alive necessary hardware

    // check for watchdog errors and shut down all motors on an error
    if (lastWatchdogErrorCount != CBAnalogIO::getWatchdogErrorCount()) {
      lastWatchdogErrorCount = CBAnalogIO::getWatchdogErrorCount();

      int stopCount = 0;

      // stop all polyaxes
      for (ipoly=0; ipoly<LIST_END; ++ipoly) {
        if (ipoly.obj()->isMoving()) {
          ipoly.obj()->stop(1);
          ++stopCount;
        }
      }
      // stop all unattached axes
      LListIterator<Hardware> ih(Hardware::getHardwareList());
      for (ih=0; ih<LIST_END; ++ih) {
        // look for axis types
        Hardware *hwobj = ih.obj();
        if (strcasecmp(hwobj->getClassName(), "Axis")) continue;
        // look for unowned axes
        if (hwobj->getOwner()) continue;
        // stop the axis motion
        if (((Axis *)hwobj)->isRunning()) {
          ((Axis *)hwobj)->stop();
          ++stopCount;
        }
      }
      if (stopCount) {
        display->message("Watchdog error! -- all motors stopped");
      }
    }

    // update hardware objects in sequence
    if (ftime - lastUpdate > kUpdateTime) {
      lastUpdate = ftime;
      if (ihard < LIST_END) {
        if (ihard.obj()->needUpdate()) {
          ihard.obj()->updateDatabase();
        }
        ++ihard;  // update next hardware object next time through loop
      } else {
        ihard = 0;
      }
    }

    // accept new connections and listen for incoming commands
    for (i=0; i<numServers; ++i) {
      serverList[i]->poll();
    }

    /* poll keyboard for command */
    keyboard.poll(dispatcher);

    // insert commands from the command file
    if (cmdFile && !dispatcher->isCommandAvailable() && RTC->time()>=cmdTime) {
      // send output of command file execution to originating host
      display->clearOutputs();
      if (cmdOutput) *display += cmdOutput;

      // get a new command from the file if the last one was executed OK
      if (!cmdTryAgain) {
        cmdCommand[0] = 0;
        if (!fgets(tmpBuff,BUFFER_LENGTH,cmdFile)) {
          // no more commands. end of this command file
          fclose(cmdFile);
          cmdFile = 0;
          display->message("-- Command file end --");
        } else {

          // parse command for arguments
          char *src1 = tmpBuff;
          char *src2;
          char *dst = cmdCommand;
          int  len;
          int  left = BUFFER_LENGTH;

          for (;;) {
            src2 = strchr(src1,'%');
            if (!src2) {
              left -= strlen(src1);
              if (left <= 0) break;
              strcpy(dst, src1);  // copy rest of command
              break;
            }
            // ignore % unless followed by a number
            if (src2[1]<'1' || src2[1]>'9') {
              src1 = src2 + 1;
              continue;
            }
            // do we have enough parameters for this subsitution?
            if (src2[1]-'1' >= cmdArgNum) {
              display->message("Not enough parameters for command:");
              display->message(tmpBuff);
              cmdCommand[0] = 0;
              // end of this command file
              fclose(cmdFile);
              cmdFile = 0;
              display->message("-- Command file aborted --");
              break;
            }
            len = (int)(src2 - src1);
            if ((left-=len) <= 0) break;
            memcpy(dst,src1,len);
            dst += len;
            *dst = 0;   // null terminate it

            // add the parameter
            len = strlen(cmdArgs[src2[1]-'1']);
            if ((left-=len) <= 0) break;
            strcpy(dst,cmdArgs[src2[1]-'1']);
            dst += len;

            src1 = src2 + 2;  // continue at character folling parameter
          }
        }
      }
      cmdTryAgain = 0;

      if (cmdCommand[0]) {
        dispatcher->addCommand(cmdCommand,cmdOutput);
        cmdFlag = 1;  // set flag indicating this is command is from a file
      } else {
        cmdFlag = 0;
      }
    } else if (autoexec) {
      dispatcher->addCommand("autoexec");
      autoexec = 0;
      cmdFlag = 1;
    } else {
      cmdFlag = 0;
    }

    // excecute available commands
    // (this also sets the output list to the associated device)
    int flag = dispatcher->getNextCommandFlag();
    if (dispatcher->getNextCommand(commandString,BUFFER_LENGTH) > 0) {

      // get pointer to device that initiated this command
      OutputDev *commandDevice = display->getOutput(0);

      // ignore 'monitor' commands when logging - PH 06/05/00
      if (!strstr(commandString, "MONITOR")
          && !strstr(commandString, "monitor")
          // 2013-07-30 PH Enable logging of CMD file commands:
          //&& !cmdFlag // don't log command file commands
         )
      {
        char *host;
        if (commandDevice) {
          host = commandDevice->getName();
        } else {
          host = "console";
        }
        writeCmdLog(commandString, host);
      }

      // initialize default response for local commands
      strcpy(responseString, "COMMAND_ACCEPTED");

      // get the command name
      char  *tok = strtok(commandString, delim);
      if (!tok) continue;
//
// commands executed before screen is cleared
//
      if (!strcasecmp(tok,"say"))
      {
        char *msg = strtok(NULL,"");
        if (cmdFlag) {
          // command file is executing, send to originating host
          display->message(msg);
        } else if (display->altOutputs()) { // command is from remote host
          // get pointer to originating host
          out = getCurrentClient(&ConsoleClient::console);
          sprintf(display->outString,"---- %s",msg);
          display->clearOutputs();  // temporarily clear output list
          display->message();   // send to console
          // send to other clients if they exist
          for (i=0; i<numServers; ++i) {
            serverList[i]->outputAllBut(out);
          }
          if (display->altOutputs()) {
            display->message();
            display->clearOutputs();
          }
          *display += out;  // restore output list
        } else {  // command is from console
          // echo to screen
          sprintf(display->outString,"    [%s]",msg);
          display->message();
          // add all servers to output list
          for (i=0; i<numServers; ++i) {
            serverList[i]->outputAllBut(NULL);
          }
          if (display->altOutputs()) {
            sprintf(display->outString,"---- %s",msg);
            display->message(msg);
            display->clearOutputs();
            // poll the server connections to send the message now
            for (i=0; i<numServers; ++i) {
              serverList[i]->doneCommand("HW_ERR_OK");
            }
          } else {
            display->message("Remote host not connected");
          }
        }
        continue; // done with this command
      }
//
// clear screen for local manual commands only
//
      if (flag) {
        display->clearLines(kFirstMessageLine, kLastMessageLine);
      }
//
// commands executed after screen is cleared
//
      if (!strcasecmp(tok,"quit")) {      // quit
        thePoly->stop(1);
        break;
      }
      else if (!strcasecmp(tok,"mem"))    // check memory usage
      {
        checkHeap();
      }
      else if (!strcasecmp(tok,"cmdlog")) // activate command log
      {
        OutputDev *cmdLogger = commandDevice;
        if (!cmdLogger) cmdLogger = &cmdLogFile;
        setLogging(cmdLogger,strtok(NULL,delim),"Command",&cmdLogList);
      }
      else if (!strcasecmp(tok,"datalog"))  // activate data log
      {
        OutputDev *dataLogger = commandDevice;
        if (!dataLogger) dataLogger = &dataLogFile;
        setLogging(dataLogger,strtok(NULL,delim),"Data",&dataLogList);
      }
      else if (!strcasecmp(tok,"errorlog")) // activate error log
      {
        OutputDev *errorLogger = commandDevice;
        if (!errorLogger) errorLogger = &errorLogFile;
        setLogging(errorLogger,strtok(NULL,delim),"Error",&errorLogList);
      }
      else if (!strcasecmp(tok,"debug"))  // toggle debugging
      {
        tok = strtok(NULL,delim);
        if (!strcasecmp(tok,"on")) {
          debugging = 1;
          display->message("Debugging ON");
        } else if (!strcasecmp(tok,"off")) {
          debugging = 0;
          display->message("Debugging OFF");
        } else {
          sprintf(display->outString,"Debugging is currently %s",
                  debugging ? "ON" : "OFF");
          display->message();
          display->message("Specify ON or OFF to change");
        }
      }
      else if (!strcasecmp(tok,"stop")) // stop all motors
      {
/* Ramp down everything now - PH 05/26/98
        thePoly->rampDown();
*/
        // stop all polyaxes
        for (ipoly=0; ipoly<LIST_END; ++ipoly) {
          ipoly.obj()->rampDown();
        }
        // stop all unattached axes
        LListIterator<Hardware> ih(Hardware::getHardwareList());
        for (ih=0; ih<LIST_END; ++ih) {
          // look for axis types
          Hardware *hwobj = ih.obj();
          if (strcasecmp(hwobj->getClassName(), "Axis")) continue;
          // look for unowned axes
          if (hwobj->getOwner()) continue;
          // stop the axis motion
          ((Axis *)hwobj)->rampDown();
        }
        if (cmdFile) {
          fclose(cmdFile);
          cmdFile = 0;
          display->message("-- Command file aborted --");
        }
      }
      else if (!strcasecmp(tok,"halt")) // stop all motors abruptly
      {
/* Stop everything now - PH 04/27/99
        thePoly->stop(1);
*/
        // stop all polyaxes
        for (ipoly=0; ipoly<LIST_END; ++ipoly) {
          ipoly.obj()->stop(1);
        }
        // stop all unattached axes
        LListIterator<Hardware> ih(Hardware::getHardwareList());
        for (ih=0; ih<LIST_END; ++ih) {
          // look for axis types
          Hardware *hwobj = ih.obj();
          if (strcasecmp(hwobj->getClassName(), "Axis")) continue;
          // look for unowned axes
          if (hwobj->getOwner()) continue;
          // stop the axis motion
          ((Axis *)hwobj)->stop();
        }
        if (cmdFile) {
          fclose(cmdFile);
          cmdFile = 0;
          display->message("-- Command file aborted --");
        }
      }
      else if (!strcasecmp(tok,"help"))
      {
        display->message("Type in the name of a hardware object,");
        display->message("or any of these special commands:");
        display->message("HELP     STATUS        CLEAR      CMDLOG [ON|OFF]");
        display->message("SHOW     LIST [type]   GET <file> DATALOG [ON|OFF]");
        display->message("STOP     COMMAND       PUT <file> ERRORLOG [ON|OFF]");
        display->message("HALT     CONNECTIONS   KILL <id>  DEBUG [ON|OFF]");
        display->message("MEM      EXPERT [pswd] WAIT <sec> POLLS [ON|OFF]");
        display->message("LOGOUT   ALLOW <ip>    ZPOLL      QUIT   INIT   VER");
      }
      else if (!strcasecmp(tok,"ver")) {
        dprintf("MANIP version %s", VERSION);
      }
      else if (!strcasecmp(tok,"allow")) {
        tok = strtok(NULL, delim);
        if (tok) {
          Server2::allowTemp(tok);
          sprintf(display->outString,"Temporarily allowing connections from %s",tok);
        } else {
          sprintf(display->outString,"Please specify IP to allow");
        }
        display->message();
      }
      else if (!strcasecmp(tok,"init")) {
        if (dataInput->initAVRs()) {
        	// intialize necessary AVR32 channels
            Hardware::doCheckInit();
            dataInput->doneInit();
        }
      }
      else if (!strcasecmp(tok,"status"))
      {
        sprintf(responseString, "SOURCE_%s",
                thePoly->isMoving() ? "MOVING" : "STOPPED");
        if (!thePoly->isMoving()) {
          sprintf(strchr(responseString,0), "_%s", thePoly->getStopCause());
        }
        updateFlag = 1;   // force update of all readouts
      }
      else if (!strcasecmp(tok,"show"))
      {
        tok = strtok(NULL, delim);  // get object to display
        if (!tok) {
          sprintf(display->outString,"Currently displayed PolyAxis: %s",thePoly->getObjectName());
          display->message();
          Hardware::showObjects("PolyAxis",tok,kMatchType);
        } else {
          for (ipoly=0; ipoly<LIST_END; ++ipoly) {
            if (!strcasecmp(tok, ipoly.obj()->getObjectName())) {
              setDisplayObject(ipoly.obj());
              break;
            }
          }
          if (ipoly == LIST_END) {
            // show a list of available PolyAxis objects
            Hardware::showObjects("PolyAxis",tok,kMatchType);
          }
        }
      }
      else if (!strcasecmp(tok,"polls"))
      {
        tok = strtok(NULL, delim);  // get "on" or "off"
        if (tok) {
          ClientDev *out = getCurrentClient(&ConsoleClient::console);
          if (!out->isExpert()) {
            strcpy(display->outString, "Expert command is not accessible");
          } else {
            int on = -1;
            if (!strcasecmp(tok,"on")) on = 1;
            else if (!strcasecmp(tok,"off")) on = 0;
            if (on < 0) {
              sprintf(display->outString, "Must specify on or off");
            } else {
              Hardware::polls(on);
              sprintf(display->outString, "Turned %s polling for all objects", tok);
            }
          }
          display->message();
        } else {
          Hardware::showObjects("Polls on",NULL,kPollsOn);
          Hardware::showObjects("Polls off",NULL,kPollsOff);
        }
      }
      else if (!strcasecmp(tok,"list"))
      {
        tok = strtok(NULL, delim);  // get class name to display
        if (tok) {
          Hardware::showObjects(tok,NULL,kMatchType);
        } else {
          Hardware::showObjects("Hardware",NULL,kShowAll);
        }
      }
      else if (!strcasecmp(tok,"logout"))
      {
        if (display->altOutputs()) {
          // one of out clients logged out
          ClientDev *client = getCurrentClient(NULL);
          for (i=0; i<numServers; ++i) {
            serverList[i]->deleteClient(client, 1);
          }
        } else {
          // logout was typed from console -- kill all clients
          for (i=0; i<numServers; ++i) {
            serverList[i]->deleteAllClients();
          }
        }
      }
      else if (!strcasecmp(tok,"clear"))
      {
        display->clearLines(1,kNetStatusLine-1);
        display->home_cursor();     // return cursor to home position
        updateFlag = 1;
        numAxes = -1;     // force readraw of everything
      }
      else if (!strcasecmp(tok,"wait"))
      {
        if (!cmdFile) {
          display->message("The wait command is only useful when executed from a command file.");
        } else {
          tok = strtok(NULL, delim);
          double waitTime;
          if (tok) waitTime = atof(tok);
          else waitTime = 0;
          cmdTime = RTC->time() + waitTime;
          sprintf(display->outString,"Waiting %g seconds",waitTime);
          display->message();
        }
      }
      else if (!strcasecmp(tok,"command"))
      {
        if (cmdFile) {
          sprintf(display->outString,"Command currently being executed: %s",cmdCommand);
          display->message();
        } else {
          display->message("No command file currently executing");
        }
      }
      else if (!strcasecmp(tok,"connections"))
      {
        sprintf(display->outString,"%s in %s mode",
                ConsoleClient::console.getName(),
                ConsoleClient::console.isExpert() ? "Expert" : "Normal");
        display->message();
        for (i=0; i<numServers; ++i) {
          serverList[i]->showClients();
        }
      }
      else if (!strcasecmp(tok,"expert"))
      {
        // get pointer to originating host
        ClientDev *out = getCurrentClient(&ConsoleClient::console);
        tok = strtok(NULL,delim);
        int exp;
        char *str2;
        int extended = 0;
        if (!tok) {
          exp = out->isExpert();
          str2 = "";
        } else {
          exp = strcasecmp(tok,"room601") ? 0 : 1;
          if (exp && !cmdFile) {
            // remove password from history
            keyboard.changeHistoryEntry("expert *******");
          }
          if ((!exp) == (!out->isExpert())) {
            if (exp) {
              extended = 1;
            } else {
              str2 = "already ";
            }
          } else {
            str2 = "now ";
          }
          out->setExpert(exp); // set expert mode
        }
        if (extended) {
          sprintf(display->outString,"%s Expert mode timeout extended", out->getName());
        } else {
          const char *modeStr = exp ? "Expert" : "Normal";
          sprintf(display->outString,"%s is %sin %s mode",
                  out->getName(), str2, modeStr);
        }
        display->message();
      }
      else if (!strcasecmp(tok,"kill"))
      {
        tok = strtok(NULL,delim);
        int clientID;
        if (tok && (clientID=atoi(tok)) > 0) {
          int done = 0;
          for (i=0; i<numServers; ++i) {
            if (serverList[i]->deleteClient(clientID)) {
              done = 1;
            }
          }
          if (!done) {
            display->message("Invalid client number");
          }
        } else {
          display->message("Invalid client ID");
          display->message("Type CONNECTIONS to get a list of current client ID's");
        }
      }
      else if (!strcasecmp(tok,"get"))
      {
        out = getCurrentClient(NULL);
        if (out) {
          tok = strtok(NULL,delim);
          if (tok) {
            out->openFile(tok, kFileRead);
          } else {
            display->message("You must specify a filename for the GET command.");
          }
        } else {
          display->message("The GET command is only meaningful");
          display->message("when sent from a remote host.");
        }
      }
      else if (!strcasecmp(tok,"put"))
      {
        out = getCurrentClient(NULL);
        if (out) {
          tok = strtok(NULL,delim);
          if (tok) {
            FILE *fp = fopen(tok,"r");
            if (fp) {
              fclose(fp);
              display->message("WARNING: File exists and will be overwritten!");
            }
            out->openFile(tok, kFileWrite);
            if (out->getFileStatus() != kFileNone) {
              display->message("PUT started -- end with 'ENDPUT' or abort with 'ABORTPUT'");
            }
          } else {
            display->message("You must specify a filename for the PUT command");
          }
        } else {
          display->message("The PUT command is only meaningful");
          display->message("when sent from a remote host.");
        }
      }
      else if (!strcasecmp(tok,"endput")) {
        out = getCurrentClient(NULL);
        if (!out || out->getFileStatus()!=kFileWriteDone) {
          display->message("No PUT command in progress from this client");
        } else if (!out->closeFile()) {
          display->message("PUT file closed");
        }
      }
      else if (!strcasecmp(tok,"abortput")) {
        out = getCurrentClient(NULL);
        if (!out || out->getFileStatus()!=kFileWrite) {
          display->message("No PUT command in progress from this client");
        } else if (!out->closeFile()) {
          display->message("PUT aborted");
        }
      }
      else if (startCommandFile(tok,&cmdFile))
      {
        // execute special command file
        sprintf(display->outString,"-- Command file start: %s --",tok);
        display->message();
        // save command arguments
        for (cmdArgNum=0; cmdArgNum<kMaxArgs; ++cmdArgNum) {
          tok = strtok(NULL,delim);
          if (!tok) break;
          strncpy(cmdArgs[cmdArgNum],tok,kMaxArgLen);
          cmdArgs[i][kMaxArgLen-1] = 0;
        }
        cmdOutput = getCurrentClient(NULL);
        cmdCommand[0] = 0;  // better safe than sorry
        cmdTime = 0;
      }
      else if (!strcasecmp(tok,"zpoll")) {
        float z = 2000;
        PolyAxis *lowPoly = NULL;
        for (i=-1,ipoly=0; ipoly<LIST_END; ++ipoly) {
          if (!ipoly.obj()->getNumAxes()) continue; // must have ropes attached
          ThreeVector position = ipoly.obj()->getSourcePosition();
          if (z < position[Z]) continue;
          z = position[Z];
          lowPoly = ipoly.obj();
        }
        if (lowPoly) {
          sprintf(display->outString,"%s: %s\n",tok,lowPoly->getObjectName());
          display->message();
          strcpy(commandString,"monitor");
          char *temp = Hardware::doCommand(lowPoly->getObjectName(), commandString);
          if (temp) {
            strncpy(responseString,temp,BUFFER_LENGTH-1);
          } else {
            strcpy(responseString,"<none>");
          }
        } else {
          sprintf(display->outString,"%s: <no sources deployed>\n", tok);
          display->message();
          strcpy(responseString, "HW_ERR_OK");
        }
      }
      else if (!strcasecmp(tok,"autoexec")) {
        display->message("No AUTOEXEC command file");
      }
      else
      {
        // else pass command to hardware
        char *temp = Hardware::doCommand(tok, strtok(NULL,""));
        if (temp)
        {
//          int len = strlen(temp)<BUFFER_LENGTH-1?strlen(temp):BUFFER_LENGTH-1;
          // fixed this - PH 12/04/96
          strncpy(responseString,temp,BUFFER_LENGTH-1);
          if (!strcmp(responseString,"HW_ERR_NO_ACCESS")) {
            if (cmdFlag) {
              cmdTryAgain = 1;
              if(ftime-lastTime > 0.1f) updateFlag = 1; // update readouts while waiting for object to become accessible
              strcpy(responseString,"HW_ERR_IGNORE");
            } else {
              display->message("Command is not accessible");
            }
          } else if (!strcmp(responseString,"HW_ERR_EXPERT")) {
            strcpy(display->outString,"Expert command is not accessible");
            display->error();
            writeCmdLog();
          }
        } else {
          strcpy(responseString, "<none>");
        }
      }

      // finish commands now, before polling hardware
      // to avoid sending error messages to remote hosts
      // - PH 07/07/98
      if (!updateFlag) {
        // poll the network server object to send command response
        for (i=0; i<numServers; ++i) {
          serverList[i]->doneCommand(responseString);
        }
        display->clearOutputs();
      }

    } else {

      if(ftime-lastTime > 0.1f) {
        updateFlag = 1;                    // update readouts
      }
    }

    if (updateFlag) {

      // change file names if this is a new month - PH 12/20/99
      thisTime = time(NULL);
      tms = localtime(&thisTime);
      int new_stamp = (tms->tm_year % 100) * 100 + tms->tm_mon + 1;
      if (date_stamp != new_stamp) {
        date_stamp = new_stamp;
        sprintf(buff,"CMD%.4d.LOG", date_stamp);
        cmdLogFile.flush();
        cmdLogFile.setName(buff);
        sprintf(buff,"DATA%.4d.LOG", date_stamp);
        dataLogFile.flush();
        dataLogFile.setName(buff);
        sprintf(buff,"ERR%.4d.LOG", date_stamp);
        errorLogFile.flush();
        errorLogFile.setName(buff);
      }

      // update display if number of axes changed
      int num = thePoly->getNumAxes();
      if (thePoly->getUmbilical()) ++num;
      if (num != numAxes) setDisplayObject(thePoly);

      *display += dataLogList;    // add data loggers to output
      display->echoToScreen(1);   // output must go to screen even if alt outputs

      updateFlag = 0;
      wx=wherex();
      wy=wherey();

      static char *altTension[4] = { "Axis1 Tension: ", "Axis2 Tension: ", "Axis3 Tension: ", "Umbilical Tension: "};
      static char *altLength[4] = { "Axis1 Length: ", "Axis2 Length: ", "Axis3 Length: ", "Umbilical Length: " };
      static char *altError[4] = { "Axis1 Error: ", "Axis2 Error: ", "Axis3 Error: ", "Umbilical Error: " };

      Axis *axis;
      int x, dy = 0;
      int axisNum;
      for (iaxis=0; ; ++iaxis) {
        if (iaxis < 3) {
          axis = iaxis.obj();
          axisNum = iaxis;
          x = 19 + iaxis * 25;
        } else {
          axis = thePoly->getUmbilical();
          if (!axis) break;
          axisNum = 3;
          x = 19 + 2 * 25;
          dy = 4;
        }
        sprintf(display->outString,"%7.2f  ",axis->getTension());
        display->altMessage(altTension[axisNum], 0);
        display->message(x,dy+kTensionLine);
        sprintf(display->outString,"%7.2f  ",axis->getLength());
        display->altMessage(altLength[axisNum], 0);
        display->message(x,dy+kLengthLine);
        sprintf(display->outString,"%7.2f  ",axis->getEncoderError());
        display->altMessage(altError[axisNum], 0);
        display->message(x,dy+kAxisErrorLine);
        if (iaxis >= 3) break;
      }

      sprintf(display->outString,"%s",RTC->timeStr(ftime));
      display->altMessage("Time: ", 0);
      display->message(20,kTimeLine);
// try displaying max poll time instead - PH 07/22/98
//      sprintf(display->outString,"%8.2lf",Hardware::getAvgPollTime()*1e3);
      sprintf(display->outString,"%8.2lf",Hardware::getMaxPollTime()*1e3);
      display->altMessage("Polls(ms): ", 0);
      display->message(42,kTimeLine);


      ThreeVector position = thePoly->getSourcePosition();
      sprintf(display->outString,"%9.2f %9.2f %9.2f",position[X],position[Y],position[Z]);
      display->altMessage("Source Position: ", 0);
      display->message(21,kPositionLine);
      sprintf(display->outString,"%8.2f", thePoly->getPositionError());
      display->altMessage("Length Error: ", 0);
      display->message(22,kStatsLine);
      sprintf(display->outString,"%7.2f", thePoly->getNetForce());
      display->altMessage("Net Force: ", 0);
      display->message(43,kStatsLine);

      lastTime = ftime;
      if (ftime - lastReset > 60) {
        Hardware::resetMaxPollTime();
        lastReset = ftime;
      }
      gotoxy(wx,wy);

      *display -= dataLogList;    // remove data loggers from output
      display->echoToScreen(0);
      dataLogFile.flush();        // flush data log file if required

      // finish command if output going to remote host - PH 11/06/00
      if (display->altOutputs()) {
        // poll the network server object to send command response
        for (i=0; i<numServers; ++i) {
          serverList[i]->doneCommand(responseString);
        }
      }
    }
  }

  display->clearLines(kCommandLine+1,25);
    
  if (did_mem) checkHeap();
    
  qprintf("Normal Termination.  Ciao.\n");

  myCleanup();
  return(0);
}


//--------------------------------------------------------------
// other routines
//

void writeCmdLog(char *msg, char *host)
{
  if (cmdLogList.count()) {
    if (!msg) msg = display->outString;
    if (!host) host = "log";
    sprintf(tmpBuff,"%s [%s] %s",
            RTC->timeStr(RTC->time(),kClockDate), host, msg);
    char *pt = strchr(tmpBuff,'\0');
    if (*(pt-1) != '\n') {
      strcpy(pt,"\n");
    }
    cmdLogList.outString(tmpBuff);
  }
}

void Except(int regList)  // FPE execption handler
{
  regList = regList;      // warning suppression

//PH  flushall();           // flush buffers

  sprintf(display->outString,"FPE: motors stopped\n");
  display->message();
  quit();
}

// handle math errors without barfing too badly - PH 01/29/97
//
// NOTE:  Currently after this routine is called, garbage
//        is left in the FPU registers.  This can cause
//        numerical errors down the line!  If the reason for
//        this is not found, then it would be better if
//        we just aborted on numerical errors. - PH
//
int matherr(struct exception *a)
{
/*PH  char *errStr;
  switch (a->type) {
    case DOMAIN:
      errStr = "DOMAIN";
      break;
    case SING:
      errStr = "SING";
      break;
    case OVERFLOW:
      errStr = "OVERFLOW";
      break;
    case UNDERFLOW:
      errStr = "UNDERFLOW";
      break;
    case TLOSS:
      errStr = "TLOSS";
      break;
    default:
      errStr = "UNKNOWN";
      break;
  }
  sprintf(display->outString,"%s() %s error\n",a->name,errStr);
  display->messageB(1,kErrorLine);*/
  return(1);
}

// cleanup routine called on program termination - PH 01/29/97
static void myCleanup()
{
  Hardware::doUpdate(); // do one final update of all databases

  // delete our servers (closes clients too)
  for (int i=0; i<numServers; ++i) {
    delete serverList[i];
  }
  numServers = 0;

  // delete our polaxis objects (just in case they want to clean up)
  LListIterator<PolyAxis> lp(polyList);
  for (lp=0; lp<LIST_END; ++lp) {
    lp.obj()->stop(1);    // stop it just in case
    delete lp.obj();      // delete it
  }
  // remove all items from the polyaxis list
  polyList.clear();

  display->clearLines(kNetStatusLine,kNetStatusLine); // clear net status line

  // delete other stuff
  delete RTC;
  delete dataInput;
  delete display;

  display = NULL;
}

// pre-create ALL Axis, PolyAxis and Server objects - PH 01/30/97
static void createAllObjects(LList<PolyAxis>& polyList, AV &av)
{
  FILE  *fp = NULL;
  char  name[256];
  char  subClass[128];

  // first create a Axis objects
  for (;;) {
    name[0] = '\0';   // look for any matching name
    fp = findEntry("AXIS", name, AXIS_VER, "r", fp, subClass);
    if (!fp) break;   // file is closed and null is returned when no more AXIS entries

    // parse the subclass name and create an object of the specified type.
    // (Could make this automatic, but registering the subclasses would be
    // just about as painful, and this accomplishes it all in one step) - PH
    if (!subClass[0] || !strcasecmp(subClass,"AXIS")) {

      new Axis(name, &av, fp);  // create new Axis object

    } else if (!strcasecmp(subClass,"UMBILICAL")) {

      new Umbilical(name, &av, fp); // create a new Umbilical

    } else if (subClass[0] != '/') {  // error if not a comment

      qprintf("Unrecognized Axis subclass %s in AXIS.DAT\n",subClass);
      quit("aborting");
    }
  }

  // then create all PolyAxis objects, and add them to the list
  for (;;) {
    name[0] = '\0';   // look for any matching name
    fp = findEntry("POLYAXIS", name, 1.00, "r", fp, subClass);
    if (!fp) break;   // file is closed and null is returned when no more POLYAXIS entries

    // parse the subclass name and create an object of the specified type.
    // (Could make this automatic, but registering the subclasses would be
    // just about as painful, and this accomplishes it all in one step) - PH
    if (!subClass[0] || !strcasecmp(subClass,"POLYAXIS")) {

      polyList += new PolyAxis(name, &av, fp);  // create new PolyAxis and add to our list

    } else if (!strcasecmp(subClass,"R_SOURCE")) {

      polyList += new R_Source(name, &av, fp);  // create a new R_Source

    } else if (subClass[0] != '/') {  // error if not a comment

      qprintf("Unrecognized PolyAxis subclass %s in POLYAXIS.DAT\n",subClass);
      quit("aborting");
    }
  }
  // create servers
  while (numServers < kMaxServers) {
    name[0] = '\0';   // look for any matching name
    fp = findEntry("SERVER", name, 1.00, "r", fp, subClass);
    if (!fp) break;   // file is closed and null is returned when no more AXIS entries
    Server2 *serv = new Server2(name, dispatcher, fp);
    if (serv) {
      serverList[numServers++] = serv;
    } else {
      display->error("out of memory");
      fclose(fp);
      break;
    }
  }
}

// set the currently displayed PolyAxis object - PH 01/31/97
static void setDisplayObject(PolyAxis *obj)
{
  thePoly = obj;    // set our display object
  numAxes = thePoly->getNumAxes();  // save current number of axes
  iaxis   = thePoly->getAxisList(); // initialize iterator for axis list

  if (thePoly->getUmbilical()) ++numAxes;

  display->clearLines(1, kAxisErrorLine+4); // clear area of screen

  // print standard labels
  sprintf(display->outString,"Time:");
  display->message(4,kTimeLine);
  sprintf(display->outString,"Polls(ms):");
  display->message(32,kTimeLine);
  sprintf(display->outString,"Source Position:");
  display->message(4,kPositionLine);
  sprintf(display->outString,"X         Y         Z");
  display->message(26,kPositionLine-1);
  sprintf(display->outString,"Length Error:");
  display->message(4,kStatsLine);
  sprintf(display->outString,"Net Force:");
  display->message(32,kStatsLine);

  // print polyaxis name
  sprintf(display->outString,"PolyAxis: %s           ",obj->getObjectName());
  display->message(31, kTitleLine);

  Axis *axis;
  int x, dy=0;
  for (iaxis=0; ; ++iaxis) {    // set up display format
    if (iaxis < 3) {
      axis = iaxis.obj();
      x = 4 + iaxis * 25;
    } else {
      axis = thePoly->getUmbilical();
      if (!axis) break;
      x = 4 + 2 * 25;
      dy = 4;
    }
    sprintf(display->outString,"%s",axis->getObjectName());
    display->message(x+6,dy+kAxisNameLine);
    sprintf(display->outString,"Tension(N):");
    display->message(x,dy+kTensionLine);
    sprintf(display->outString,"Length(cm):");
    display->message(x,dy+kLengthLine);
    sprintf(display->outString,"Error(cm): ");
    display->message(x,dy+kAxisErrorLine);
    if (iaxis >= 3) break;
  }
}

