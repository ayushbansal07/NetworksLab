#ifndef CTP_H
#define CTP_H

#include <sys/types.h>
#include <sys/socket.h>
#include <string>
#include <netdb.h>

using namespace std;

class CTP{
public:
	int sockfd;
	struct sockaddr_in serveraddr;

public:
	CTP(int sockfd, struct sockaddr_in serveraddr);
	int appSend(string filename);

};
#endif