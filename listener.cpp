//
// listener.cxx
//
// Test program to demonstrate server application of PSocket class
//
// - PH 09/26/00
//
extern "C" {
#include <stdio.h>
#include <string.h>
}
#include "psocket.h"
#include "doslib.h"

const int kMaxClients = 5;

void other_main()
{
	int i, j;
	int	count = 0;
	int id = 0;
	int done = 0;
	PSocket	server;
	PSocket *clientList[kMaxClients];

	printw("Running...\n");

	if (server.Listen(2005,3)) {
		printw("%s\n",server.GetErrorString());
	} else {
		while (!done) {
			PSocket *theSocket = server.Accept();
			if (theSocket) {
				printw("Accepted new connection %d) %s\n",id,theSocket->GetAddress());
				theSocket->SetID(id);
				if (count >= kMaxClients) {
					printw("Disconnected %d) %s\n",clientList[0]->GetID(),clientList[0]->GetAddress());
					clientList[0]->Write("Too many connections\n");
					delete clientList[0];
					memmove(clientList, clientList+1, (kMaxClients-1)*sizeof(PSocket *));
					--count;
				}
				clientList[count++] = theSocket;
				++id;
			}
			for (i=0; i<count; ++i) {
				const int kBuffSize = 256;
				char buff1[kBuffSize-1];
				int num = clientList[i]->Read(buff1, kBuffSize);
				if (num > 0) {
					buff1[num] = '\0';	// null terminate string
					for (j=0; j<count; ++j) {
						if (j == i) {
							clientList[i]->Write("You sent: ");
						} else {
							char buff2[64];
							sprintf(buff2,"%d) ",clientList[i]->GetID());
							clientList[j]->Write(buff2);
						}
						int n = clientList[j]->Write(buff1);	// echo back to host
						if (n != num) {
							printw("Error writing: wrote %d instead of %d to %d (%s)\n",
										n,num,clientList[j]->GetID(),clientList[j]->GetErrorString());
						}
					}
					// trim off trailing CR or LF
					while (num && (buff1[num-1]=='\r' || buff1[num-1]=='\n')) --num;
					buff1[num] = '\0';
					printw("Received %d) %s\n",clientList[i]->GetID(),buff1);
					if (!strcmp(buff1,"quit")) {
						done = 1;
						break;
					}
				} else if (num == 0) {
					printw("%d) disconnected (%s)\n",clientList[i]->GetID(),clientList[i]->GetAddress());
					delete clientList[i];
					if (i < count-1) {
						memmove(clientList+i, clientList+i+1, (count-i-1)*sizeof(PSocket *));
					}
				} else if (clientList[i]->GetErrorString()[0]) {
					printw("Error %d reading from %d\n",
									clientList[i]->GetErrorString(),clientList[i]->GetID());
				}
			}
		}
	}
	for (i=0; i<count; ++i) {
		clientList[i]->Write("Quitting...\n");
		delete clientList[i];
	}
	server.Close();
	printw("Done!\n");
}
