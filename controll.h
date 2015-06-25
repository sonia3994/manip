/****************************************************************/
/*								                                              */
/*	Controller acts as an interface between the Hardware	      */
/*	base class and the specific types of hardware.  It					*/
/*	contains the Command array structure that needs to be				*/
/*	inherited by all Hardware objects.  Hardware objects				*/
/*	must inherit their hardware status via the Controller				*/
/*	class rather than the Hardware class directly to get				*/
/*	Command functionality.  Controller is a template class			*/
/*	so that the static Command array will be different for			*/
/*	each instantiation of the template.                         */
/*                                                              */
/*	The template takes two formal parameters: a dummy type			*/
/*	and an integer expression.  The type should be set to				*/
/*	the name of the class inheriting from Controller, and				*/
/*	the integer commandNumber should be set to the number				*/
/*	of elements in the Command array for that type.  The				*/
/*	dummy type is not used for anything in the class:  it				*/
/*	just ensures that the derived class is using a unique				*/
/*	instantiation of the template class, so that there can			*/
/*	not be any ambiguity.																				*/
/*																															*/
/*	       		-- written 95-07-28 Thomas J. Radcliffe						*/
/*														Queen's University at Kingston		*/
/*																															*/
/****************************************************************/
#ifndef __CONTROLLER_H
#define __CONTROLLER_H

#include "doslib.h"	 // needed for outport

#include "command.h"
#include "display.h"
#include "hardware.h"

const short		kMonitorBufferSize	= 1024;	// buffer for monitor command

extern Display *display;		// defined in motorcon.cpp


class Controller: public Hardware
{
public:
	Controller(char* className, char* objectName):
		Hardware(className, objectName) { }	// pass names on to Hardware

	virtual ~Controller(void) { }

	virtual void monitor(char *outStr) { strcpy(outStr, "Status: Error No Monitor\n"); }

	char* command(char* commandName, char* argString);    	  // command parser

protected:
	// command list access functions that must be overridden
	// by subclasses
	virtual Command *getCommand(int num) = 0;
	virtual int 		getNumCommands() = 0;
	
private:
	static char			monitorBuffer[kMonitorBufferSize];
};

#endif
