//
// File:		PSocket.h
//
// Description:	Network socket class built on Unix socket library
//
// Created:		06/01/00 - P. Harvey
//
#ifndef __PSocket_h__
#define __PSocket_h__

extern "C"
{
#include  <fcntl.h>
#include	<errno.h>
#include	<sys/types.h>
#include	<sys/socket.h>
#include 	<netinet/in.h>
#include	<netdb.h>

//PH#include	<pctcp/error.h>

#ifndef MSW3              /* this stuff is cut out of <arpa\inet.h> 			  */
#define DLLFAR            /* to avoid bad keyword (``class'') in one stuct  */
#endif /* MSW3 */         /* (BCC failure here may be a bug, or a bad option*/
char DLLFAR * DLLFAR inet_ntoa(struct in_addr);
}

// class description
class PSocket 
{
public:
				PSocket();
				~PSocket();
	
	int			Open(char *ip_addr, int portnum);
	
	int			Listen(int portnum, int numListeners=2);
	PSocket *	Accept();
	
	int			Close();
	
	int			Read(char *buff, int buffsize);
	int			Write(char *str);
	
	char *	GetAddress();
	char *	GetErrorString()	{ return mErrorBuffer; }
	int			GetPortNumber()		{ return mPortNumber; }

	void		SetID(int id)			{ mID = id;		}
	int			GetID()						{ return mID;	}

private:
	int			mSocket;			// socket descriptor
	sockaddr_in	mServer;			// server description
	char		mErrorBuffer[128];	// buffer for error string
	int			mPortNumber;	// port number
	int			mID;				// user settable ID number
};

#endif // __PSocket_h__
