
/****************************************************************/
/*                                                              */
/*      Hardware is a mostly virtual base class that serves as  */
/*      an interface to the subCommand member of the Command    */
/*      class.  All Hardware devices (eg. motors, loadcelss,    */
/*      axes, etc.) inherit from Hardware (via the Controller   */
/*      template class) so that they may be passed as arguments */
/*      to subCommands.  This inheritance takes place through   */
/*      the Controller template class, which implements the     */
/*      calls to Commands.                                      */
/*                                                              */
/*                      -- written 95-07-25 Thomas J. Radcliffe */
/*                              Queen's University at Kingston  */
/*                                                              */
/****************************************************************/
#ifndef __HARDWARE_H
#define __HARDWARE_H

#include <string.h>

#include "hwutil.h"                     // declaration for quit() function
#include "llist.h"                      // linked list code
#include "object.h"
#include "command.h"

#define HW_ERR_OK          0            // doCommand() return values
#define HW_ERR_NO_OBJECT  -1
#define HW_ERR_NO_COMMAND -2

enum {    // flags for showObjects()
  kShowAll    = 0x00,   // show all objects
  kMatchType  = 0x01,   // show only specified type of object
  kNotOwned   = 0x02,   // show only objects that aren't owned
  kPollsOn    = 0x04,   // show only objects that are being polled
  kPollsOff   = 0x08    // show only objects that are not being polled
};


class Hardware : public Object
{
 private:
  static LList<Hardware>  hardwareList; // linked list of all hardware objects
  static double lastPollTime; // time of last poll of all hardware
  static double avgPollTime;  // average poll time in sec
  static double maxPollTime;  // maximum poll time in sec
  int           commandFlag;  // flags for command access (setting bits restricts access)
  int           pollable;     // can object be polled?
  Object        *owner;       // owner of the object

 protected:
  static const char     delim[];        // token parsing delimiters

  static double getLastPollTime() { return lastPollTime;  }

 public:
  Hardware(char* cName = "Test", char* oName = "test"); // constructor
  virtual ~Hardware(void);                              // destructor

  static Hardware * signOut(char *objectName, Object *owner);   // take ownership of an object
  static Hardware * findObject(char *objectName); // find object in list
  Hardware *        signOut(char *objName)  { return(signOut(objName,this)); }
  Hardware *        signOut(Hardware *obj);
  Hardware *        signIn(Hardware *obj);
  Object *          getOwner()              { return(owner);  }

  static LList<Hardware>& getHardwareList() { return hardwareList;  }

  virtual void commandOff(int flag=kAccessCode1) {
                 commandFlag |= flag; // turns off command processing
               }
  virtual void commandOn(int flag=kAccessCode1) {
                 commandFlag &= ~flag;  // turns on command processing
               }
  int          getCommandFlag() { return commandFlag; }

  virtual void pollOff(void) { pollable = 0; }  // turns off polling
  virtual void pollOn(void)  { pollable = 1; }  // turns on polling
  int isPollable()           { return(pollable); }

  static char* doCommand(char *objName, char* commandString);    // do a command on all H/w
  virtual char* command(char* commandName, char* argString)=0;   // for calling real commands

  static void doUpdate();       // update all databases
  static void doPoll(void);     // poll all Hardware
  static void doPoll(char* objectName); // single object poll function
  static void polls(int on);    // turn on/off polling for all objects
  static int  doCheckInit();
  static double getAvgPollTime()  { return avgPollTime; }
  static double getMaxPollTime()  { return maxPollTime; }
  static void resetMaxPollTime()  { maxPollTime = 0;  }

  static void showObjects(char *objectType, char *objectName=NULL, int flags=kShowAll);


  virtual void poll(void) { }       // default do-nothing poll function
  virtual int  checkInit() { return 0; }
  virtual void updateDatabase() { } // default database update routine
  virtual int  needUpdate()     { return 0; } // true if hardware should update database

};      // end of Hardware class declaration

#endif
