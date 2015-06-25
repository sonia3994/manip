//PH#include <conio.h>
#include "hwutil.h"
#include "display.h"

void (*cleanupFn)()	= NULL;

void setCleanup(void (*fn)())
{
	cleanupFn = fn;
}

void quit(char* varName, char* className) // error message and exit
{
	qprintf("Could not allocate %s in class: %s\n",
		varName, className);

	// cleanup everything nicely if we have a cleanup function,
	// otherwise abort abnormally
	if (cleanupFn) {
		cleanupFn();
		exit(0);
	} else {
		abort();
	}
}

void quit(char* ename)
{
	if (ename) {
	    qprintf("%s\n",ename);
    }
	if (cleanupFn) {
		cleanupFn();
		exit(0);
	} else {
		abort();
	}
}
