#include <string.h>
#include "command.h"
#include "hardware.h"
#include "doslib.h"

// constructor sets name of command and specifies access code
Command::Command(char* n, int access) 
{
	argFormat		= NULL;
	accessMask  = access;
	anyName = !strcasecmp(n,"<any>"); // check for special "<any>" command
	commandName = new char[strlen(n)+1];
	strcpy(commandName,n);
}

Command::Command(char* n, char *format, int access)
{
	argFormat		= format;
    accessMask  = access;
	anyName = !strcasecmp(n,"<any>"); // check for special "<any>" command
	commandName = new char[strlen(n)+1];
	strcpy(commandName,n);
}

Command::~Command(void)
{ 
	delete [] commandName;     // free name
} 


char* Command::subCommand(Hardware* hw, char* inString)  // default subCommand
{
  printw("Hardware: %s Object: %s Command: %s Command input: %s\n",
     hw->Hardware::getClassName(),hw->Hardware::getObjectName(),commandName,inString);
	return "HW_ERR_OK";
}
