#include <iostream>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <cstring>
#include <ctime>
#include <linux/icmp.h>
#include <linux/ip.h>

using namespace std;

struct EchoRequest{
	icmphdr header;
	time_t sendTime;
};

unsigned short int get_checkSum(unsigned short int * bytes, int sz)
{
	unsigned short int ans = 0;
	while(sz > 0)
	{
		ans += (*bytes);
		bytes++;
		sz -= sizeof(unsigned short int);
	}
	return ans;

}

int main()
{
	int fd = socket(AF_INET,SOCK_RAW,IPPROTO_ICMP);
	if(fd < 0) cout<<"Error creating socket"<<endl;

	struct hostent *server = gethostbyname("127.0.0.1");
	if(server == NULL) cout<<"Erorr no such host exists"<<endl;

	struct sockaddr_in server_addr;
	bzero((char *) &server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	bcopy((char *)server->h_addr, (char *)&server_addr.sin_addr.s_addr, server->h_length);

	int this_pid = htons(getpid());
	int seqNo = 0;
	EchoRequest request;
	request.header.type = ICMP_ECHO;	//ICMP_ECHO = 8 (Request)
	request.header.code = 0;
	request.header.un.echo.id = this_pid;
	request.header.un.echo.sequence = seqNo++;
	request.header.checksum = 0;
	request.sendTime = time(NULL);
	request.header.checksum = get_checkSum((unsigned short int *)&request,sizeof(request));

	int n = sendto(fd, (void *)&request, sizeof(request), 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
	if(n<0) cout<<"Error Sending Request"<<endl;






}