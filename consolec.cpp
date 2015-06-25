#include "consolec.h"
#include "display.h"

extern Display *display;

ConsoleClient ConsoleClient::console;

void ConsoleClient::outString(char *str)
{
	display->messageOnly(str);
}