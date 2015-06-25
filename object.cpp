//
// Base object class - PH 01/31/97
//
// This class was separated from Hardware to give more
// flexibility to the hierarchy.  Specifically, objects can
// now claim ownership of a Hardware object without having
// to be derived from the Hardware class.
//
#include <string.h>
#include "hwutil.h"		// for the quit() prototype
#include "object.h"

Object::Object(char *cName, char *oName)
{
	className = new char[strlen(cName)+2]; // record name of derived class
	if (!className) quit("className","Hardware");
	strcpy(className,cName);

	objectName = new char[strlen(oName)+2];// record name of specific object
	if (!objectName) quit("objectName","Hardware");
	strcpy(objectName,oName);
	strUP(objectName);  // make all names uppercase
	alias = (char *)0;
}


Object::~Object()
{
	delete className;          // delete names
	delete objectName;
	delete alias;
}

void Object::setAlias(char *str)
{
		delete alias;
		if (str && str[0]) {
			alias = new char[strlen(str) + 1];
			if (alias) strcpy(alias,str);
		} else {
			alias = (char *)0;
		}
}