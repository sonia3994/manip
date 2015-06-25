#ifndef __ClientDev_h__
#define __ClientDev_h__

#include <time.h>
#include <stdio.h>
#include "display.h"

enum EFileStatus {
	kFileNone,
	kFileWrite,
	kFileRead,
	kFileWriteDone
};

class PSocket;
class Dispatcher;

class ClientDev : public OutputDev {
public:
	ClientDev(PSocket *sock);
	virtual ~ClientDev();
	virtual void	outString(char *str);
	virtual char*	getName() { return("network"); }

	int					addCommands(Dispatcher *disp);
	void				openFile(char *name, EFileStatus status);
	int					closeFile();
	int					getFileStatus()					 { return mFileStatus; }
	void				setSocket(PSocket *sock) { mSocket = sock; }
	PSocket 	*	getSocket()							 { return mSocket; }
	void    		poll(char *resultStr);
	void				flushBuffer();
	void				writeBuffer();
	int					isExpert(int checkExpire=1);
	void				setExpert(int on);
	time_t			getLastRcvTime()				 { return mLastRcvTime; }

private:
	PSocket		*	mSocket;
	char			*	mInputBuffer;
	char			*	mOutputBuffer;	// null-terminated string output buffer
	int					mInputBufferPos;
	int					mOutputBufferPos;
	int					mExpert;
	int					mID;
	time_t			mLastRcvTime;
	time_t			mExpertTime;
	int					mCrFlags;

	FILE			*	mFile;
	EFileStatus	mFileStatus;
	char			*	mFileName;		// name of file
};

#endif // __ClientDev_h__
