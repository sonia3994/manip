#include <time.h>
#include <stdlib.h>
#include "server2.h"
#include "psocket.h"
#include "clientde.h"
#include "display.h"
#include "dispatch.h"

extern Display *display;

const int	kTempTimeoutTime = 120;	// allow temporary connection for 2 minutes

int	Server2::sLastClientID = 0;
time_t Server2::sTempTime[kMaxTempClients] = { 0 };
char * Server2::sTempClientIP[kMaxTempClients] = { 0 };

Server2::Server2(char *serverName, Dispatcher *disp, FILE *fp)
{
	mDispatcher = disp;
	mNumClients = 0;
	mNumAllowedClients = 0;

	// open file and find object
	InputFile in("SERVER", serverName, 1.00, NL_FAIL, NULL, fp);

	mPortNumber = in.nextFloat("PORT_NUMBER:");
	mMaxClients = in.nextFloat("MAX_CONNECTIONS:");
	mNumListeners = in.nextFloat("NUM_LISTENERS:");

	in.setErrorFlag(NL_QUIET);

	for (mNumAllowedClients=0;;) {

		// find next client entry in file
		char *key = in.nextLine("CLIENT_NAME:");
		if (!key) break;
		char *clientName = noWS(key);

		if (!clientName) break;

		if (mNumAllowedClients >= kMaxAllowedClients) {
			eprintf("Too many client names for %s in SERVER.DAT",serverName);
			break;
		}
		char *clientIP = getClientIP(clientName);
		if (!clientIP) continue;

		mAllowedClientName[mNumAllowedClients] = new char[strlen(clientName)+1];
		mAllowedClientIP[mNumAllowedClients] = new char[strlen(clientIP)+1];

		if (!mAllowedClientName[mNumAllowedClients] || !mAllowedClientIP[mNumAllowedClients]) {
			quit("memory error");
		}
		strcpy(mAllowedClientName[mNumAllowedClients],clientName);
		strcpy(mAllowedClientIP[mNumAllowedClients],clientIP);
		++mNumAllowedClients;
	}

	if (!mNumAllowedClients) {
		display->message("No clients in SERVER.DAT -- NETWORK SECURITY DISABLED!");
	}

	if (mMaxClients > kMaxClients) {
		mMaxClients = kMaxClients;
		display->message("too many server connections");
	}
	mSocket = new PSocket;
	if (!mSocket) {
		printw("Out of memory for server socket\n");
		exit(0);
	}
	if (init(mPortNumber, mNumListeners)) {
		sprintf(display->outString,"Error %s initializing server",mSocket->GetErrorString());
		display->error();
	}
}
Server2::~Server2()
{
	deleteAllClients();
	if (mSocket) mSocket->Close();
	for (int i=0; i<mNumAllowedClients; ++i) {
		delete mAllowedClientName[i];
		delete mAllowedClientIP[i];
	}
}

int Server2::init(int portNumber, int numListeners)
{
	return mSocket->Listen(portNumber, numListeners);
}

void Server2::poll()
{
//
// accept new connections
//
	PSocket *theSocket = mSocket->Accept();
	if (theSocket) {
		char *addr_str = theSocket->GetAddress();
		int is_allowed = 0;
		int	i;
		if (!mNumAllowedClients) {
			is_allowed = 1;
		} else {
			for (i=0; i<mNumAllowedClients; ++i) {
				if (!strcmp(mAllowedClientIP[i],addr_str)) {
					addr_str = mAllowedClientName[i];	// translate client name if allowed
					is_allowed = 1;
					break;
				}
			}
		}
		// check for temporarily allowed clients
		if (!is_allowed) {
			time_t time_now = time(NULL);
			for (i=0; i<kMaxTempClients; ++i) {
				if (sTempTime[i] >= time_now && sTempClientIP[i]) {
					if (!strcmp(sTempClientIP[i], addr_str)) {
						is_allowed = 1;
						break;
					}
				}
			}
		}
		if (is_allowed) {
			theSocket->Write("CONNECTION ACCEPTED\n");
//		printw("Accepted new connection %d) %s\n",clientID,theSocket->GetAddress());
			theSocket->SetID(++sLastClientID);
			newClient(theSocket);
		} else {
			sprintf(display->outString,"ILLEGAL ACCESS ATTEMPT! %s",addr_str);
			display->messageB(1,kNetStatusLine);
//			fprintf(logfile,"%.24s ACCESS DENIED! %s\n",tm_str,addr_str);
			theSocket->Write("ACCESS DENIED\n");
			delete(theSocket);
		}
	}
//
// read commands from existing connections
//
	for (int i=0; i<mNumClients; ++i) {
		int val = mClient[i]->addCommands(mDispatcher);
		if (val == 0) {
			deleteClient(mClient[i], 0);
		} else if (val<0 && mClient[i]->getSocket()->GetErrorString()[0]) {
			deleteClient(mClient[i], 0);
//				display->message("Error %d reading from %d\n",
//								clientList[i]->GetErrorString(),clientList[i]->GetID());
		}
	}
}

char *Server2::getClientIP(char *clientName)
{
	char *clientIP = NULL;
	struct hostent *clientent = gethostbyname(clientName); 	/* get client data entry */
	if (clientent) {
		struct sockaddr_in theClient;	// internet domain socket address of allowed client
		theClient.sin_family = AF_INET;        								/* Internet domain 	*/
        theClient.sin_addr.s_addr = *(long *)clientent->h_addr;
//PH		theClient.sin_addr.s_net 	 = (char) *(clientent->h_addr); /* address bytes	*/
//		theClient.sin_addr.s_host  = (char) *((clientent->h_addr)+1);
//		theClient.sin_addr.s_lh 	 = (char) *((clientent->h_addr)+2);
//		theClient.sin_addr.s_impno = (char) *((clientent->h_addr)+3);
        clientIP = inet_ntoa(theClient.sin_addr);/*get client name in dot notation*/
	} else {
		sprintf(display->outString,"Could not get client entry for: %s\n",clientName);
		display->error();
	}
	return clientIP;
}

// allow connections from specified IP for a temporary time (2 mins)
void Server2::allowTemp(char *clientName)
{
	time_t time_now = time(NULL);
	char *clientIP = getClientIP(clientName);
	if (!clientIP) {
		display->error("Bad host name\n");
		return;
	}
	for (int i=0; i<kMaxTempClients; ++i) {
		if (sTempTime[i] < time_now) {
			if (sTempClientIP[i]) {
				delete sTempClientIP[i];
			}
			sTempClientIP[i] = new char[strlen(clientIP) + 1];
			if (sTempClientIP[i]) {
				strcpy(sTempClientIP[i], clientIP);
				sTempTime[i] = time_now + kTempTimeoutTime;
			} else {
				display->error("Not enough memory\n");
			}
			return;
		}
	}
	display->error("Temporary client list full!\n");
}


// doneCommand - called when a network command has completed execution
void Server2::doneCommand(char *responseStr)
{
	for (int i=0; i<mNumClients; ++i) {
		mClient[i]->poll(responseStr);
	}
}

ClientDev *Server2::getCurrentClient()
{
	for (int i=0; i<mNumClients; ++i) {
		if (display->isOutput(mClient[i])) {
			return(mClient[i]);
		}
	}
	return(NULL);
}

void Server2::outputAllBut(ClientDev *notThis)
{
	for (int i=0; i<mNumClients; ++i) {
		// don't send to originating host
		if (mClient[i] != notThis) {
			*display += (mClient[i]);
		}
	}
}
void Server2::deleteAllClients()
{
	while (mNumClients) {
		deleteClient(mClient[0], 1);
	}
}
void Server2::deleteClient(ClientDev *clientDev, int notify)
{
	for (int i=0; i<mNumClients; ++i) {
		if (clientDev == (mClient[i])) {
			PSocket *sock = clientDev->getSocket();
			if (sock) {
				sprintf(display->outString,"Disconnected from %s on port %d (ID %d)",
								sock->GetAddress(),	mPortNumber, sock->GetID());
				display->messageB(1,kNetStatusLine);
				if (notify) {
					sock->Write("DISCONNECT\n");
				}
			}
			// remove all dispatcher commands from this client, just to be safe
			// (otherwise, we could access a deleted object)
			mDispatcher->removeCommands(mClient[i]);
			// delete the device (this will delete the socket too!)
			delete mClient[i];
			if (i < mNumClients-1) {
				memmove(mClient+i, mClient+i+1, (mNumClients-i-1)*sizeof(ClientDev *));
			}
			--mNumClients;
			if (!mNumClients) {
				// take this opportunity to re-initialize the socket,
				// just in case it got confused somehow
				delete mSocket;
				mSocket = new PSocket;
				if (init(mPortNumber, mNumListeners)) {
					sprintf(display->outString,"Error %s initializing server",mSocket->GetErrorString());
					display->error();
				}
			}
		}
	}
}
int Server2::deleteClient(int clientID)
{
	for (int i=0; i<mNumClients; ++i) {
		if (mClient[i]->getSocket()->GetID() == clientID) {
			deleteClient(mClient[i], 1);
			return(1);
		}
	}
	return(0);
}
// getIdleClient - return pointer to client with longest idle time
ClientDev *Server2::getIdleClient()
{
	int	num = 0;
	for (int i=1; i<mNumClients; ++i) {
		if (mClient[i]->getLastRcvTime() < mClient[num]->getLastRcvTime()) {
			num = i;
		}
	}
	return(mClient[num]);
}
void Server2::newClient(PSocket *sock)
{
	if (mNumClients >= mMaxClients) {
		deleteClient(getIdleClient(), 1);
	}
	ClientDev *client = new ClientDev(sock);
	if (client) {
		mClient[mNumClients++] = client;
		sprintf(display->outString,"Connected to %s on port %d (ID %d)",
						sock->GetAddress(),	mSocket->GetPortNumber(), sock->GetID());
		display->messageB(1,kNetStatusLine);
	} else {
		display->error("Error creating client device\n");
		delete sock;
	}
}

void Server2::showClients()
{
	if (mNumClients) {
		sprintf(display->outString,"%d network client%s currently connected on port %d (max=%d):",
						mNumClients, mNumClients==1 ? "" : "s", mSocket->GetPortNumber(), mMaxClients);
		display->message();
		display->message("   ID    Client          Mode    Idle time");
		time_t timeNow;
		time(&timeNow);
		for (int i=0; i<mNumClients; ++i) {
			long	idleTime = timeNow - mClient[i]->getLastRcvTime();
			char	*timeUnits = "seconds";
			if (idleTime >= 120) {
				idleTime /= 60;
				timeUnits = "minutes";
				if (idleTime >= 120) {
					idleTime /= 60;
					timeUnits = "hours";
					if (idleTime >= 48) {
						idleTime /= 24;
						timeUnits = "days";
					}
				}
			}
			sprintf(display->outString,"%5d   %-15s %s   %ld %s",
							(int)mClient[i]->getSocket()->GetID(),
							mClient[i]->getSocket()->GetAddress(),
							mClient[i]->isExpert() ? "Expert" : "Normal",
							idleTime, timeUnits);	// !!!! eventually print idle time
			display->message();
		}
	} else {
		sprintf(display->outString,"No network clients currently connected on port %d (max=%d).",
						mSocket->GetPortNumber(), mMaxClients);
		display->message();
	}
}