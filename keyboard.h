#ifndef __KEYBOARD_H__
#define __KEYBOARD_H__

//
// This code really sucks

#define kHistDepth 64   	// number of commands to store in a
													// wrap-around buffer

#define BUFFER_LENGTH 513

#ifdef BUFFER_LENGTH      					// from motorcon.cpp
#define kHistWidth BUFFER_LENGTH		// maximum length of a command line

#else
#define kHistWidth 128							// ought to be enough, unless commands
																// get much longer than they are currently
#endif

const short		kKeyGoodTime	= 15;	// can wipe out keyboard input after this time

// error messages returned by putBuf()
enum KeyboardErrors {
	kKeyboardBusy	= -1,
	kKeyboardOverrun = -2
};


#include "time.h"
#include "display.h"
extern Display *display;

class Dispatcher;

class Keyboard
{
	private:
		int 	index;           		// command line index
		int historyCommand;		// the point in the history buffer from which we
												// last retrieved a command.
		int historyAdd;				// the point in the histroy buffer at which we
												// will
		char** historyBuffer; 	// buffer for command History

		char	commandString[256];
		time_t lastKeyTime;						  // time the last key was hit - PH 12/05/96

	public:
		Keyboard();        // constructor initializes indices
		~Keyboard(void);

		int poll(Dispatcher *disp);  // poll keyboard for input
		void changeHistoryEntry(char *str);
};

#endif
