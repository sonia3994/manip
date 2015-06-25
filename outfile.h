// class to support an output file device - PH 11/21/97

#ifndef __OUTFILE_H
#define __OUTFILE_H

#include <stdio.h>
#include "display.h"

class OutputDevFile : public OutputDev {
public:
	OutputDevFile(char *filename, int bufferedOutput=0);
	~OutputDevFile();
		
	virtual void	outString(char *str);
	virtual char*	getName()	{ return mFileName;	}

	virtual void	setName(char *filename);
	
	void		flush();
	
private:
	void		openFile();
	void		closeFile();
	
	char*		mFileName;
	char*		mBuffer;
	int			mBufferBytes;
};

#endif
