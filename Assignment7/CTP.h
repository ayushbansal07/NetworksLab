#ifndef CTP_H
#define CTP_H

#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <queue>
#include <pthread.h>
#include <semaphore.h>

#define BUFF_SIZE 1024
#define SEND_BUFF_CAP 1024000
#define MSS 1024
#define MAX_SEQ_NO 1000000000
#define RECV_BUFFER_CAP 1024000

using namespace std;

struct segment{
	bool ack;
	long int ackNo;
	long int seqNo;
	int len;
	long int recv_window;
	char data[BUFF_SIZE];
};

class CTP{
private:
	int sockfd;
	struct sockaddr_in serveraddr;
	socklen_t serverlen;
	queue<char> sending_buffer;
	long int cong_wind;
	long int ssthresh;
	long int recv_wind;
	int dupAcks;
	long int currptr;
	long int baseptr;
	long int seq;
    queue<segment> sendQ;
    queue<segment> resendQ;
    long int last_ack;
    int recv_state;
    queue<char> recv_buffer;
    long int seq_recv;
    pthread_t RATE_CONTROLLER, RECEIVER_PROCESS;
    sem_t sQ,rQ,srsQ;

public:
	CTP(int sockfd, struct sockaddr_in serveraddr);
	void appSend(char *, long int);
	int appRecieve(char *,long int);

private:
	void sendbuffer_handle(char *, long int);
	static void * rate_control_helper(void *);
	void * rate_control();
	segment create_packet(long int *);
	static void * receiver_process_helper(void *);
	void * receiver_process();
	void resend_data_func();
	//int parse_packets();
	void update_window(long int,long int);
	void recvbuffer_handle(segment);
	void send_ack(long int);
};
#endif