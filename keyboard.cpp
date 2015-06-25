#include "keyboard.h"
#include "dispatch.h"

const char *kAutoKey	= "MONITOR AUTO";	// don't log net commands containing this

Keyboard::Keyboard()        // constructor initializes indices
{
	index = 0;
	lastKeyTime = 0;

	historyCommand = 0;
	historyAdd = 0;
	historyBuffer = new char*[kHistDepth];
	for (int i=0; i<kHistDepth; i++) {
		historyBuffer[i]=new char[kHistWidth];
		strcpy(historyBuffer[i],"");
	}
	strcpy(historyBuffer[kHistDepth-5],"KDR was here");
	// Every good program needs an Easter Egg
}


Keyboard::~Keyboard()
{
	for (int i=0; i< kHistDepth;i++) delete [] historyBuffer[i];
	delete [] historyBuffer;
}


int Keyboard::poll(Dispatcher *disp)  // poll keyboard for input
{
    CommandItem *cmd;
	if (kbhit())
	{
		lastKeyTime = time((time_t *)NULL);
		char ch=getche();// get character input
		switch(ch)
		{
			case 0:				 // arrow key has been hit (old DOS)
				ch=getch_();	// get 2nd character in arrow sequence
				switch(ch)
				{
					case 72:		// up arrow
						historyCommand--;
						if (historyCommand < 0) historyCommand+=kHistDepth;
						break;
					case 80:		// down arrow
						historyCommand++;
						if(historyCommand >= kHistDepth) historyCommand-=kHistDepth;
						break;
				}
				strcpy(commandString,historyBuffer[historyCommand]);
				index=strlen(commandString);
				display->home_cursor();
				printw(commandString);
				break;
			case 0x1b: {		// ESC key
				if (kbhit()) ch=getch_();	// get 2nd character in escape sequence
				if (ch == 0x5b) {
				    ch = getch_();
                    switch(ch)
                    {
                        case 0x41:		// up arrow
                            historyCommand--;
                            if (historyCommand < 0) historyCommand+=kHistDepth;
                            break;
                        case 0x42:		// down arrow
                            historyCommand++;
                            if(historyCommand >= kHistDepth) historyCommand-=kHistDepth;
                            break;
                    }
                    strcpy(commandString,historyBuffer[historyCommand]);
                    index=strlen(commandString);
                    display->home_cursor();
                    printw(commandString);
                    break;
                }
                int x = wherex();
                int y = wherey();
				for (;;) {
					if (index <= 0) break;
					--index;
					--x;
                    gotoxy(x,y);
                    putchar_(' ');
				}
                gotoxy(x,y);
				disp->addCommand("STOP");	// stop everything on ESC key
				break;
			}
			case '\b':        // delete key has been hit
			case 0x7f: {
				if (index)
				{
					index--;
                    int x = wherex();
                    int y = wherey();
                    gotoxy(x-1,y);
                    putchar_(' ');
                    gotoxy(x-1,y);
				}
				break;
		    }
		    case '\n':
			case '\r':		 // end of line encounterd
				commandString[index] = '\0';// NULL-terminate the string
				if (index > 0 && (!historyAdd || strcmp(historyBuffer[historyAdd-1],commandString)))
					strcpy(historyBuffer[historyAdd++],commandString);
				if (historyAdd == kHistDepth)
					historyAdd = 0;
				historyCommand = historyAdd;
				index = 0;									// prepare for next command
				// give command to dispatcher
				cmd = disp->addCommand(commandString);
				if (cmd) cmd->flag = 1;
				display->home_cursor();     // return cursor to home position
				return 1;										// report good command ready
			case EOF:
				break;			 // do nothing if no character ready
			default:       // collect command characters into line buffer
				commandString[index++]=ch;	// record character for later use
		} // end of switch(ch) statement
	}		// end of kbhit() statement

	return 0; // if get to here no end of line yet

}

void Keyboard::changeHistoryEntry(char *str)
{
	int index = historyAdd - 1;
	if (index < 0) index = kHistDepth - 1;
	strcpy(historyBuffer[index], str);
}
