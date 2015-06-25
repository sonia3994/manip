//
// ConsoleClient.h - PH 09/29/00
//
#ifndef __ConsoleClient_h__
#define __ConsoleClient_h__

#include "clientde.h"

class ConsoleClient : public ClientDev
{
public:
	ConsoleClient() : ClientDev(NULL) { }
	virtual void	outString(char *str);
	virtual char*	getName() { return("Console"); }

	static ConsoleClient console;
};

#endif // __ConsoleClient_h__
