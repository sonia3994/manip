//-------------------------------------------------------
// File:        Dispatch.h
//
// Author:      Phil Harvey
//
// Revisions:   08/01/97 - PH created
//
// Description: This object handles the flow of user-interface
//              commands from they keyboard and remote host.
//
//              This object parses the command for semicolons
//              and separates it out into individual commands
//              returned by GetCommand() in the order they were
//              given.
//
// Notes:
//
//-------------------------------------------------------
#ifndef __Dispatch_h__
#define __Dispatch_h__

class OutputDev;
class Display;

struct CommandItem {    // item for linked-list of commands
  char        *string;
  CommandItem *next;
  OutputDev   *out;
  int         flag;     // flag used to clear display before command
};


class Dispatcher {
public:
          Dispatcher(Display *dpy);
          ~Dispatcher();

  CommandItem *addCommand(char *str, OutputDev *out=NULL);
  void    removeCommands(OutputDev *out);
  int     getNextCommand(char *str, int maxLen);
  int     isCommandAvailable()  { return nextCommand!=0;  }
  int     getNextCommandFlag()  { return nextCommand ? nextCommand->flag : 0; }

private:
  CommandItem *nextCommand;
  Display     *mDisplay;
};

#endif // __Dispatch_h__
