#include "CTP.h"
#include <iostream>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
using namespace std;

void sig_wake(int sigid){}

CTP::CTP(int sockfd, struct sockaddr_in serveraddr)
{
	signal(SIGUSR1,sig_wake);
	this->sockfd = sockfd;
	this->serveraddr = serveraddr;
	this->serverlen = sizeof(serveraddr);
	this->cong_wind = MSS;
	this->sshthresh = SEND_BUFF_CAP;
	this->dupAcks = 0;
	this->currptr = -1;
	this->baseptr = -1;
	this->seq = 0;
}

int CTP::appSend(char * data,long int size)
{
	sendbuffer_handle(data, size);
}

void CTP::sendbuffer_handle(char * data,long int size)
{
	long int ct = 0;
	while(1)
	{
		while(this->sending_buffer.size() < SEND_BUFF_CAP && ct < size)
		{
			this->sending_buffer.push(data[ct]);
			ct++;
		}
		//TODO: SIGNAL CONSUMER THAT DATA IS PUT INSIDE SENDING_BUFFER
		if(ct==size) break;
		usleep(1000);
	}
}

void * CTP::rate_control(void * threadid)
{
    while(1)
    {
    	long int curr_wind = min(this->cong_wind,this->recv_wind);
    	while(baseptr - currptr < curr_wind && !this->sending_buffer.empty())
    	{
            if(!this->resendQ.empty())
            {
                segment temp_seg = this->resendQ.front();
                this->resendQ.pop();
                this->sendQ.push(temp_seg);
                cout<<"Resending pkt = "<<temp_seg.seqNo<<endl;
                int n = sendto(this->sockfd, &temp_seg, sizeof(temp_seg), 0, (struct sockaddr *) &(this->serveraddr), this->serverlen);
                if(n<0) cout<<"Error sending data to server, pkt no = "<<seq<<endl;
                else {
                    this->baseptr += temp_seg.len;
                }
            }
            else
            {
                segment to_send = create_packet(&(this->seq));
                this->sendQ.push(to_send);
                int n = sendto(this->sockfd, &to_send, sizeof(to_send), 0, (struct sockaddr *) &(this->serveraddr), this->serverlen);
                if(n<0) cout<<"Error sending data to server, pkt no = "<<(this->seq - to_send.len)<<endl;
                else {
                    this->baseptr += to_send.len;              
                }
            }
            
    	}
    	sleep(0);
    }
}



segment CTP::create_packet(long int *seqNo)
{
	segment temp;
	temp.ack = false;
	temp.seqNo = *seqNo;
	int ct = 0;
	bzero(temp.data, MSS);
	while(!this->sending_buffer.empty() && ct < MSS)
	{
		temp.data[ct] = this->sending_buffer.front();
		this->sending_buffer.pop();
		*seqNo = ((*seqNo) + 1)%MAX_SEQ_NO;
	}
	temp.len = ct;
	return temp;
}

int main()
{
	cout<<MAX_SEQ_NO<<endl;
	struct sockaddr_in serveraddr;
	CTP *my = new CTP(0,serveraddr);
	char *data = "lion";
	my->appSend(data,2);
}