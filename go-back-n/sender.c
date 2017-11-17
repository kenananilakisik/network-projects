#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <time.h>
#define ACK 2
#define DATA 3 
#define INIT 1
typedef struct packet{
	char type;
	int seq_no;
	char data[1024];
	int data_length;
}Packet;

int main(int argc, char **argv)
{
	int fd;
	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 1;
    if ( (fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
        perror("socket failed");
        return 1;
    }
	setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	//--------------- Read file into memory(buffer)-------------------
	FILE *fileptr;
	char *buffer;
	long filelen;
	int windowsize = atoi(argv[2]); 

	if ((fileptr = fopen(argv[8], "rb")) == NULL){
		perror("can't open the file");
	}
	fseek(fileptr, 0, SEEK_END);
	filelen = ftell(fileptr);
	rewind(fileptr);

	buffer = (char *)malloc((filelen+1));
	fread(buffer, filelen, 1, fileptr);
	fclose(fileptr);
	//------------------------- Build Client Addr and Bind------------------
	struct sockaddr_in clientaddr;
    memset( &clientaddr, 0, sizeof(clientaddr));
    clientaddr.sin_family = AF_INET;
    clientaddr.sin_port = htons(15000);
    clientaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	if ( bind(fd, (struct sockaddr *)&clientaddr, sizeof(clientaddr)) < 0 ) {
        perror( "bind failed" );
        return 1;
    }
	//------------------------- Build Server Addr------------------
    char *hostname;
    struct sockaddr_in serveraddr;
    struct hostent *server;
    hostname = argv[6];
    server = gethostbyname(hostname);
    memset( &serveraddr, 0, sizeof(serveraddr) );
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(atoi(argv[4]));              
    serveraddr.sin_addr = *((struct in_addr *)server->h_addr);
	//--------------------------Send INIT / Monitor INITack------------------------
	Packet send;
	memset(&send, 0, sizeof(send));
	Packet receive;
	send.type = INIT;
	send.seq_no = 0;

	// Put filename, filelenght and windowsize(mode) into init packet
	memcpy(send.data,&windowsize, sizeof(windowsize));
	memcpy(send.data + sizeof(windowsize),&filelen, sizeof(filelen));
	memcpy(send.data + sizeof(windowsize) + sizeof(filelen),argv[8],sizeof(argv[8]));

	if (sendto( fd, &send, sizeof(Packet), 0, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0 ) {
		perror( "sendto failed" );
		return 1;
	}

	recvfrom( fd, &receive, sizeof(Packet), 0, NULL, 0 );

	if( receive.type == ACK ){ // If ACK for init received, then proceed
		memset(&receive, 0, sizeof(receive));
		printf("Started sending \"%s\" using window size of %d\n",argv[8], windowsize);
	}
	else{ // Connection Refused
		perror("INIT refused, either mode mismatch or server unavailable");
		return 0;
	}
	//---------------------------Send File---------------------------------------
	Packet packet;
	memset(&packet, 0, sizeof(Packet));
	packet.seq_no = 1;
	int i = 0;
	int MAX_PACKET_SIZE = 1024;
	int drop = 1;
	int expected_seqno = 0;
	int timeout = 2*CLOCKS_PER_SEC;
	int last_ack = 0;
	int MAX_PACKET_NUMBER = (filelen/1024) + 1;//Calculates max packet number
	int read_bytes = 0;

    while (last_ack <= MAX_PACKET_NUMBER) {
		packet.type = DATA;
		//if((drop % 3) != 0){//Sends out of oreder packets to test
		while(windowsize > 0 && i <= MAX_PACKET_NUMBER - 1){

			if (i == MAX_PACKET_NUMBER -1)// If at the last part of the file instead of reading MAX_PACKET_SIZE read what is left
				read_bytes = filelen - (MAX_PACKET_SIZE*i);
			else
				read_bytes = MAX_PACKET_SIZE;// Reads MAX_PACKET_SIZE

			memcpy(packet.data,buffer+(i*MAX_PACKET_SIZE),read_bytes);// Copy the data from file into packet
			packet.data_length = read_bytes;


			if (sendto( fd, &packet, sizeof(Packet), 0, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0 ) {
				perror( "sendto failed" );
				break;
			}
			//printf("Sending packet with seqno: %d\n", packet.seq_no);
			i++;// Increment i so that next portion of the file can be read
			packet.seq_no++;
			memset(packet.data, 0, sizeof(packet.data));
			packet.data_length = 0;
			windowsize--; // to send next paket in the window
		}
		windowsize = atoi(argv[2]);// reset window size to original value
		printf("\n");
		//}
		int m = windowsize;
		int no_ack = 0;
		//Set time out = windowsize * 1ms
		while(m > 0){
			if(recvfrom( fd, &receive, sizeof(Packet), 0, NULL, 0 ) < 0){
				no_ack = 1; // ACK received
			}
			else{ // No acks
				packet.seq_no = receive.seq_no;
				last_ack = receive.seq_no;
			}
			m--;	
		}
		
		if(no_ack == 0){ //If ack received set i such that window is filled with expected seqnumber + windowsize
			i = receive.seq_no - 1;
		}
		
		else{// else set "i" to last ack and resend.
			i = last_ack - 1;
		}
		memset(&receive, 0, sizeof(receive));
		//}
		//else
			//packet.seq_no++;
		//drop++;
        
    }
	//------------------------------------------------------------------
	printf("Sending complete!\n");
    close( fd );
	return 0;
}
