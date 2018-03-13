#include "CTP.h"
#include <iostream>
#include <stdlib.h>
using namespace std;

CTP::CTP(int sockfd, struct sockaddr_in serveraddr)
{
	this->sockfd = sockfd;
	this->serveraddr = serveraddr;
}

int CTP::appSend(string filename)
{
	cout<<"Sendinf file "<<filename<<endl;
}

/*class CTP{
	CTP(int sockfd, struct sockaddr_in serveraddr)
	{
		this->sockfd = sockfd;
		this->serveraddr = serveraddr;
	}

	int appSend(string filename)
	{
		cout<<"Sendinf file "<<filename<<endl;
	}
};*/


int main()
{
	struct sockaddr_in serveraddr;
	CTP *my = new CTP(0,serveraddr);
	my->appSend("yola.txt");
}