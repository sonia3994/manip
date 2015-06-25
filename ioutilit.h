/************************************************************************/
/*																																			*/
/*	Declarations for opticalUtility routines.														*/
/*																																			*/
/*			-- Feb. - July 1993 tjr																					*/
/*																																			*/
/************************************************************************/
//
// Revisions: 01/31/97 PH - Interface totally re-written
//
#ifndef __IOUTILITY_H
#define __IOUTILITY_H

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>				/*  for isspace etc. */
#include "threevec.h"

// constants

enum ENextLineFlag {
	NL_FALSE,		// nextLine control flags
	NL_TRUE,
	NL_FAIL,
	NL_WARN,
	NL_QUIET
};

// class definitions

class InputFile {		// InputFile wrapper class - PH
public:
				InputFile(char *type,
									char* fileName,
									float version,
									int errorFlag = NL_FAIL,
									FILE* dumpFile = NULL,
									FILE* inputFile = NULL);
				~InputFile();

	float nextFloat(char *keyWord=NULL, char *dataWord=NULL);
	ThreeVector nextThreeVec(char *keyWord=NULL, char *dataWord=NULL);
	char  *nextLine(char *keyWord=NULL, char *dataWord=NULL, int toUpper=1);
	void  setErrorFlag(int errorFlag=NL_FAIL) { mErrorFlag = errorFlag;	}
	int		foundKeyword()		{ return(mFileFlag);	}
	FILE	*getFile()				{ return(mFpin);			}
	void	leaveOpen()				{ mustClose = 0;			}

private:
	char	mString[513];		// input buffer
	FILE 	*mFpin;					// input file
	char 	*mFileName;			// name of input file
	int 	mErrorFlag;			// flag to specify error handling strategy
	int 	mFileFlag;			// NL_TRUE if next line is read from file
	FILE	*mDumpFile;			// optional output dumpfile
	int		mustClose;			// true if we opened the file
	char*	mKeyPt;					// pointer to current token
};



class OutputFile {		// OutputFile wrapper class - PH
public:
				OutputFile(char *type, char *name, float version, int hardFail=1);
				~OutputFile();

	int		updateLine(char *keyStr, char *value);
	int		updateFloat(char *keyStr, float value);
	int		updateInt(char *keyStr, int value);
	void	setHardFail(int flag=1)	{ mHardFail = flag;	}

private:
	FILE	*mOutFile;			// output file pointer
	char	*mFileName;			// output file name
	int		mHardFail;			// non-zero to quit on update error
};


// utility functions

int		strUP(char *str1);			//  converts to upper case
int		strLC(char *str1);			//  converts to lower case
int 	strNLWcmp(char *str1, char *str2);		//  compares without leading white space
char*	noWS(char* string);			// removes whitespace around string
char*	skipWS(char* string);			// finds first non-whitespace char
char*   strfirst(char *string, char *substring);  // compare substring at start of string
void	stripComments(char *s);
void	newLine2Null(char *s);
FILE* opener(char* fname, char* status);        /*  file opening routine */
FILE* findEntry(char* type, char* name, float version, char* fmode = "r",
								FILE *fp=NULL, char *derivedName=NULL);


#endif
