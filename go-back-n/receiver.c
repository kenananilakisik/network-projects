#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#define ACK 2
#define DATA 3 
#define INIT 1
typedef struct packet{
	int type;
	int seq_no;
	char data[1024];
	int data_length;
}Packet;


int main( int argc, char **argv)
{
	int windowsize = atoi(argv[2]); 
    int fd;
    if ( (fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
        perror( "socket failed" );
        return 1;
    }
	//------------------------- Build Server and Bind ----------------------
    struct sockaddr_in serveraddr;
    memset( &serveraddr, 0, sizeof(serveraddr) );
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(15002);
    serveraddr.sin_addr.s_addr = htonl( INADDR_ANY);
	if ( bind(fd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0 ) {
        perror( "bind failed" );
        return 1;
    }

    //------------------------- Build Client Addr------------------
	char *hostname;
	struct sockaddr_in clientaddr;
    struct hostent *client;
	hostname = argv[6];
    client = gethostbyname(hostname);
	memset( &clientaddr, 0, sizeof(clientaddr));
    clientaddr.sin_family = AF_INET;
    clientaddr.sin_port = htons(atoi(argv[4]));
    clientaddr.sin_addr = *((struct in_addr *)client->h_addr);   


	//--------- Receive INIT / Create File Accordingly / Send Ack for INIT----------
	Packet receive, ack;
	int seqno = 1;
	ack.type = ACK;
	long filesize;
	int MAX_FILENAME = 50;
    char pid_filename[100];
	char filename[MAX_FILENAME];
	long pid = getpid();
	long sz = 0;
	int sender_windowsize;
	int init_flag = 0;
	
	while(init_flag == 0){
		if(recvfrom( fd, &receive, sizeof(Packet), 0, NULL, 0 ) < 0){
			perror( "recvfrom failed" );
			return 0;
		}
		// Get filesize, filename and windowsize from init packet.
		memcpy(&sender_windowsize, receive.data, sizeof(int));
		memcpy(&filesize, receive.data + sizeof(int), sizeof(long));
		memcpy(filename, receive.data + sizeof(int) + sizeof(long), MAX_FILENAME);
		sprintf(pid_filename,"%ld%s",pid,filename);

		//Checks if receiver window size matches with sender's windows size
		if(receive.type == INIT && windowsize == sender_windowsize){
			printf("Filename: %s\nFilesize: %ld\nWindowsize: %d\nWriting...\n", filename, filesize, sender_windowsize);
			memset(&receive,0,sizeof(Packet));
			if (sendto( fd, &ack, sizeof(Packet), 0, (struct sockaddr *)&clientaddr, sizeof(clientaddr)) < 0 ) {
				perror( "sendto failed" );
			}
			init_flag = 1;
		}
	}
	
	// Create/append file in binary mode
	FILE *fp = fopen(pid_filename, "ab+");
	printf("Writing to %s\n",pid_filename);
	
    //---------------------------Write to File, Send Acks------------------------------
    while(1) {
		rewind(fp);
		// Receive data packets here
        if(recvfrom( fd, &receive, sizeof(Packet), 0, NULL, 0 ) < 0){
			perror( "recvfrom failed" );
			return 0;
		}
		//printf("\nreceived seq no: %d and expected seqno %d\n", receive.seq_no, seqno); 
		if (receive.type == DATA && seqno == receive.seq_no){// If received packet has the correct sequence number write it to file and send ack
			seqno++;
			ack.seq_no = seqno;
			if (sendto( fd, &ack, sizeof(Packet), 0, (struct sockaddr *)&clientaddr, sizeof(clientaddr)) < 0 ) {
				perror( "sendto failed" );
			}		
			fwrite(receive.data, 1, receive.data_length, fp);
			memset(&receive,0,sizeof(Packet));
			
		}
		// If sequence number is wrong send last correct sequence number with the ack
		else if(receive.type == DATA && seqno != receive.seq_no){
			ack.seq_no = seqno;
			if (sendto( fd, &ack, sizeof(Packet), 0, (struct sockaddr *)&clientaddr, sizeof(clientaddr)) < 0 ) {
				perror( "sendto failed" );
			}
			memset(&receive,0,sizeof(Packet));
		}
		else{
			//Stay IDLE
		}
		//End of File
		if (seqno -1  > filesize/1024)
			break;
		fseek(fp, 0L, SEEK_END);
		sz = ftell(fp);	
    }
    close( fd );
	return 0;
}
