// Display.h - header file for display object
//
// All character output for the manipulator goes through the Display object.
//
// Output to remote hosts and files is done by adding them to the display
// output list using the overloaded += operator.  If none of these "alternate
// outputs" exist in the display list, the output goes to the screen.
// All outputs made through error() methods are echoed to a separate list
// of error outputs which is specified through setErrorOutput(). - PH
//
#ifndef __DISPLAY_H
#define __DISPLAY_H

#include <stdio.h>
#include "llist.h"
#include "doslib.h"
#include "layout.h"
#include "hwutil.h"

void dprintf(char *fmt, ...);
void eprintf(char *fmt, ...);
void qprintf(char *fmt, ...);

class OutputList;

// OutputDev is an interface class that allows output of a character
// string to a device.  It has the added feature that you can maintain
// lists of output devices using an OutputList class, and the devices
// will automatically be removed from the list when they are deleted.
// Any output device can be added to multiple output lists, and it
// will be removed from all lists when deleted.  This is nice because 
// it takes care of alot of bookeeping, and prevents accessing objects 
// that have been deleted. - PH
class OutputDev
{
	friend class	OutputList;

public:
	virtual 			~OutputDev();
	virtual void	outString(char *str) = 0;
	virtual char*	getName() = 0;

private:
	LList<OutputList>	listList;	// list of lists containing this device
};

// special class of linked list for outputs
// automatically removes output from list when it's deleted
class OutputList : public LList<OutputDev> {
public:
	virtual					~OutputList();
	OutputList &		operator+=(OutputDev *obj);	// add item to end of list
	OutputList &		operator-=(OutputDev *obj);	// remove item from list
	void						addFirst(OutputDev *obj);		// add item to start of list
	void						clear();

	virtual void		outString(char *str);				// write string to all our outputs
};


class Display
{
public:

	Display(char *p,int wx,int wy,int top,int bottom); // constructor
	~Display();  // free prompt

	Display & operator+=(OutputDev *out) { outputList+=out; return *this; }
	Display & operator-=(OutputDev *out) { outputList-=out; return *this; }
	Display & operator+=(OutputList &aList);
	Display & operator-=(OutputList &aList);

	void messageOnly(char *text, int x=1, int y=-1);
	void message(char* text, int x=1, int y=-1);	// print a message at x,y
	void messageB(char* text, int x=1, int y=-1); // blank line and print
	void error(char* text, int x=1, int y=-1);	// print an error at x,y
	void errorB(char* text, int x=1, int y=-1);	// blank line and print error
	int	 altMessage(char* text, int newLine=1,OutputList *oList=0);	// send text to alternate outputs
	int  echoToScreen()							{ return mEchoToScreen;	}
	void echoToScreen(int mode)			{ mEchoToScreen = mode;	}
	void altError(char* text);	// send text to alternate outputs

	int	 altOutputs()								{ return outputList.count();	}
	void clearOutputs() 						{ outputList.clear(); }
	int	 isOutput(OutputDev *dev)		{ return outputList.find(dev) >= 0;	}

	OutputDev *getOutput(int num);

	void setErrorOutput(OutputList *list)	{ errorList = list; }

	char outString[120];			  // pre-installed message string and print
	void message(int x=1, int y=-1) {message(outString,x,y);} // function for it
	void messageB(int x=1, int y=-1) {messageB(outString,x,y);}
	void error(int x=1, int y=-1)  {error(outString,x,y);} // function for it
	void errorB(int x=1, int y=-1) {errorB(outString,x,y);}

	void home_cursor(void);  // redisplay prompt
	void clearLines(int from, int to);	// clear area of screen
	void scroll_(int from, int to);			// scroll area of screen up one line

    static Display *sDisplay;
private:
	int 		x1,x2,y;        // prompt position and input position
	int			lastY;					// last coordinates for message
	int			topY, bottomY;	// range for scrolling area of display
	int			mEchoToScreen;	// non-zero if output should be echoed to screen

	char* 	prompt;         // prompts user for input
	OutputList	outputList;	// list of alternate output devices

	OutputList	*errorList;	// list of error output devices
};

#endif



