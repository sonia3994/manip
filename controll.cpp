// Modifications:
//
// 01/22/97 PH  - Moved code to .CPP file
//              - Handle access code for commands

#include "clientde.h" // horrible I know - PH 08/14/00
#include "controll.h"
#include "clock.h"

char Controller::monitorBuffer[kMonitorBufferSize];


// either or both of the input string pointer may be NULL - PH 01/31/97
char* Controller::command(char* commandName, char* argString)       // command parser
{
  int   i;          // loop counter
  int   istart;       // locations for error messages
  int   jstart;

  // intercept monitor commands (always accessible)
  if (commandName) {
    if (strlen(commandName) > 500 || (argString && strlen(argString) > 500)) {
      return "HW_ERR_CMD_TOO_LONG";
    }
    if (!strcasecmp(commandName,"monitor"))
    {
      char *pt = monitorBuffer;
      if (argString) {
        argString = strtok(argString,Hardware::delim);
      }
      if (argString) {
        pt += sprintf(pt, "Monitor: %s %s\n", getObjectName(), argString);
      } else {
        pt += sprintf(pt, "Monitor: %s\n", getObjectName());
      }
      pt += sprintf(pt, "Time: %s\n",
            Clock::getClock()->timeStr(getLastPollTime(),kClockDate|kClockHundredths|kClockDST));
      pt += sprintf(pt, "Polls: %s\n", isPollable() ? "ON" : "OFF");
      monitor(pt);
      display->message(monitorBuffer);  // display monitor result
      // write to file if the argument contains a "."
      if (argString && strchr(argString,'.'))
      {
        FILE *fp = fopen(argString,"at");
        int err = 0;
        if (fp) {
          fputs("\n",fp);
          if (fputs(monitorBuffer, fp) == EOF) err = 1;
          fclose(fp);
        } else {
          err = 1;
        }
        if (err) {
          display->message("Error writing to monitor file");
        } else {
          // make filename upper case
          for (char *pt=argString; *pt; ++pt) *pt = toupper(*pt);
          sprintf(display->outString,"[Written to file %s]",argString);
          display->message();
        }
      }
      return("HW_ERR_OK");
    }
    else if (!strcasecmp(commandName,"wait"))
    {
      if (!getCommandFlag()) {
        return("HW_ERR_OK");
      } else {
        return("HW_ERR_NO_ACCESS");
      }
    }
    else if (!strcasecmp(commandName,"polls"))
    {
      if (isPollable() && getCommandFlag()) {
        return("HW_ERR_NO_ACCESS");
      }
      if (argString) {
        argString = strtok(argString,Hardware::delim);
        // can always turn polls on
        if (!strcasecmp(argString,"ON")) {
          pollOn();
          commandOn();
          sprintf(display->outString,"Polls and commands turned ON for %s",getObjectName());
        } else if (!strcasecmp(argString,"OFF")) {
          pollOff();
          commandOff();
          sprintf(display->outString,"Polls and commands turned OFF for %s",getObjectName());
        } else {
          sprintf(display->outString,"Invalid argument: Specify ON or OFF only");
        }
        display->message();
      } else {
        dprintf("Polls are %s",isPollable() ? "ON" : "OFF");
      }
      return("HW_ERR_OK");
    }

    for(i=0; i<getNumCommands(); ++i)    // loop over commands
    {
        char buff[1024];
        // compare command name
        if (!strcasecmp(commandName,getCommand(i)->getName()) || getCommand(i)->isAny())
        {
          // handle special "<any>" command -- pass full command to controller
          if (getCommand(i)->isAny()) {
            strcpy(buff, commandName);
            if (argString) {
              strcat(buff," ");
              strcat(buff, argString);
            }
            argString = buff;
          }
          if (!(getCommandFlag() & getCommand(i)->getAccessMask())) {
            // horrible, terrible hack follows
            extern ClientDev *getCurrentClient();
            ClientDev *client = getCurrentClient();
            if (!(getCommand(i)->getAccessMask() & kAccessExpert) || client->isExpert()) {
              if (!argString) argString = ""; // prevent accessing null pointer
              char *fmt = getCommand(i)->getFormat();
              if (fmt) {
                char *pt = argString;
                for (int done=0; !done; ++fmt) {
                  while (isspace(*fmt)) ++fmt;
                  while (isspace(*pt)) ++pt;
                  switch (*fmt) {
                    case '#':
                      if (!*pt) break;
                      while ((*pt>='0' && *pt<='9') || *pt=='.' || *pt=='-' || *pt=='+') ++pt;
                      if (isspace(*pt) || !*pt) continue;
                      break;  // error
                    case 0:
                      if (!(*pt)) {
                        done = 1;
                        continue;
                      }
                      break;  // error
                    default:
                      qprintf("unknown command format code %c for %s %s\n",
                              *fmt, getObjectName(), getCommand(i)->getName());
                      quit("aborting");
                      break;
                  }
                  sprintf(display->outString,"Command syntax error.  Expected arguments: %s",
                          getCommand(i)->getFormat());
                  display->message();
                  return("HW_COMMAND_SYNTAX_ERROR");
                }
              }
              return(getCommand(i)->subCommand(this,argString)); // run command
            } else {
              return("HW_ERR_EXPERT");
            }
          } else {
//            display->message("Command is not accessible");
            return("HW_ERR_NO_ACCESS");
          }
        }
    }
  } else {
    commandName = "<null>";   // change command name for printing below
  }

  display->clearLines(kFirstMessageLine, kLastMessageLine);
  sprintf(display->outString,"Hardware object: %s of type: %s could not run command: %s\n",
          getObjectName(), getClassName(), commandName);  // warn user command was not run
  display->message();
  sprintf(display->outString,"Accessible commands are:");
  display->message();
  jstart = 1; istart = kFirstMessageLine+3;
  for(i=-3; i<getNumCommands(); i++)
  {
    if (i == -3) {
      sprintf(display->outString,"monitor");
    } else if (i == -2) {
      sprintf(display->outString,"wait");
    } else if (i == -1) {
      if (!isPollable() || !getCommandFlag()) {
        sprintf(display->outString,"polls");
      } else {
        continue;
      }
    } else if (!(getCommandFlag() & getCommand(i)->getAccessMask())) {
      sprintf(display->outString,"%s", getCommand(i)->getName());
    } else {
      continue;
    }
    display->message(jstart,istart);
    if (++istart > kLastMessageLine-1) {
      istart = kFirstMessageLine+3;
      jstart += 15;
    }
  }

  return "HW_ERR_NO_COMMAND";     // return no command run error

} // end of Controller::command
