// class to support an output file device - PH 11/21/97

#include <string.h>
#include "outfile.h"

const short	kOutputBufferSize	= 2048;

// if file is buffered, writes only occur when flush() is called, 
// or when the object is deleted
OutputDevFile::OutputDevFile(char *filename, int bufferedOutput)
{
	mFileName = NULL;
	setName(filename);

	if (bufferedOutput) {
		mBuffer = new char[kOutputBufferSize];	// defaults to non-buffered if not enough memory
	} else {
		mBuffer = 0;
	}
	mBufferBytes = 0;
}


OutputDevFile::~OutputDevFile()
{
	flush();		// make sure we don't lose anything

	delete mFileName;
	delete mBuffer;
}

void OutputDevFile::setName(char *filename)
{
	delete mFileName;
	mFileName = new char[strlen(filename)+1];
	if (mFileName) strcpy(mFileName,filename);
}

// send string to device
void OutputDevFile::outString(char *str)
{
	if (mBuffer) {
		int	len = strlen(str);
		if (mBufferBytes+len >= kOutputBufferSize) {
			flush();
			// make sure our input string isn't too long
			if (len >= kOutputBufferSize) len = kOutputBufferSize-1;
		}
		// add string to our buffer
		memcpy(mBuffer+mBufferBytes, str, len);
		mBufferBytes += len;
	} else {
		// for now, don't complain if we can't open it
		FILE *outFile = fopen(mFileName,"at");
		if (outFile) {
			fputs(str, outFile);
			fclose(outFile);			// close after each write
		}
	}
}


// flush buffered output
void OutputDevFile::flush()
{
	if (mBufferBytes) {
		mBuffer[mBufferBytes] = '\0';	// null terminate string
		FILE *outFile = fopen(mFileName,"at");
		if (outFile) {

			fputs(mBuffer, outFile);

			fclose(outFile);

		}

		mBufferBytes = 0;	// reset our buffer
	}
}