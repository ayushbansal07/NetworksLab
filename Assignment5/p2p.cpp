#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <fstream>
#include <map>

#define MAX_NAME_LEN 20
using namespace std;


/*struct user_info{
	char name[MAX_NAME_LEN];
	struct sockaddr_in address;
};*/

map<string, struct sockaddr_in> get_user_info(){
	ifstream f;
	map<string, struct sockaddr_in> res;
	f.open("user_info.in");
	string name;
	while(f>>name){
		struct sockaddr_in serveraddr;
		char hostname[MAX_NAME_LEN];
		f>>hostname;
		struct hostent *server;
		server = gethostbyname(hostname);
		if (server == NULL) {
	        fprintf(stderr,"ERROR, no such host as %s\n", hostname);
	        exit(0);
	    }
	    bzero((char *) &serveraddr, sizeof(serveraddr));
	    serveraddr.sin_family = AF_INET;
	    bcopy((char *)server->h_addr, (char *)&serveraddr.sin_addr.s_addr, server->h_length);
	    int portno;
	    f>>portno;
	    serveraddr.sin_port = htons(portno);
	    res[name] = serveraddr;
	}

	return res;
	

}

int main(int argc, char ** argv)
{
	int sockfd;

	if(argc != 2)
	{
		fprintf(stderr,"Usage: %s <port>\n",argv[0]);
		exit(1);
	}
	int portno = atoi(argv[1]);

	map<string, struct sockaddr_in> user_info = get_user_info();
	/*map<string, struct sockaddr_in>::iterator itr;
	for(itr = user_info.begin();itr != user_info.end();itr++)
	{
		cout<<itr->first<<endl;
	}*/

	sockfd = socket(AF_INET,SOCK_STREAM,0);
	if(sockfd < 0) cout<<"Error opening socket"<<endl;

	int optval  = 1;
	setsockopt(sockfd, SOL_SOCKET,SO_REUSEADDR,(const void *) &optval, sizeof(int));

	struct sockaddr_in serveraddr;
	bzero((char *) &serveraddr, sizeof(serveraddr));

	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons((unsigned short) portno);

	if(bind(sockfd, (struct sockaddr *) &serveraddr,sizeof(serveraddr)) < 0)
		cout<<"Error binding Server Socket to port"<<endl;





}