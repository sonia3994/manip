#include "clientde.h"
#include "psocket.h"
#include "dispatch.h"
#include <unistd.h>

const int kInputBufferSize	= 256;
const int kOutputBufferSize = 2048;
const int	kExpertTimeLimit	= 1800;
const char *kTempPutFile		= "put_.tmp";

extern Display *display;

//======================================================
// ClientDev object
//
ClientDev::ClientDev(PSocket *sock)
{
	mSocket = sock;
	mInputBufferPos = 0;
	mOutputBufferPos = 0;
	mExpert = 0;
	mExpertTime = 0;
	mID = 0;
	mCrFlags = 0x03;
	mFileName = 0;
	mFileStatus = kFileNone;
	mInputBuffer = new char[kInputBufferSize];
	mOutputBuffer = new char[kOutputBufferSize];
	if (!mInputBuffer || !mOutputBuffer) {
		quit("out of memory");
	}
	time(&mLastRcvTime);	// save time when we last received something
}
ClientDev::~ClientDev()
{
	closeFile();
	delete mSocket;
	delete mInputBuffer;
	delete mOutputBuffer;
}
void ClientDev::outString(char *str)
{
	int len = strlen(str);
	if (len < kOutputBufferSize) {
		if (len + mOutputBufferPos >= kOutputBufferSize) {
			flushBuffer();
		}
		strcpy(mOutputBuffer+mOutputBufferPos, str);
		mOutputBufferPos += len;
	}
}

// openFile - open file for data transfer
void ClientDev::openFile(char *name, EFileStatus status)
{
	closeFile();		// make sure the current file is closed

	int len = strlen(name);
	mFileName = new char[len+1];
	if (mFileName) {
		strcpy(mFileName, name);
		if (status == kFileRead) {
			mFile = fopen(name, "r");
		} else {
			mFile = fopen(kTempPutFile, "w");
		}
		if (mFile) {
			mFileStatus = status;
			while (mFileStatus == kFileRead) {
				// read entire file and send it to the host
				int roomInBuffer = kOutputBufferSize-mOutputBufferPos-1;
				if (roomInBuffer) {
					int num = fread(mOutputBuffer+mOutputBufferPos,1,roomInBuffer,mFile);
					if (num) {
						mOutputBufferPos += num;	// update buffer position
						// output buffer string must be null-terminated
						mOutputBuffer[mOutputBufferPos] = '\0';
					} else if (!mOutputBufferPos) {
						// close file if we are all done reading and writing
						closeFile();
					}
				}
				writeBuffer();		// write buffer to network client
			}
		} else {
			display->error("Error opening file");
		}
	} else {
		display->error("Out of memory for file name");
	}
}

// closeFile - close data transfer file
int ClientDev::closeFile()
{
	int rtnVal = 0;
	if (mFileStatus != kFileNone) {
		fclose(mFile);
		if (mFileStatus == kFileWriteDone) {
			unlink(mFileName);	// erase original file if it exists
			if (rename(kTempPutFile, mFileName)) { // rename output file
				display->message("Error renaming temporary file");
				rtnVal = -1;
			}
		}
		mFileStatus = kFileNone;
		delete mFileName;
		mFileName = NULL;
	}
	return(rtnVal);
}

// getString - return CR/LF-delimited input as a null-terminated string
// returns: -1=error, 0=client disconnected, >0=string returned OK
int ClientDev::addCommands(Dispatcher *disp)
{
	char *pt = mInputBuffer + mInputBufferPos;
	int num = mSocket->Read(pt, kInputBufferSize-mInputBufferPos);

	if (num > 0) {
		time(&mLastRcvTime);	// save time when we last received something
		mInputBufferPos += num;
		// scan characters received for CR or LF
		for (;;) {
			if (*pt=='\n' || *pt=='\r') {
				int crFlags;
				if (*pt == '\n') {
					crFlags = 0x01;
				} else {
					crFlags = 0x02;
				}
				// send command if not empty
				if (pt != mInputBuffer) {
					*pt = '\0';		// null terminate the string
					// check for end of "put" command
					if (mFileStatus == kFileWrite) {
						if (!strcasecmp(mInputBuffer,"endput")) {
							// end of the put command
							mFileStatus = kFileWriteDone;
							disp->addCommand(mInputBuffer, this);
						} else if (!strcasecmp(mInputBuffer,"abortput")) {
							// pass along the abort command to abort the put
							disp->addCommand(mInputBuffer, this);
						} else {
							// write this string to output file
							fprintf(mFile,"%s\n",mInputBuffer);
						}
					} else {
						// add the command
						disp->addCommand(mInputBuffer, this);
					}
				} else if (mFileStatus==kFileWrite && (crFlags & mCrFlags)) {
					// handle writing of blank lines to output file
					fprintf(mFile,"\n");
					mCrFlags = 0;		// reset flags
				}
				mCrFlags |= crFlags;	// update CR flags
				--num; // decrement our counter
				mInputBufferPos = num; // update buffer position
				if (num <= 0) break;	// all done if no more characters received
				// copy remaining characters to the bottom of the buffer
				memmove(mInputBuffer, pt+1, num);
				// move pointer down to start of buffer
				pt = mInputBuffer;
			} else {
				mCrFlags = 0;
				if (--num <= 0) break;	// decrement character counter
				++pt;		// step to next character
			}
		}
		// check for a buffer full of crap
		if (mInputBufferPos >= kInputBufferSize) {
			// clear it
			mInputBufferPos = 0;
			sprintf(display->outString,"Network client %d input buffer overflow",mSocket->GetID());
			display->messageB(1,kNetStatusLine);
		}
		num = 1;	// return > 0 to indicate something was received
	}
	return(num);
}
void ClientDev::flushBuffer()
{
	if (mSocket && mOutputBufferPos) {
		mSocket->Write(mOutputBuffer);
		mOutputBufferPos = 0;
	}
}
int ClientDev::isExpert(int checkExpire)
{
	if (checkExpire && mExpert==2 && time(NULL)>mExpertTime) {
		sprintf(display->outString,"Expert mode time limit expired for %s client", getName());
		display->error();
		mExpert = 0;
	}
	return(mExpert);
}
// Set Expert Mode flag
// 0 - not expert (some commands inaccessible)
// 1 - expert with a new time limit
// 2 - expert with existing time limit
// 3 - expert with no time limit
void ClientDev::setExpert(int isExpert)
{
	if (isExpert == 1) {
		// set new time limit
		mExpertTime = time(NULL) + kExpertTimeLimit;
		isExpert = 2;	// now use the existing time
	}
	mExpert = isExpert;
}
void ClientDev::writeBuffer()
{
	if (mSocket && mOutputBufferPos) {
		int n = mSocket->Write(mOutputBuffer);
		if (n > 0) {
			mOutputBufferPos -= n;
			memmove(mOutputBuffer, mOutputBuffer+n, mOutputBufferPos+1);
		}
	}
}
void ClientDev::poll(char* resultStr)	/* loop and look for messages coming in	*/
{
	// send accumulated responses
	// Notes: 1) send any pending buffers now even if we aren't still in the output list
	// (as in the "say" command, which temporarily puts us in the output list)
	// 2) some commands don't produce any output. In this case, the response will be
	// empty, but we will be in the output list.  We still send the result code in this case.
	if (mOutputBufferPos || display->isOutput(this)) {

		if (!strcmp(resultStr,"HW_ERR_IGNORE")) {
			// don't send buffer if we are told to ignore this result
			mOutputBufferPos = 0;
		} else {
			if (display->isOutput(this)) {
				// add result code to output if we are in the output list
				outString("Result: ");
				outString(resultStr);

				int resultLen = strlen(resultStr);
				// make sure we send a trailing LF after the result
				if (!resultLen || resultStr[resultLen-1]!='\n') {
					outString("\n");
				}
			}

			// write the output buffer
			writeBuffer();
		}
	}
}
//======================================================
