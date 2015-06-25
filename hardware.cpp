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
//
// Revisions:		01/29/97 PH - changed to linked list and added registry
//							01/31/97 PH - changed doCommand to take two arguments
//							03/03/97 PH - added avgPollTime
//
#include "hardware.h"
#include "controll.h"
#include "display.h"
#include "clock.h"

#define MIN_POLL_TIME				20.e-3  	// minimum polling loop time (sec) (was 1e-4)

extern Display *display;									// defined in motorcon.cpp
int		 debugging;												// set by motorcon command line

LList<Hardware> Hardware::hardwareList;		 // empty hardware list
double		 Hardware::lastPollTime = 0;
double		 Hardware::avgPollTime = 1e-3;
double		 Hardware::maxPollTime = 0;
const char Hardware::delim[] = " \t,:\n\r";     // command line token delimiters

/************************************************************************/
/* Constructor for Hardware objects.  This is called by all derived     */
/* classes as well.  It stores the class and object names of the derived*/
/* class object and handles management of the hardware object list.     */
/* This list is used to allow static member functions doCommand         */
/* and doPoll to send commands to all Hardware objects and to poll all  */
/* Hardware objects with single line commands at the top level.         */
/************************************************************************/
Hardware::Hardware(char* cName, char* oName)
				: Object(cName, oName)
{
	pollable = 1;         // default state is pollable
	commandFlag = kAccessAlways; // default state is to accept commands
	owner = NULL;

	// make sure the object doesn't already exist
	if (findObject(oName)) {
		qprintf("Duplicate object name: %s\n",oName);
		quit("aborting");
	}

	// add item to linked list
	hardwareList += this;
}       // end of constructor

/**************************************************************************/
/* Destructor for Hardware objects deletes class and object names and   	*/
/* removes the object from the hardware linked list.                    	*/
/**************************************************************************/
Hardware::~Hardware(void)
{
	// remove item from linked list
	hardwareList -= this;
}

/************************************************************************/
/* Parse commandString to find Hardware object it is directed to and    */
/* send the command string and any arguments, stripped of the Hardware  */
/* object's name, to that object via the Controller instance of the     */
/* pure virtual Hardware member function command.                       */
/************************************************************************/
char* Hardware::doCommand(char *objectName, char* commandString)
{
    char buff[1024];
	LListIterator<Hardware> li(hardwareList);
	char* commandName;                  // name of command
	char* argString;                    // arguments passed to command
	Hardware *obj = findObject(objectName);
	if (obj) {
		if (!commandString) {
			commandName = NULL;
			argString = NULL;
		} else {
		    strcpy(buff, commandString);
			commandName = strtok(buff,delim);   // next word is name of command
			argString   = strtok(NULL,"");		// get the rest of the string
		}
		return obj->command(commandName,argString); // returns result
	}

	showObjects("Hardware", objectName, kShowAll);

	return "HW_ERR_NO_OBJECT"; // if get to here could not find object for command
}

/************************************************************************/
/* Display a list of available hardware objects - PH 01/31/97           */
/*																																			*/
/* - If objectName is NULL, doesn't print "not available" message				*/
/* - If showAll is zero, only objects not owned are displayed						*/
/************************************************************************/
void Hardware::showObjects(char *objectType, char *objectName, int flags)
{
	LListIterator<Hardware> li(hardwareList);
	int		n,len;							// temporary variable for length of name

	if (objectName) {
		sprintf(display->outString,"%s object '%s' is not available\n",
						objectType, objectName); // warn user of failure
		display->message();
	}
	if (flags & (kPollsOff | kPollsOn)) {
    	sprintf(display->outString,"%s:\n", objectType);
	} else {
    	sprintf(display->outString,"Available %s objects are:\n", objectType);
    }
	display->message();

	len = 0;
	for (li=0; li<LIST_END; ++li)
	{
		Hardware *hwobj = li.obj();
		if (flags & kMatchType) {	// don't show objects of other types
			if (strcasecmp(hwobj->getClassName(), objectType)) continue;
		}
		if (flags & kNotOwned) {	// don't show owned objects
			if (hwobj->getOwner()) continue;
		}
		if (flags & kPollsOn) {
		    if (!hwobj->pollable) continue;
		}
		if (flags & kPollsOff) {
		    if (hwobj->pollable) continue;
		}
		n = strlen(hwobj->getObjectName());
		// avoid running over each 20th column if possible
		if ((len%20) && n>=10 && n<20 && len<70) {
			strcpy(display->outString+len,"          ");
			len += 10;
		}
		if (n+len > 80) {
			display->message();
			len = 0;
		}
		strcpy(display->outString+len, hwobj->getObjectName());
		len += n;
		n = 10 - (len % 10);
		// pad to next 10-character tab stop with spaces
		do {
			display->outString[len++] = ' ';
		} while (--n);
		display->outString[len] = 0;
	}
	if (len) display->message();	// print remaining text
}

/************************************************************************/
/* Poll all Hardware objects.  If the derived class definition does not */
/* provide overloading for the virtual poll(void) member function,      */
/* the default do-nothing poll(void) function is called.                */
/************************************************************************/
void Hardware::doPoll(void)
{
	double	theTime = Clock::getClock()->time();
	double	pollTime;

    if (!lastPollTime) lastPollTime = theTime;
	if ((theTime-lastPollTime) < MIN_POLL_TIME) return; // slow down polling

	// do a 1% IIR averaging of poll time
	pollTime = theTime - lastPollTime;
	avgPollTime = avgPollTime * 0.99 + pollTime * 0.01;

	lastPollTime = theTime;	// save time of last poll
	if (pollTime > maxPollTime) maxPollTime = pollTime;

	for (LListIterator<Hardware> li(hardwareList); li<LIST_END; ++li)
	{
		if (li.obj()->pollable)
		{
			if (debugging)
			{
				sprintf(display->outString,"polling: %s   ", li.obj()->getObjectName());
				display->message(1,kErrorLine);
			}
			li.obj()->poll();
		}
	}
}

/************************************************************************/
/* Check to see if any new AVR32 channels need initializing             */
/************************************************************************/
int Hardware::doCheckInit(void)
{
    int err = 0;
	for (LListIterator<Hardware> li(hardwareList); li<LIST_END; ++li)
	{
		if (li.obj()->checkInit()) err = 1;
	}
	return err;
}

/************************************************************************/
/* Update the database of all hardware objects                          */
/************************************************************************/
void Hardware::doUpdate(void)
{
	for (LListIterator<Hardware> li(hardwareList); li<LIST_END; ++li)
	{
		if (li.obj()->needUpdate()) {
			li.obj()->updateDatabase();
		}
	}
}

/************************************************************************/
/*	Poll named Hardware object.																					*/
/************************************************************************/
void Hardware::doPoll(char* objectName)
{
	// loop over all objects
	for (LListIterator<Hardware> li(hardwareList); li<LIST_END; ++li)
	{
		if (!li.obj()->pollable) continue; // is not pollable

		if (!strcasecmp(objectName,li.obj()->getObjectName())) // found object
		{
			li.obj()->poll();               	// poll object
		}
	}     // end of for-loop over Hardware objects
}


/************************************************************************/
/*	Turn on/off polling for all hardware								*/
/************************************************************************/
void Hardware::polls(int on)
{
	// loop over all objects
	for (LListIterator<Hardware> li(hardwareList); li<LIST_END; ++li)
	{
	    if (on) {
	        li.obj()->pollOn();
	    } else {
	        li.obj()->pollOff();
	    }
	}
}


/************************************************************************/
/*	Find a named object in the Hardware list - PH 											*/
/************************************************************************/
Hardware *Hardware::findObject(char *objectName)
{
	// loop over all objects
	for (LListIterator<Hardware> li(hardwareList); li<LIST_END; ++li) {
		if (!strcasecmp(objectName,li.obj()->getObjectName()) ||
				!strcasecmp(objectName,li.obj()->getTrueName())) // found object
		{
			return(li.obj());		// return the item
		}
	}
	return(NULL);	// no matching object found
}

/************************************************************************/
/*	Sign object out from registry	[static] - PH													*/
/*																																			*/
/*  Returns NULL if the object is not found, or is already owned				*/
/************************************************************************/
Hardware * Hardware::signOut(char *objectName, Object *owner)
{
	Hardware	*obj = findObject(objectName);	// find the object

    if (!obj) {
        dprintf("Can't find object %s",objectName);
    } else if (!obj->getOwner()) {
		obj->owner = owner;		// sign out the object
		return(obj);					// return the item
	}
	return(NULL);		// item cannot be signed out: return NULL
}

/************************************************************************/
/*	Sign object out from registry	- PH																	*/
/*																																			*/
/*  Returns NULL if the object is not found, or is already owned				*/
/************************************************************************/
Hardware *Hardware::signOut(Hardware *obj)
{
	if (obj && !obj->getOwner()) {
		obj->owner = this;		// sign out the object
		return(obj);					// return the item
	}
	return(NULL);		// item cannot be signed out: return NULL
}

/************************************************************************/
/*	Sign object into the registry	- PH																	*/
/*																																			*/
/*  Returns NULL if the object is not owned by this             				*/
/************************************************************************/
Hardware *Hardware::signIn(Hardware *obj)
{
	if (obj && obj->getOwner()==this) {
		obj->owner = NULL;
		return(obj);
	}
	return(NULL);		// item cannot be signed in: return NULL
}
