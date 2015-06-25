/****************************************************************/
/*                                                              */
/*      Command is a base class that is used to allow the       */
/*      creation of an array of Commands with different derived */
/*      types that can be fed through the parser in the         */
/*      Controller template class and executed automatically.   */
/*                                                              */
/*      Each Command consists of a name and an (overloadable)   */
/*      subCommand member function.  Member functions of        */
/*      derived classes can overload subCommand, and thus the   */
/*      member functions become callable via the parser in the  */
/*      Controller template class.                              */
/*                                                              */
/*                      -- written 95-07-25 Thomas J. Radcliffe */
/*                              Queen's University at Kingston  */
/*                                                              */
/****************************************************************/
//
// PH - added access codes and argument format specification
//
// argument format codes:
//
// # 		- any valid floating point number (no exponents)
// ' '	- any white space
//
#ifndef __COMMAND_H
#define __COMMAND_H

#include <stdio.h>

enum AccessMask {
	kAccessAlways		= 0x00,	// can access command at any time
	kAccessCode1		= 0x01,	// default access code for normal commands
	kAccessCode2		= 0x02,	// user access code 2 (1st group of special commands)
	kAccessCode3		= 0x04,	// user access code 3 (2nd group of special commands)
	kAccessExpert		= 0x08,	// command requires a password
	kAccessDefault	= kAccessCode1 | kAccessExpert
};

class Hardware;

class Command
{
protected:
	char*   commandName;                    // name of command
	char*		argFormat;
	int			accessMask;										 	// access mask for this command
    int         anyName;            // special "any" command
public:
	Command(char* name, char *format, int access=kAccessDefault);
	Command(char* name="test", int access=kAccessDefault);
	virtual ~Command(void);                 // destructor frees name

	char*   getName(void)				{ return commandName; }     // returns command name
	int			getAccessMask(void)	{ return accessMask;	}			// return access mask
	char*		getFormat(void)			{ return argFormat;		}
	int         isAny(void)             { return anyName; }
	virtual char* subCommand(Hardware* hw, char* inString); // calls command

};      // end of Command class declaration

#endif
