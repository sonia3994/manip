//
// Server2.h - new server object (PH 09/29/00)
//
#ifndef __Server2_h__
#define __Server2_h__

#include <stdio.h>
#include <time.h>

const int		kMaxClients = 20;
const int		kMaxAllowedClients	= 32;
const int		kMaxTempClients = 10;

class PSocket;
class ClientDev;
class Dispatcher;

class Server2
{
public:
	Server2(char *serverName, Dispatcher *disp, FILE *fp);
	~Server2();

	int					init(int portNumber, int numListeners);
	void				poll();
	void				doneCommand(char *responseStr);

	ClientDev	*	getCurrentClient();
	ClientDev *	getIdleClient();
	void				outputAllBut(ClientDev *notThis);
	void				deleteAllClients();
	void				deleteClient(ClientDev *client, int notify);
	int					deleteClient(int clientID);
	void				showClients();

	static void		allowTemp(char *clientName);
	static char* 	getClientIP(char *clientName);

private:
	void				newClient(PSocket *sock);

	int					mNumClients;
	ClientDev	*	mClient[kMaxClients];
	int					mPortNumber;
	int					mNumListeners;
	int					mMaxClients;
	PSocket		*	mSocket;
	Dispatcher*	mDispatcher;

	int			mNumAllowedClients;
	char*		mAllowedClientName[kMaxAllowedClients];	// list of allowed client names from SERVER.DAT
	char*		mAllowedClientIP[kMaxAllowedClients];		// dot-formated allowed client IP's

	static char*	sTempClientIP[kMaxTempClients];
	static time_t	sTempTime[kMaxTempClients];
	static int		sLastClientID;
};


#endif // __Server2_h__
