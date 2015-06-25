//-------------------------------------------------------
// File:				Dispatch.cpp
//
// Author:			Phil Harvey
//
// Revisions: 	08/01/97 - PH created
//
// Description:	This object handles the flow of user-interface
//							commands from they keyboard and remote host.
//
//							This object parses the command for semicolons
//							and separates it out into individual commands
//							returned by GetCommand() in the order they were
//							given.
//
// Notes:
//
//-------------------------------------------------------

#include <string.h>
#include "dispatch.h"
#include "display.h"

Dispatcher::Dispatcher(Display *dpy)
{
	nextCommand = 0;
	mDisplay = dpy;
}

Dispatcher::~Dispatcher()
{
	// flush the command buffer to release all memory
	while (getNextCommand((char*)0, 0));
}

// add command to queue
// - separates semicolon-delineated commands into individual commands
CommandItem *Dispatcher::addCommand(char *str, OutputDev *out)
{
	CommandItem	**itemPt = &nextCommand;
    CommandItem *firstItem = NULL;

	// scan for last element in linked list
	while (*itemPt) itemPt = &((*itemPt)->next);

	for (;;) {
		char	*cmdEnd = strchr(str,';');	// look for end of command
		if (!cmdEnd) cmdEnd = strchr(str,'\0');

		int	len = (int)(cmdEnd - str);

		if (len) {	// ignore null commands

			CommandItem	*newItem = new CommandItem;

			if (!newItem) {
				mDisplay->message("Out of memory while executing command.");
				break;
			}

			// allocate memory for command string
			newItem->string = new char[len+1];
			newItem->next = 0;	// initialize linked list pointer
			newItem->out = out;
			newItem->flag = 0;

			if (newItem->string) {

				memcpy(newItem->string, str, len);	// copy the command
				newItem->string[len] = 0;						// terminate it
				*itemPt = newItem;									// insert into linked list
				itemPt = &newItem->next;						// step onto new last element
                if (!firstItem) firstItem = newItem;

			} else {

				mDisplay->message("Out of memory for command string");
				delete newItem;	// clean up
			}
		}
		if (!(*cmdEnd)) break;	// stop now if we reached the string terminator

		str = cmdEnd + 1;	// continue with next character after semicolon
	}
	return firstItem;
}

// retrieves next command from queue
//
// returns: 0 if no command is available
//					1 if a good command was returned
//					-1 if the command was too long for the buffer (and flushes command)
int Dispatcher::getNextCommand(char *str, int maxLen)
{
	int	rtnVal;
	
	mDisplay->clearOutputs();

	if (nextCommand) {
		if (maxLen > strlen(nextCommand->string)) {
			strcpy(str, nextCommand->string);	// copy the command over
			if (nextCommand->out) *mDisplay += nextCommand->out;	// add output
			// remove this command from the list
			rtnVal = 1;
		} else {
    	mDisplay->message("Command too long for buffer.  Ignored.");
			rtnVal = -1;
		}
		CommandItem *item = nextCommand;
		nextCommand = item->next;
		delete item->string;
		delete item;
	} else {
		rtnVal = 0;
	}
	return(rtnVal);
}


// remove all commands from an output device - PH 10/03/00
void Dispatcher::removeCommands(OutputDev *out)
{
	CommandItem	**cmdPt = &nextCommand;

	while (*cmdPt) {
		// get a pointer to the item
		CommandItem *item = *cmdPt;
		if (item->out == out) {
			// unlink this item from the list
			*cmdPt = item->next;
			// delete the item
			delete item->string;
			delete item;
		} else {
			cmdPt = &(item->next);	// step to next item in list
		}
	}
}
