//
// File:		PSocket.cxx
//
// Description:	Network socket class built on Unix socket library
//
// Created:		06/01/00 - P. Harvey
//
// Revisions:	09/25/00 - PH Fixed bug where socket wasn't closed properly if connect() failed.
//				09/26/00 - PH Added Listen() and Accept().
//
// Notes:		There are two ways to use a PSocket object...
//
//				1) To connect to a specific host:
//					a) Create a PSocket object.
//					b) Call Open() with the host IP and port number.
//					c) If Open succeeds (returns 0), then call Read()/Write()
//						to communicate with the specified host.
//					d) Call Close() or delete the PSocket object when done.
//
//				2) To accept incoming connections:
//					a) Create a PSocket object.
//					b) Call Listen() with the port number.
//					c) Call Accept() repeatedly -- this will return a new
//						PSocket object if a connection is made.
//					d) Use Read()/Write() with the new PSocket object to
//						communicate with the newly connected host.  Continue
//						to monitor for new connections using the original
//						PSocket object's Accept() while communicating.
//					e) Delete the new PSocket to terminate communication.
//					f) Call Close() or delete the original PSocket object
//						to stop listening for new connections.  Note that
//						this can be done while the existing connections are
//						still open.
//
//				- Call GetErrorString() to get a description of the error if
//				  any function call returns an error code.
//
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include "psocket.h"

//PH typedef int socklen_t;

// function prototypes
unsigned long atonl(char *addr_str);

// PSocket constructor
PSocket::PSocket()
{
	mSocket			= -1;
	mPortNumber	= 0;
	mErrorBuffer[0]	= '\0';
}

// PSocket destructor
PSocket::~PSocket()
{
	Close();
}

// Open - open connection to remote host
// - returns 0 on success
int PSocket::Open(char *ip_addr, int portnum)
{
	Close();	// be sure any old connections are closed first

	mPortNumber = portnum;

	// create socket
	mSocket = socket(AF_INET, SOCK_STREAM, 0);

	if (mSocket == -1) {
		strcpy(mErrorBuffer, "Error creating socket");
		return(-1);
	}
	
	mServer.sin_family = AF_INET;				// set communications domain
	mServer.sin_addr.s_addr = atonl(ip_addr); 	// set IP address
	mServer.sin_port = htons(portnum);	 		// set port number
	
	// attempt to connect with remote host
	if (connect(mSocket, (struct sockaddr*)&mServer, sizeof(mServer)) < 0) {
		sprintf(mErrorBuffer, "Error %d connecting to server (%s)",errno,strerror(errno));
		return(-2);
	}
	
	// set socket to non-blocking
	if (fcntl(mSocket,F_SETFL,FNDELAY) < 0) {
		strcpy(mErrorBuffer, "Error setting up socket");
		return(-3);
	}

	return(0);
}

// Listen - open new socket and listen for incoming connections
int PSocket::Listen(int portnum, int numListeners)
{
	Close();	// be sure any old connections are closed first

	mPortNumber = portnum;

	mSocket = socket(AF_INET,SOCK_STREAM,0);		// create socket

	if (mSocket == -1)	{							// creation failed
		strcpy(mErrorBuffer, "Error creating socket");
		return(-1);
	}
    // allow the socket to be re-created after closing without having to wait - PH
    int reuse = 1;
    setsockopt(mSocket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    
	mServer.sin_family = AF_INET;			// set communications domain
	mServer.sin_addr.s_addr = INADDR_ANY;	// any machine ok as server
	mServer.sin_port = htons(portnum);	 	// set port number

	// bind socket to server
	if (bind(mSocket, (struct sockaddr*)&mServer, sizeof(mServer))) {
		strcpy(mErrorBuffer, "Binding socket stream to server failed");
		return(-2);
	}
	socklen_t len = sizeof(mServer);
	// get socket name
	if (getsockname(mSocket, (struct sockaddr*)&mServer, &len)) {
		strcpy(mErrorBuffer, "Getting socket name failed");
		return(-3);
	}

	listen(mSocket, numListeners);	/* active socket and create connection queue */

	// make socket non-blocking
	if (fcntl(mSocket,F_SETFL,FNDELAY) < 0) {
		strcpy(mErrorBuffer, "Error setting up socket");
		return(-4);
	}
	
	return(0);
}

// Accept - accept new connection
// - returns new socket object if successful (must be deleted by caller)
PSocket * PSocket::Accept()
{
	PSocket	*newSocket = NULL;
	
	// try to accept incoming connection
	struct sockaddr_in theServer;
	socklen_t len = sizeof(theServer);
	int theSocket = accept(mSocket, (struct sockaddr*)&theServer, &len);
	
	if (theSocket == -1) {
		if (errno == EWOULDBLOCK) {
			mErrorBuffer[0] = '\0';		// normal error - no new connection
		} else {
			strcpy(mErrorBuffer, strerror(errno));
		}
	} else {
		newSocket = new PSocket;
		if (newSocket) {
			// make socket non-blocking
			if (fcntl(theSocket,F_SETFL,FNDELAY) < 0) {
				strcpy(mErrorBuffer, "Error setting up socket");
				delete newSocket;
				newSocket = NULL;
			} else {
				newSocket->mSocket = theSocket;
				memcpy(&newSocket->mServer, &theServer, sizeof(theServer));
			}
		} else {
			close(theSocket);
		}
	}
	return(newSocket);
}

// GetAddress - get IP address of connected host
char *PSocket::GetAddress()
{
	if (mSocket == -1) {
		return("(no socket)");
	} else {
		return(inet_ntoa(mServer.sin_addr));
	}
}

// Close - close connection with remote host
// - returns 0 on success
int PSocket::Close()
{
	if (mSocket >= 0) {
		int sock = mSocket;
		mSocket = -1;		// reset socket number

// This is BAAADDD!! --> causes socket to never be closed if connect() failed!! - PH 09/25/00
/*		if (shutdown(sock, SHUT_RDWR) < 0) {
			strcpy(mErrorBuffer, "Error shutting down socket");
			return(-1);
		}*/
		if (close(sock) < 0) {
			strcpy(mErrorBuffer, "Error closing socket");
			return(-2);
		}
	}
	return(0);
}

// Read - read bytes from remote host into buffer
// - returns number of bytes read
// - if 0 is returned, remote host has disconnected (call Close() to terminate connection)
// - if <0 returned, errno is set according to the error type
//   (should ignore EWOULDBLOCK errors since the socket is
//	  set to non-blocking by default)
int PSocket::Read(char *buff, int buffsize)
{
	int rtnVal = read(mSocket, buff, buffsize);
	if (rtnVal < 0) {
		if (errno == EWOULDBLOCK) {
			mErrorBuffer[0] = '\0';		// normal error - no new data
		} else {
			strcpy(mErrorBuffer, strerror(errno));	// set our error string on any error
		}
	}
	return(rtnVal);
}

// Write - write a string to remote host
// - string must be null terminated
// - returns number of bytes written on success
// - returns < 0 on error (errno is set to error type)
int PSocket::Write(char *str)
{
	// write the string to the socket
	int rtnVal = write(mSocket, str, strlen(str));
	if (rtnVal < 0) {
		strcpy(mErrorBuffer, strerror(errno));	// set our error string on any error
	}
	return(rtnVal);
}

// atonl - utility to convert from ASCII IP to network long integer
unsigned long atonl(char *addr_str)
{
	unsigned long 	ip = 0;
	int				shift = 24;
	char *			pt = addr_str;
	
	for (;;) {
		ip |= (atoi(pt) << shift);
		shift -= 8;
		if (shift < 0) break;
		pt = strchr(pt,'.');
		if (!pt) {
			fprintf(stderr,"IP address format error!");
			break;
		}
		++pt;
	}
	return(htonl(ip));
}


