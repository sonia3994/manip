#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdarg.h>
#include <curses.h>

#include "display.h"
#include "global.h"
#include "doslib.h"

Display *Display::sDisplay = NULL;

// display printf (do NOT end lines with a "\n")
void dprintf(char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    if (Display::sDisplay) {
        char buff[1024];
        vsprintf(Display::sDisplay->outString, fmt, ap);
        Display::sDisplay->message();
    } else {
        vprintf(fmt, ap);
    }
    va_end(ap);
}

// error printf (do NOT end lines with a "\n")
void eprintf(char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    if (Display::sDisplay) {
        char buff[1024];
        vsprintf(Display::sDisplay->outString, fmt, ap);
        Display::sDisplay->error();
    } else {
        vprintf(fmt, ap);
    }
    va_end(ap);
}

// printf before quitting (do NOT end lines with a "\n")
void qprintf(char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    dos_end();
    vprintf(fmt, ap);
    va_end(ap);
}

Display::Display(char *p,int wx,int wy,int top,int bottom) // constructor
{
//PH	window(1,1,80,25);  // initialize text window
	clrscr();           // clear screen

	x1 = wx;            // x1 is postion of start of prompt
	y  = wy;            // y is line of prompt
	gotoxy(wx,wy);      // goto start of prompt
	printw("%s",p);     // write prompt string
	x2 = wherex();      // x2 is end of prompt -- where to look for input

	lastY = top;
	topY = top;
	bottomY = bottom;

	errorList = NULL;
	mEchoToScreen = 0;

	prompt = new char[strlen(p)+1]; // need to free this in destructor
	if (!prompt) quit("prompt","Display");
	strcpy(prompt,p);
	gotoxy(0,bottom+2);
	sDisplay = this;
}

Display::~Display()  // free prompt
{
	delete [] prompt;
	if (sDisplay == this) sDisplay = NULL;
}

// if x or y is negative, then it defaults to the
// line below the previous message - PH 06/24/97
void Display::message(char* text, int x, int y)
{
	// send to alternate outputs instead of screen if they exist
	if (!altMessage(text) || echoToScreen()) {
		messageOnly(text,x,y);
	}
}

// send message to this device only (not alternate devices)
void Display::messageOnly(char *text, int x, int y)
{
	int   wx,wy;
	wx = wherex();
	wy = wherey();
	for (;;) {
		char *pt = strchr(text,'\n');	// look for a newline
		if (pt) *pt = 0;	// temporarily null-terminate it
		if (y<0) {
			if (lastY == bottomY) scroll_(topY,bottomY);
			y = lastY+1;
		}
		gotoxy(x,y);
//PH      printw("%s",text);
        printw("%s",text);
		if (y>=topY && y<=bottomY) {
			lastY = y;
			y = -1;
		} else {
			++y;
		}
		if (!pt) break;
		*pt = '\n';		// restore original newline
		text = pt+1;	// step to next segment in string
		if (!*text) break;
	}
	gotoxy(wx,wy);
}

void Display::messageB(char* text, int x, int y)
{
	// send to alternate outputs instead of screen if they exist
	if (!altMessage(text) || echoToScreen()) {
		int   wx,wy;
		wx = wherex();
		wy = wherey();
		if (y<0) {
			if (lastY == bottomY) scroll_(topY,bottomY);
			y = lastY+1;
		}
		if (y>=topY && y<=bottomY) {
			lastY = y;
		}
		gotoxy(x,y);
		clreol();						// clear to end of line
		printw("%s",text);	// print message
		gotoxy(wx,wy);
	}
}


void Display::altError(char *text)
{
	if (errorList && errorList->count()) {
		char	buff[32];
		sprintf(buff,"%s ",RTC->timeStr(RTC->time(),kClockDate));
		altMessage(buff,0,errorList);
		altMessage(text,1,errorList);
	}
}


// print an error at x,y
void Display::error(char* text, int x, int y)
{
    altError(text);
	message(text,x,y);
}



// blank line and print error
void Display::errorB(char* text, int x, int y)
{
	altError(text);
	messageB(text,x,y);
}

// altMessage - send message to alternate output devices
int Display::altMessage(char *text, int newLine,OutputList *oList)
{
	if (!oList) oList = &outputList;
	
	int		num = oList->count();

	if (num) {
		int	done = 0;
		char ch;
		for (;;) {

			char *last = strchr(text,'\n');		// find end of line

			if (last) {
				++last;
				ch = *last;
				*last = 0;		// temporarily truncate string at after CR
			} else {
				done = 1;
				last = strchr(text, 0);
			}
			// skip leading white space
			while (isspace(*text)) ++text;

			// don't do anything if there is no text to send
			if (text < last) {
				// send message to all alternate output devices
				for (LListIterator<OutputDev> out(*oList); out<LIST_END; ++out) {
					// only output to each device once
					if (out != oList->find(out.obj())) continue;
					// send text to this output
					out.obj()->outString(text);
					// add trailing LF if necessary
					if (newLine) {
						if (*(last-1) != '\n') {
							out.obj()->outString("\n");
						}
					}
				}
			}
			if (done) break;

			*last = ch;				// restore original character
			text = last;			// continue with next line
		}
	}
	return(num);
}


Display & Display::operator+=(OutputList &aList)
{
	// add all items in list to our outputs
	for (LListIterator<OutputDev> out(aList); out<LIST_END; ++out) {
		*this += out.obj();
	}
	return(*this);
}


Display & Display::operator-=(OutputList &aList)
{
	// remove all items in list from our outputs
	for (LListIterator<OutputDev> out(aList); out<LIST_END; ++out) {
		*this -= out.obj();
	}
  return(*this);
}


OutputDev *Display::getOutput(int num)
{
	LListIterator<OutputDev>	out(outputList);
	
	return(out.obj(num));
}


void Display::home_cursor(void)  // redisplay prompt
{
	gotoxy(x1,y);           // goto prompt position
	printw("%s",prompt);  	// print prompt
	clreol();               // clear screen
	gotoxy(x2,y);           // goto end of prompt
}

void Display::clearLines(int from, int to)	// clear area of screen - PH
{
	int	x = wherex();	// save current position
	int y = wherey();

	for (int i=from; i<=to; ++i) {
		gotoxy(1, i);
		clreol();
	}
	if (lastY>=from && lastY<=to) {
		lastY = from;
		if (lastY < topY) lastY = topY;
	}
	gotoxy(x,y);	// restore cursor position
}

// scroll_ an area of the screen up one line
void Display::scroll_(int from,int to)
{
//PH	movetext(1,from+1,80,to,1,from);
//	gotoxy(1,to);
//	clreol();
    doscroll(from,to);
	if (lastY>=from && lastY<=to) --lastY;
}



//-------------------------------------------------------------------
// OutputDev and OutputList routines
//

OutputDev::~OutputDev()
{
	// remove this device from all output lists

	LListIterator<OutputList>	ilist(listList);
	for (ilist=0; ilist<LIST_END; ++ilist) {

		// remove ourselves from this output list
		// Note: This will call OutputList::operator-=() which will

		// delete theList from our list of lists.
		*ilist.obj() -= this;
	}

}

OutputList::~OutputList()
{
	// remove this list from all output device list lists
	LListIterator<OutputDev> idev(*this);
	for (idev=0; idev<LIST_END; ++idev) {
		idev.obj()->listList -= this;
	}
}

OutputList& OutputList::operator+=(OutputDev *obj)
{
	LList<OutputDev>::operator+=(obj);
	obj->listList += this;
	return(*this);
}

// remove an item from the list
OutputList& OutputList::operator-=(OutputDev *obj)
{
	LList<OutputDev>::operator-=(obj);
	obj->listList -= this;
	return(*this);
}



// add item to start of list
void OutputList::addFirst(OutputDev *obj)
{
	LList<OutputDev>::addFirst(obj);
	obj->listList += this;
}



void OutputList::clear()
{
	// remove this list from all output device list lists
	LListIterator<OutputDev> idev(*this);
	for (idev=0; idev<LIST_END; ++idev) {
		idev.obj()->listList -= this;
	}
	LList<OutputDev>::clear();	// let the base class clear our list
}


// write a string to all outputs in our list
void OutputList::outString(char *str)
{
	LListIterator<OutputDev> idev(*this);
	for (idev=0; idev<LIST_END; ++idev) {
		idev.obj()->outString(str);
	}
}

