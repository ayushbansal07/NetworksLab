#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <map>
#include <vector>
#include <fcntl.h>
#include <string>
#include <utility>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <cmath>

#define MAX_NAME_LEN 20
#define MAX_BUF_SIZE 1024
#define TIMEOUTVAL 30
#define MAX_CONNECTIONS 5
#define S second
#define F first

using namespace std;
typedef pair<string, int> PI;
typedef pair<int, time_t> PT;
typedef pair<PT, string> PTN;

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

map<PI, PTN> init_isOpen(){
	map<PI, PTN> ans;
	ifstream f;
	f.open("user_info.in");
	string name;
	while(f>>name)
	{
		string hostname;
		f>>hostname;
		int portno;
		f>>portno;
		PI temp = make_pair(hostname,portno);
		ans[temp] = make_pair(make_pair(0, time(NULL)),name);
	}
	return ans;
}

int main(int argc, char ** argv)
{
	int server_sock;

	if(argc != 2)
	{
		fprintf(stderr,"Usage: %s <port>\n",argv[0]);
		exit(1);
	}
	int portno = atoi(argv[1]);

	map<string, struct sockaddr_in> user_info = get_user_info();
	map<PI, PTN> isOpen = init_isOpen();
	//vector<int> open_peers(5,0);

	server_sock = socket(AF_INET,SOCK_STREAM,0);
	if(server_sock < 0) cout<<"Error opening socket"<<endl;

	int optval  = 1;
	setsockopt(server_sock, SOL_SOCKET,SO_REUSEADDR,(const void *) &optval, sizeof(int));

	struct sockaddr_in serveraddr;
	bzero((char *) &serveraddr, sizeof(serveraddr));

	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons((unsigned short) portno);

	if(bind(server_sock, (struct sockaddr *) &serveraddr,sizeof(serveraddr)) < 0)
		cout<<"Error binding Server Socket to port"<<endl;

	if(listen(server_sock, MAX_CONNECTIONS) < 0){
		cout<<"Error listening"<<endl;
	}

	fd_set readfds;

	int max_sd = server_sock;

	struct sockaddr_in clientaddr;
	socklen_t clientlen;

	char buffer[MAX_BUF_SIZE];

	int num_open = 0;
	struct timeval tv;
    tv.tv_sec = TIMEOUTVAL;
    tv.tv_usec = 0;
    

	while(1)
	{
		double max_diff_time = 0;
		FD_ZERO(&readfds);
		FD_SET(server_sock, &readfds);
		FD_SET(0,&readfds);
		map<PI, PTN>::iterator itr;
		for(itr = isOpen.begin(); itr != isOpen.end();itr++)
		{
			int sd = itr->S.F.F;
			if(sd > 0)
			{
				double diff_time = difftime(time(NULL),itr->S.F.S);
				if(diff_time <= TIMEOUTVAL)
				{
					FD_SET(sd, &readfds);
					max_sd = max(sd,max_sd);
					max_diff_time = max(diff_time,max_diff_time);
				}
				else
				{
					cout<<"Disconnecting "<<itr->F.F<<":"<<itr->F.S<<endl;
					close(sd);
					itr->S.F.F = 0;
					itr->S.F.S = time(NULL);
					num_open--;
				}
				
			}
		}

		cout<<"-----------------------------\nWaiting for Connection or user input, No. of Open Connections = "<<num_open<<"\n-----------------------------"<<endl;


		tv.tv_sec = TIMEOUTVAL - floor(max_diff_time);
		int activity = select(max_sd + 1, &readfds, NULL, NULL, &tv);
		if(activity < 0)
		{
			cout<<"Select error"<<endl;
		}

		if(FD_ISSET(0,&readfds))
		{
			//stdin
			bzero(buffer,MAX_BUF_SIZE);
			int n = read(0,buffer, MAX_BUF_SIZE);
			if(n==0)
			{
				cout<<"Error reading from stdin"<<endl;
				continue;
			}
			string ip = string(buffer);
			size_t found = ip.find("/");
			string username = ip.substr(0,found);
			strcpy(buffer, ip.substr(found+1).c_str());
			map<string,struct sockaddr_in>::iterator itr2 = user_info.find(username);
			if(itr2 == user_info.end())
			{
				cout<<"Invalid Username. User Does Not Exist"<<endl;
			}
			else
			{
				//cout<<"User exists"<<endl;
				//See if connection already exists
				PI temp;
				temp.F = string(inet_ntoa(itr2->S.sin_addr));
				temp.S = ntohs(itr2->S.sin_port);
				int childfd = isOpen[temp].F.F;
				if(childfd > 0)
				{
					if(write(childfd, buffer, MAX_BUF_SIZE) < 0)
						cout<<"Error writing to socket"<<endl;
					isOpen[temp].F.S = time(NULL);
				}
				else
				{
					if(num_open >= MAX_CONNECTIONS) continue;
					struct sockaddr_in connection_addr = itr2->S;
					childfd = socket(AF_INET, SOCK_STREAM, 0);
					if(childfd < 0)
						cout<<"Error creating socket to write"<<endl;
					if(connect(childfd, (struct sockaddr *) &connection_addr, sizeof(connection_addr)) < 0)
					{
						cout<<"Error conencting to server"<<endl;
					}
					isOpen[temp] = make_pair(make_pair(childfd,time(NULL)),isOpen[temp].S);
					num_open++;
					cout<<"-----------------------------\nOpening Connection "<<temp.F<<":"<<temp.S<<"\n-----------------------------\n";
					if(write(childfd,&portno,sizeof(int))<0) cout<<"Error telling port"<<endl;
					//if(setsockopt(childfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv))< 0) cout<<"Error in sockopt"<<endl;
					if(write(childfd, buffer, MAX_BUF_SIZE) < 0)
						cout<<"Error writing to socket"<<endl;	
				}
			}

		}


		if(FD_ISSET(server_sock,&readfds))
		{
			if(num_open >= MAX_CONNECTIONS){
				continue;
			}


			int childfd = accept(server_sock, (struct sockaddr *) &clientaddr, &clientlen);
			if(childfd < 0)
			{
				cout<<"Error in accept"<<endl;
				continue;
			}
			PI temp;
			struct sockaddr_in peer_addr;
			socklen_t peer_len = sizeof(peer_addr);
			getpeername(childfd, (struct sockaddr *)&peer_addr, &peer_len);
			temp.F = string(inet_ntoa(peer_addr.sin_addr));
			if(read(childfd, &temp.S,sizeof(int)) < 0) cout<<"Error reading port no"<<endl;
			//temp.S = ntohs(peer_addr.sin_port);
			isOpen[temp] = make_pair(make_pair(childfd,time(NULL)),isOpen[temp].S);
			num_open++;
			cout<<"-----------------------------\nAccept Opening connection "<<temp.F<<":"<<temp.S<<"\n-----------------------------\n";

		}


		bzero(buffer, MAX_BUF_SIZE);
		for(itr = isOpen.begin(); itr != isOpen.end(); itr++)
		{
			int sd = itr->S.F.F;
			if(sd==0) continue;
			if(FD_ISSET(sd,&readfds))
			{
				int n = read(sd,buffer,MAX_BUF_SIZE);
				if(n==0)
				{
					cout<<"-----------------------------\nDisconnecting "<<itr->F.F<<":"<<itr->F.S<<endl<<"-----------------------------\n";
					close(sd);
					itr->S = make_pair(make_pair(0,time(NULL)),itr->S.S);
					num_open--;
				}
				else
				{
					cout<<itr->S.S<<": "<<buffer<<endl;
					bzero(buffer,MAX_BUF_SIZE);
					itr->S.F.S = time(NULL);
				}
			}
		}
		

	}


}