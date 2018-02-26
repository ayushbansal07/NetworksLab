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

#define MAX_NAME_LEN 20
#define MAX_BUF_SIZE 1024
#define S second

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

/*map<string, int> init_isOpen(){
	map<string, int> ans;
	ifstream f;
	f.open("user_info.in");
	string name;
	while(f>>name)
	{
		ans[name] = 0;
		char hostname[MAX_NAME_LEN];
		f>>hostname;
		int portno;
		f>>portno;
	}
	return ans;
}*/

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
	//map<string, int> isOpen = init_isOpen();
	vector<int> open_peers(5,0);

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

	if(listen(server_sock, 5) < 0){
		cout<<"Error listening"<<endl;
	}

	fd_set readfds;

	int max_sd = server_sock;

	struct sockaddr_in clientaddr;
	socklen_t clientlen;

	char buffer[MAX_BUF_SIZE];

	while(1)
	{
		FD_ZERO(&readfds);
		FD_SET(server_sock, &readfds);
		FD_SET(0,&readfds);
		/*map<string, int>::iterator itr;
		for(itr = isOpen.begin(); itr != isOpen.end();itr++)
		{
			int sd = itr->S;
			if(sd > 0)
			{
				FD_SET(sd, &readfds);
				max_sd = max(sd,max_sd);
			}
		}*/
		for(int i=0;i<5;i++)
		{
			if(open_peers[i] > 0)
			{
				FD_SET(open_peers[i],&readfds);
				max_sd = max(open_peers[i], max_sd);
			}
		}

		int activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);
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
			//cout<<"User: "<<ip.substr(0,found)<<" Meaasge: "<<ip.substr(found+1)<<endl;
			string username = ip.substr(0,found);
			cout<<"Username: "<<username<<endl;
			strcpy(buffer, ip.substr(found+1).c_str());
			map<string,struct sockaddr_in>::iterator itr = user_info.find(username);
			if(itr == user_info.end())
			{
				cout<<"Invalid Username"<<endl;
			}
			else
			{
				cout<<"User exists"<<endl;
				//TO CONTINUE
				struct sockaddr_in connection_addr = itr->S;
				int childfd = socket(AF_INET, SOCK_STREAM, 0);
				if(childfd < 0)
					cout<<"Error creating socket to write"<<endl;
				if(connect(childfd, (struct sockaddr *) &connection_addr, sizeof(connection_addr)) < 0)
				{
					cout<<"Error conencting to server"<<endl;
				}
				if(write(childfd, buffer, MAX_BUF_SIZE) < 0)
					cout<<"Error writing to socket"<<endl;
				for(int i=0;i<5;i++)
				{
					if(open_peers[i]==0)
					{
						open_peers[i] = childfd;
						cout<<"Peer added to list"<<endl;
						break;
					}
					
				}

			}

		}

		if(FD_ISSET(server_sock,&readfds))
		{
			clientlen = sizeof(clientaddr);
			int childfd = accept(server_sock, (struct sockaddr *) &clientaddr, &clientlen);
			if(childfd < 0)
			{
				cout<<"Error in accept"<<endl;
				continue;
			}
			for(int i=0;i<5;i++)
			{
				if(open_peers[i]==0)
				{
					open_peers[i] = childfd;
					cout<<"Peer added to list"<<endl;
					break;
				}
			}

		}

		bzero(buffer, MAX_BUF_SIZE);
		for(int i=0;i<5;i++)
		{
			if(FD_ISSET(open_peers[i],&readfds))
			{
				int n = read(open_peers[i], buffer, MAX_BUF_SIZE);
				if(n==0)
				{
					cout<<"Disconnecting"<<endl;
					close(open_peers[i]);
					open_peers[i] = 0;
				}
				cout<<buffer<<endl;
				bzero(buffer,MAX_BUF_SIZE);
			}
		}
		

	}





}