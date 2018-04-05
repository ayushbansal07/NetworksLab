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
#include <signal.h>
#include <climits>
#include <cstdlib>

using namespace std;

int num_transmit, num_recvd; 
double min_rtt, max_rtt, sum_rtt;
int max_ttl;


struct EchoRequest{
	icmphdr header;
	timespec sendTime;
};

int32_t checksum(uint16_t *buf, int32_t len){
    int32_t nleft = len;
    int32_t sum = 0;
    uint16_t *w = buf;
    uint16_t answer = 0;

    while(nleft > 1){
        sum += *w++;
        nleft -= 2;
    }

    if(nleft == 1){
        *(uint16_t *)(&answer) = *(uint8_t *)w;
        sum += answer;
    }

    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    answer = ~sum;

    return answer;
}

void ping_stats()
{
	cout<<"----- PING STATISTICS ------"<<endl;
	cout<<num_transmit<<" packets transmitted, "<<num_recvd<<" packets received, "<<(num_transmit - num_recvd)*100.0/num_transmit<<"% packet loss"<<endl;
	cout<<"rtt min/avg/max = "<<min_rtt<<"/"<<sum_rtt/num_recvd<<"/"<<max_rtt<<" ms"<<endl;
}

void sigint_handler(int signum){
	ping_stats();
	exit(0);
}

void ping_reply(int fd, int seqNo)
{
	while(1)
	{
		int size_replybuff = sizeof(iphdr)+sizeof(EchoRequest);
		char ipbuff[size_replybuff];
		bzero(ipbuff,sizeof(ipbuff));
		struct sockaddr_in reciever_addr;
		socklen_t recv_addr_len;
		int n = recvfrom(fd,ipbuff,sizeof(ipbuff),0, (struct sockaddr *)&reciever_addr,&recv_addr_len);
		if(n<0){
			cout<<"Time out while receiving response"<<endl;
			break;
		} 
		iphdr * ip_header = (iphdr *)ipbuff;
		EchoRequest * reply = (EchoRequest *)(ipbuff + (ip_header->ihl << 2));
		int id = ntohs(reply->header.un.echo.id);
		int seq_recv = ntohs(reply->header.un.echo.sequence);
		if(id == getpid() && seq_recv == seqNo)
		{
			if(reply->header.type == ICMP_ECHOREPLY)
			{
				timespec now;
				timespec_get(&now, TIME_UTC);
				int timediff = now.tv_nsec - reply->sendTime.tv_nsec;
				cout<<n<<" bytes from "<<inet_ntoa(reciever_addr.sin_addr)<<": icmp_seq = "<<seqNo<<" ttl = "<<max_ttl<<" time = "<<timediff/1e6<<" ms"<<endl;
				num_recvd++;
				min_rtt = min(min_rtt, timediff*1.0/1e6);
				max_rtt = max(max_rtt, timediff*1.0/1e6);
				sum_rtt += timediff*1.0/1e6;
				break;
			}
			else if(reply->header.type == ICMP_DEST_UNREACH)
			{
				cout<<"Destination NOT reacheable"<<endl;
				break;
			}
			else if(reply->header.type == ICMP_ECHO)
			{
				cout<<"Received Request"<<endl;
			}
			else
			{
				cout<<"Some other msg recived"<<endl;
				break;
			}
		}
	}
}

void ping_ICMP(char * hostname)
{
	int fd = socket(AF_INET,SOCK_RAW,IPPROTO_ICMP);
	if(fd < 0) cout<<"Error creating socket"<<endl;

	struct hostent *server = gethostbyname(hostname);
	if(server == NULL) cout<<"Erorr no such host exists"<<endl;

	struct sockaddr_in server_addr;
	bzero((char *) &server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	bcopy((char *)server->h_addr, (char *)&server_addr.sin_addr,sizeof(server_addr.sin_addr));

	int this_pid = htons(getpid());
	int seqNo = 0;
	EchoRequest request;

	// set recieving timeout
	timeval timeout;
	timeout.tv_sec = 1;
	timeout.tv_usec = 0;
	if(setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0){
		cout << "Timeout setting failed!" << endl;
		exit(-1);
	}

	// set TTL
	max_ttl = 50;
	if(setsockopt(fd, IPPROTO_IP, IP_TTL, &max_ttl, sizeof(int)) < 0){
		cout << "TTL setting failed" << endl;
		exit(-1);
	}

	request.header.type = ICMP_ECHO;	//ICMP_ECHO = 8 (Request)
	request.header.code = 0;
	request.header.un.echo.id = this_pid;

	cout<<"PING "<<hostname<<endl;

	for(int i=0;i<20;i++)
	{
		request.header.un.echo.sequence = htons(seqNo);
		request.header.checksum = 0;
		timespec_get(&request.sendTime, TIME_UTC);
		request.header.checksum = checksum((unsigned short int*)&request, sizeof(request));

		int n = sendto(fd, (void *)&request, sizeof(request), 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
		if(n<0){
			cout<<"Error Sending Request"<<endl;
			continue;	
		} 
		num_transmit++;
		
		ping_reply(fd, seqNo);
		seqNo++;
		sleep(1);
	}

	ping_stats();
}

void ping_header(char * hostname)
{
	int fd = socket(AF_INET,SOCK_RAW,IPPROTO_ICMP);
	if(fd < 0) cout<<"Error creating socket"<<endl;

	struct hostent *server = gethostbyname(hostname);
	if(server == NULL) cout<<"Erorr no such host exists"<<endl;

	struct sockaddr_in server_addr;
	bzero((char *) &server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	bcopy((char *)server->h_addr, (char *)&server_addr.sin_addr,sizeof(server_addr.sin_addr));

	int this_pid = htons(getpid());
	int seqNo = 0;

	// set recieving timeout
	timeval timeout;
	timeout.tv_sec = 1;
	timeout.tv_usec = 0;
	if(setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0){
		cout << "Timeout setting failed!" << endl;
		exit(-1);
	}

	int on = 1;
	if(setsockopt(fd, IPPROTO_IP, IP_HDRINCL, (char*)&on, sizeof(on)) < 0){
        cout << "IP_HDRINCL cannot be set" << endl; 
		exit(-1);
    }

    int size_reqbuff = sizeof(iphdr) + sizeof(EchoRequest);
    char ipbuff[size_reqbuff];
    bzero((char *)&ipbuff,sizeof(ipbuff));
    iphdr * ip_header = (iphdr *)ipbuff;
    EchoRequest * request = (EchoRequest *)(ipbuff + sizeof(iphdr));

    ip_header->version = 4;
	ip_header->ihl = 5;
	ip_header->tos = 0;
	ip_header->frag_off = 0;
	ip_header->ttl = 50;
	ip_header->protocol = 1;
	ip_header->daddr = server_addr.sin_addr.s_addr;

	request->header.type = ICMP_ECHO;	//ICMP_ECHO = 8 (Request)
	request->header.code = 0;
	request->header.un.echo.id = this_pid;

	cout<<"PING "<<hostname<<endl;

	for(int i=0;i<20;i++)
	{
		request->header.un.echo.sequence = htons(seqNo);
		request->header.checksum = 0;
		timespec_get(&request->sendTime, TIME_UTC);
		request->header.checksum = checksum((unsigned short int*)request, sizeof(EchoRequest));

		int n = sendto(fd, ipbuff, sizeof(ipbuff), 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
		if(n<0){
			cout<<"Error Sending Request"<<endl;
			continue;	
		} 
		num_transmit++;
		
		ping_reply(fd, seqNo);
		seqNo++;
		sleep(1);
	}

	ping_stats();


}

int main(int argc, char ** argv)
{
	signal(SIGINT, sigint_handler);

	num_transmit = 0; 
	num_recvd = 0;
	min_rtt = INT_MAX;
	max_rtt = 0;
	sum_rtt = 0;

	if (argc != 3) {
       cout<<"usage: "<<argv[0]<<" <hostname> <isheader>, isheader = {0,1}"<<endl;
       return -1;
    }
    char *hostname = argv[1];
    int isHeader = atoi(argv[2]);
    if(isHeader == 0)
    {
    	ping_ICMP(hostname);
    }
    else if(isHeader == 1)
    {
    	ping_header(hostname);
    }
    else
    {
    	cout<<"Invalid isHeader value"<<endl;
    	return -1;
    }

}