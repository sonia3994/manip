/****************************************************************/
/*								*/
/*	Some utility routines for use with the Hardware class,	*/
/*	its derivatives and associates.				*/
/*								*/
/*				-- written 95-07-31 		*/
/*				   Thomas J. Radcliffe		*/
/*				   Queen's University		*/
/*								*/
/****************************************************************/
#ifndef __HWUTIL_H
#define __HWUTIL_H

#include <stdio.h>
#include <stdlib.h>

#include "ioutilit.h"

void setCleanup(void (*fn)());
void quit(char* varName, char* className);
void quit(char* ename=NULL);

#endif
