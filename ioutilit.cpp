#include "ioutilit.h"
#include <string.h>
#include <ctype.h>
#include <math.h>
#include "hwutil.h"		// for quit()
#include "ieee_num.h"	// for checking NAN
#include "display.h"

// prototypes for static functions

static void vercheck(char* fileName,                   /*  version checking     */
							 char* versionString, float versionNumber);

static int keyBad(char* string, char* key);
static int nextLine(char* string, char** key, FILE* fpin, char* datFile,
	 char* keyWord, char* dataWord, int errorFlag, int fileFlag, FILE* dumpFile, int toUpper=1);
static int updateEntry(FILE *outFile, char *keyStr, char *value);

static float getConversion(char* string);	// convert to cm

// keep a dummy file open so we can close it to free up the
// necessary resources to open a database file if our first
// attempt at opening it fails -- this was added because it seems
// that opening too many network connections can make it so we
// can't open any files.  Keeping one open all the time is a safety
// measure that seems to work. - PH 09/08/98
static FILE	 *dummy=NULL;


//--------------------------------------------------------------

FILE* opener( char* fname, char* status)
{
	FILE* fptr;
	char  outString[80];

	if (!(fptr = fopen(fname, status)))
	{
		if (dummy) {
			// close dummy file and try one more time
			fclose(dummy);
			dummy = NULL;
			fptr = fopen(fname, status);
		}
		if (!fptr) {
		    qprintf("\n\nCOULD NOT OPEN FILE: %s \n",fname);
			qprintf("Hard exit of MANIP -- suggest you reboot the computer.\n");
			setCleanup(0);
			quit();
		}
	}

	return(fptr);

}


void vercheck(char* fileName, char* versionString, float versionNumber)
{
				float ver;

	strUP(versionString);   /* upper case conversion */
				if (strncmp("VERSION",versionString,7))
	{
	    qprintf("FILE: %s DOES NOT BEGIN WITH VERSION NUMBER\n",
			fileName);
		qprintf("LOOKING FOR:  VERSION%.2f\n",versionNumber);
		qprintf("WITH NO LEADING SPACES\n");
		qprintf("First line is: %s\n",versionString);
		quit();
	}

	ver = atof(&versionString[7]);

	if (fabs(ver-versionNumber) > 0.005)
	{
		printw("VERSION WARNING\n");
		printw("File version number for: %s\n",fileName);
		printw("differs from code version number\n");
		printw("File version: %3.2f\n",ver);
		printw("Code version: %3.2f\n",versionNumber);
		printw("WILL PROCEED WITH INPUT ANYWAY\n");
		printw("BUT IF UNSUPPORTED DATA ENCOUNTERED\n");
		printw("IN FILE, CODE WILL PROBABLY CRASH\n");
		return;
	}

}

/********************************************************/
/*                                                      */
/*  This function converts a string to upper case and */
/*  returns the length of the string      */
/*              */
/*      -- written 95-05-06 tjr   */
/*                                                      */
/********************************************************/
int strUP(char *str1)
{
	int i;
	
	for(i=0;i<strlen(str1);i++) if (islower(str1[i])) str1[i] = toupper(str1[i]);

	return(i);

}
int strLC(char *str1)
{
	int i;
	
	for(i=0;i<strlen(str1);i++) if (isupper(str1[i])) str1[i] = tolower(str1[i]);

	return(i);

}

/********************************************************/
/*                                                      */
/*  This function compares two strings after first  */
/*  removing all leading whitespace characters.  The  */
/*  length of str2 is used in the comparison.   */
/*              */
/*      -- written 95-05-06 tjr   */
/*              */
/********************************************************/
int strNLWcmp(char *str1, char *str2)
{
	int i,j;
	
	for (i = 0; i < strlen(str1); i++) 
		if (!isspace(str1[i])) break;

	for (j = 0; j < strlen(str2); j++)
		if (!isspace(str2[j])) break;

	return(strncmp(&str1[i],&str2[j],strlen(&str2[j])));

}

//-----------------------------------------------------------
// InputFile/OutputFile wrapper class methods - PH
//
InputFile::InputFile(
			 char* fileName,		// file/class name
			 char* entry,				// entry name (if "\0" will return name of first entry found)
			 float version,			// file version
			 int errorFlag,     // flag determines action upon error
			 FILE* dumpFile,    // if not NULL, write errors to file
			 FILE* fp)					// input file (or NULL to open new one)
{
	mustClose  = 0;
	mFileName  = fileName;
	mErrorFlag = errorFlag;
	mFileFlag	 = NL_TRUE;
	mDumpFile	 = dumpFile;
	mKeyPt		 = 0;
	if (fp) {
		mFpin		 = fp;
	} else {
		mFpin		 = findEntry(fileName, entry, version);
		if (mFpin) mustClose = 1;
	}
}

InputFile::~InputFile()
{
	if (mustClose) {
		fclose(mFpin);
		if (!dummy) {
			dummy = fopen("DUMMY.DAT","a");
		}
	}
}

// nextLine
//
// find next keyword, and return pointer to following character
// Returns dataword if not NULL and keyword not found.
// If keyWord is NULL, returns pointer to start of next token
// on the line (Note: tokens are not terminated)
//
// Here is an example:
//
// Input from file:	"COLOURS: YELLOW BLUE    // a comment\n"
//
// char *str;
//
// str = in.nextLine("COLOURS:");	// str = "YELLOW BLUE    // a comment\n"
// str = in.nextLine();						// str = "BLUE    // a comment\n"
// str = in.nextLine();						// str = "// a comment\n"
// str = in.nextLine();						// str = "// a comment\n"
// ...
//
char *InputFile::nextLine(char *keyWord, char* dataWord, int toUpper)
{
	if (keyWord) {
		// use error as next read flag
		mFileFlag = ::nextLine(mString, &mKeyPt, mFpin, mFileName, keyWord,
								dataWord, mErrorFlag, mFileFlag, mDumpFile, toUpper);
	} else {
		if (!mKeyPt) quit("Call to InputFile::nextLine() with no keyword");
		// skip over all by terminator, white space and comments
		while (*mKeyPt && !isspace(*mKeyPt) && *mKeyPt!='/') {
			++mKeyPt; // skip over last token
		}
	}

	if (mKeyPt) {
	    while (isspace(*mKeyPt)) ++mKeyPt;	// skip leading white space
    }
	return(mKeyPt);	// return pointer to start of next token
}

// gets the next floating point number
// Automatically reads ahead and calls getConversion()
// if it looks like there are units following the number
float InputFile::nextFloat(char *keyWord, char* dataWord)
{
	float		val;
	char		*keyPt;

	keyPt = nextLine(keyWord, dataWord);

	if ((mErrorFlag!=NL_QUIET && mFileFlag!=NL_TRUE) || sscanf(keyPt,"%f",&val)!=1) {
		qprintf("Number not found in InputFile::nextFloat()\n");
		qprintf("Keyword: \"%s\"  Input: \"%s\"\n", keyWord, keyPt);
		quit("aborting");
	} else {
		char *nextPt = nextLine();
		if (isalpha(*nextPt)) {	// convert units if next token is alphabetical
			val *= getConversion(nextPt);
		} else {
			mKeyPt = keyPt;		// restore key pointer to original state
		}
	}
	return(val);
}

// gets the next threevector
// Automatically reads ahead and calls getConversion()
// if it looks like there are units following the numbers
ThreeVector InputFile::nextThreeVec(char *keyWord, char* dataWord)
{
	ThreeVector	val;

	for (int i=0; i<3; ++i) {
		val[i] = nextFloat(keyWord, dataWord);
		// force subsequent nextfloat() calls to scan same line
		keyWord = NULL;
	}
	return(val);
}

// OutputFile constructor
OutputFile::OutputFile(char *type, char *name, float version, int hardFail)
{
	mHardFail = hardFail;
	mOutFile = NULL;
	mFileName = new char[strlen(type)+1];
	if (!mFileName) quit("Outputfile","file name");
	strcpy(mFileName, type);	// save file name

	// open output file and look for specified object name
	mOutFile = findEntry(type, name, version, "r+");

	if (!mOutFile) {
		printw("FILE WARNING: Could not update %s %s\n",type,name);
		if (mHardFail) quit("aborting from OutputFile::OutputFile()");
	}
}

OutputFile::~OutputFile()
{
	if (mOutFile) {
		fclose(mOutFile);	// close file
		if (!dummy) {
			dummy = fopen("DUMMY.DAT","a");
		}
	}

	delete mFileName;
}

int OutputFile::updateFloat(char *keyStr, float value)
{
	if (isNAN(value)) {
		printw("Attempt to write NAN to file %s %s\n",mFileName,keyStr);
		if (mHardFail) {
			quit("Aborting from OutputFile::updateFloat()");
		} else {
			return(-1);
		}
	}
	char	buff[128];
	sprintf(buff,"%f",value);					// convert to string
	return(updateLine(keyStr,buff));
}

int OutputFile::updateInt(char *keyStr, int value)
{
	char	buff[128];
	sprintf(buff,"%d",value);					// convert to string
	return(updateLine(keyStr,buff));
}

int OutputFile::updateLine(char *keyStr, char *value)
{
	int		result;

	result = ::updateEntry(mOutFile,keyStr,value);
	if (result && mHardFail) {
		qprintf("Error updating file %s entry %s with %s\n",
						mFileName, keyStr, value);
		quit("Aborting");
	}
	return(result);
}

// Update a line from a data file - PH
// Reads beginning at the current read location
// Returns non-zero on error
int updateEntry(FILE *outFile, char *keyStr, char *value)
{
		char*			key;
		const int	kBuffSize = 200;
		char			buff[kBuffSize];
		int				keyLen = strlen(keyStr);
		long			pos = ftell(outFile);

		for (;;) {
			if (!fgets(buff, kBuffSize, outFile)) return(1); // hit end of file
			for (key=buff; isspace(*key); ++key);	// skip white space
			if (!strncmp(keyStr, key, keyLen)) break;	// found the key
			if (!strncmp(buff, "END;", 4)) return(1);	// hit end of object data
		}

		key += keyLen;	// point to space after label

		int space = strlen(key);
		int needed = strlen(value) + 1;		// needed room for value string
		char *commentPos = strchr(key,'/');

// no: a newline is not 2 characters!
//		fseek(outFile,-space-1,SEEK_CUR);   // rewind to after label
		fseek(outFile,-space,SEEK_CUR);   // rewind to after label

		if (commentPos - key >= needed) {
			space = (int)(commentPos - key);		// only write up to existing comment
		} else {
			space -= 2;		// now we need space for '/' and '\n' as well
			if (needed > space) {
				printw("Please leave some space at the ends\n");
				printw("of the %s line in the .DAT file.\n", keyStr);
				return(1);
			}
		}

		key[space] = '/';				// put in comment marker
		memset(key, ' ', space);	// clear old line
		memcpy(key+1, value, needed-1);	// copy in new value string (leaving one space)

		fputs(key, outFile);		// update file

		fseek(outFile, pos, SEEK_SET);	// return read/write pointer to original positin
		return(0);
}


/********************************************************
 *
 *  This function reads a line from a GMC database file
 *  and tests to see of the expected keyword is the
 *  first word on the line (leading whitespace is
 *  ignored.)  The line is either a new line read from
 *  the file (if fileFlag is set) or an old line being
 *  tested against a new keyword (if fileFlag is not set)
 *
 *  If the keyword is not found, the action depends on the
 *  value of errorFlag:
 *    NL_FAIL  => exit programme
 *    NL_WARN  => print warning and continue
 *    NL_QUIET => continue quietly
 *
 *  The function returns NL_TRUE if the keyword is found
 *  and NL_FALSE if it is not found.  If it is found,
 *  key points to the first character *AFTER* the 
 *  keyword when the function returns.      
 * 
 *  EXAMPLE OF USE:
 * 
 *   nextFlag = nextLine(instring,    // input string
			 &key,            // pointer to keyword
			 fpin,            // input file pointer
			 fileName,    // filename         
			 "CONVERSION:",   // keyword
			 "<factor mode>", // format hint
			 NL_WARN,   // warn on failure
			 nextFlag,    // if last nextline failed, reuse
			 NULL);     // no log file
 *
 *        -- written 95-05-06 tjr 
 *
*********************************************************/
int nextLine(char* string,  /* input string (may be read from file) */ 
			 char** key,          /* returned pointer string after keyword*/
			 FILE* fpin,          /* input file pointer                   */
			 char* fileName,      /* file data to output with errors      */
			 char* keyWord,  	    /* keyword to search for                */
			 char* dataWord,      /* file format hint in case of error    */
			 int errorFlag,       /* flag determines action upon error    */
			 int fileFlag,        /* if not set, test string, don't read  */
			 FILE* dumpFile,      /* if not NULL, write errors to file    */
			 int toUpper)         /* convert input to uppercase */
{
	int returnValue;      /* dummy for return value */

	if (fileFlag == NL_TRUE)    /* use file input   */
	{
		fgets(string,512,fpin);         /* read next line         */
		if (toUpper) strUP(string);     /* upper case conversion  */
	}

    char *pt = skipWS(string);
	*key = strfirst(pt,keyWord);    /* look for keyword       */

	if (keyBad(pt,*key))    /* check if valid keyword found */
	{
		returnValue = NL_FALSE;   /* did not find keyword         */

		if (errorFlag == NL_FAIL) /* fail badly                   */
		{
			if (!dataWord) dataWord = "<no hint provided>";
			qprintf("Expected: %s %s in %s\n",keyWord,dataWord,fileName);
			qprintf("but got: %s\n",pt);
			if (dumpFile)     /* dumpfile exists    */
			{
				fprintf(dumpFile,"Expected: %s %s in %s\n",keyWord,dataWord,fileName);
				fprintf(dumpFile,"but got: %s\n",pt);
				fclose(dumpFile);         /* ensure error message written */
			}

			quit();                   /* hard fail      */

		} else {

//			if (!dataWord) {
//				qprintf("Keyword: %s not found in %s\n",keyWord,fileName);
//				qprintf("No default available.\n");
//				quit("aborting.");
//			}

			if (errorFlag == NL_WARN)  /* warn user of default and cont*/
			{
				*key = dataWord;      /* set default value for return */

				printw("Keyword: %s not found in %s\n",keyWord,fileName);
				printw("defaulting value to: %s\n",dataWord);
				if (dumpFile)         /* dumpfile exists    */
				{
					fprintf(dumpFile,"Keyword: %s not found in %s\n",keyWord,fileName);
					fprintf(dumpFile,"defaulting value to: %s\n",dataWord);
				}
			}
			else if (errorFlag == NL_QUIET) /* quiet failure      */
			{
				*key = dataWord;              /* return default     */
			}
		}

	}
	else
	{
		returnValue = NL_TRUE;          /* found keyword      */
		*key += strlen(keyWord);        /* return pointer to after keyword  */
	}

	return(returnValue);

}

/********************************************************/
/*                                                      */
/*  This function checks for a valid keyword in string  */
/*  key is set by strstr prior to the call to keyBad.   */
/*  key may be invalid either because it is NULL (the   */
/*  keyword does not occur in string) or because there  */
/*  are leading non-whitespace characters (the keyword  */
/*  must be the first word in the string.)  If one of   */
/*  the return statements in the FOR loop is not        */
/*  executed, key has not been set properly and a BAD   */
/*  value is returned.                                  */
/*                                                      */
/*        -- written 95-05-06 tjr                       */
/*                                                      */
/********************************************************/
int keyBad(char* string, char* key)
{
	int i;                    /* loop counter     */

	if (!key) return(1);      /* NULL key is invalid    */
	
	for (i = 0; i < strlen(string); i++)
	{
		if (&string[i] == key) return(0);     /* no problems found        */
		if (!(isspace(string[i]))) return(1); /* invalid character found  */
		
	}
	
	return(1);                /* this can't happen    */
	
}

/****************************************************************
 *
 * Searches a file for a named entry with a given type qualifier.
 * The type qualifier in the file *must* end with a colon, and a
 * colon is inserted in the name if necessary.  The name of the
 * file is automatically taken to be TYPE.DAT, with the type
 * name being converted to upper case and the trailing colon not
 * used.
 *
 *        -- written 95-08-30 tjr
 *
*****************************************************************/
FILE* findEntry(char* type, /* type word identifying filename and entry type  */
				char* name,         /* name of specific entry (if "" will return first found) */
				float version,			/* version number	(ignored if inFile is given)							*/
				char* fmode, 				/* file mode (read-only default, ignored if inFile is given) */
				FILE *inFile,				/* file pointer (if null will open) */
				char *derivedName)	/* returned name of derived class type */
{
	char    string[512];  		/* input string     */
	char*   key; 			      	/* keyword string   */
	char    fileName[128];  	/* file name      */
	char    typeName[128];  	/* name of datatype   */
	char    objectName[128];	/* name of object */

	strcpy(fileName,type);  	/* copy type to filename      */
	strcpy(typeName,type);  	/* copy type to typename      */
	strcpy(objectName,name);	/* local copy of object name  */

	strUP(fileName);  				/* convert to upper case  */
	strUP(typeName);
	strUP(objectName);

	key = strchr(fileName,':'); /* find colon in type string  */

	if (key) {
		*key = '\0';         	 	/* delete trailing colon from fileName */
	} else {
		strcat(typeName,":"); 	/* add trailing colon to typeName      */
	}

	strcat(fileName,".DAT");  /* put extension on   */

	if (!inFile) {
		inFile = opener(fileName,fmode);    /* open file readonly is default */

		fgets(string,512,inFile);					  /*  read first line   */
		strUP(string);                      /* upper case conv.   */
		vercheck(fileName,string,version);  /* check version      */
	}

	while(fgets(string,512,inFile))       /* search file for name */
	{
		strUP(string);                      /* upper case conversion*/
		key = strstr(string,typeName);      /* look for name    */
		if (keyBad(string,key)) continue;
		if (objectName[0]) {
			// we are looking for a specific object
			if (!strNLWcmp(key+strlen(typeName),objectName)) break; /* compare name */
		} else {
			stripComments(key);	// get rid of comments first
			// return the name of the first object found of the specified type
			char *pt = strtok(key+strlen(typeName)," \t\n\r");
			if (pt) {
				strcpy(name, pt);		// return object name to caller
				if (derivedName) {
					pt = strtok(NULL," \t\n\r");
					if (pt) strcpy(derivedName,pt);
					else *derivedName = 0;
				}
				break;
			}
		}
	}

	if(!key)                              /* did not find keyword */
	{
		if (objectName[0]) {		// we were looking for a specific object
			qprintf("Could not find %s type: %s\n",type,name);
			qprintf("in file %s\n",fileName);
			quit();
		} else {						// didn't care about the object, so close file and return
			fclose(inFile);		// close the file
			inFile = NULL;		// return null
		}
	}

	return(inFile); /* if function returns at all, it returns good pointer  */

}

char* noWS(char* string)  // returns string with WS eliminated
{
	int i,stringLength;
	stringLength = strlen(string)-1;  // don't test terminal NULL

	for(i = stringLength; i >= 0; i--)         // eliminate trailing WS
	{
		if (isspace(string[i])) string[i] = '\0';
		else break;
	}
	return(strtok(string,"\t\n\v\f \r")); // eliminate leading WS

}

char* skipWS(char* string)  // return pointer to first non-whitespace char
{
	int i,stringLength;
	stringLength = strlen(string);

	for (i=0; i<stringLength; ++i) {
		if (!isspace(string[i])) break;
	}
	return(string+i);

}

// see if substring occurs at start of string
// returns pointer to start of string if successful
char* strfirst(char *string, char *substring)
{
    int sublen = strlen(substring);
    for (int i=0; i<sublen; ++i) {
        if (substring[i] != string[i]) return((char *)NULL);
    }
    return(string);
}

// get conversion factor based on a unit string - PH
float getConversion(char* string)  // get conversion constants for cm/cm3/kg
{
	static struct {
		char *str;
		float scale;
	} units[] = {
			{ "cm",	1 },			// distances - convert to cm
			{ "mm",	0.1 },
			{ "in", 2.54 },
			{ "ft", 30.48 },
			{ "feet", 30.48 },
			{ "m", 100 },
			{ "kg", 1 },  		// weights - convert to kg
			{ "g", 1e-3 },
			{ "N", 1/9.81 },
			{ "lb", 2.2 },
			{ "lbs", 2.2 },
			{ "pound", 2.2 },
			{ "cm^3", 1 },		// volumes - convert to cm3
			{ "cc", 1 },
			{ "cm**3", 1 },
			{ "L", 1e3 },
			{ "m^3", 1e6 },
			{ "m**3", 1e6 },
			{ "psi", 1},      // pressure - covert to psi
			{ "pa", 1.45e-4},
			{ "kpa", 0.145},
			{ "atm", 14.7 },
			{ "hz", 1 },      // rates - in 1/sec
			{ "cps", 1},
			{ "khz", 1e3 },
			{ "mhz", 1e6 },
			{ NULL, 0 },			// marks end of list
	};

	if (isalpha(*string)) { // if not alphabetical - could be another number

		for (int i=0; units[i].str; ++i) {
			int len = strlen(units[i].str);
			// look for matching string
			if (!strncasecmp(string, units[i].str, len) &&
					// unit string must end at same time
					(string[len]=='\0' || isspace(string[len]))) {
				return(units[i].scale);	// return scaling factor
			}
		}

		printw("Did not recognize units: %s\n",string);
	}
	return(1.0);	// no conversion if we don't recognize units
}


////////////////////////////////////////////////////
//
void stripComments(char* s)
//
// removes C++ style comment and replaces first /
// with a null character.
//
// CJJ, April '96
//
////////////////////////////////////////////////////
{
	int i=0;
	for(i=0;;i++)	{
		if(s[i] == '\0') break;
		if( (s[i]== '/')&&(s[i+1]=='/') )	{
			s[i] = '\0';
			break;
		}
	}
	return;
}

////////////////////////////////////////////////////
//
void newLine2Null(char* s)
//
// Searches string and replaces newline character
// with null character
//
// CJJ, May '96
//
////////////////////////////////////////////////////
{
	int i=0;
	for(i=0;;i++) {
		if(s[i] == '\0') break;
		if(s[i] == '\n') {
			s[i] = '\0';
			break;
		}
	}
	return;
}


