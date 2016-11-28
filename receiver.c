#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <pthread.h>
#include "dcomm.h"

/* Delay to adjust speed of consuming buffer, in milliseconds */
#define DELAY 1000

MESGB rxbuf[MAXBUFF];
QTYPE rcvq = { 0, 0, 0, MAXBUFF, rxbuf };

QTYPE *recv_queue = &rcvq;
Byte sent_xonxoff = XON;

/* Socket */
int sockfd;
struct sockaddr_in remote_addr;
struct sockaddr_in my_addr;
socklen_t addrlen = sizeof(remote_addr);

/* Functions declaration */
void exit_message(char *s) {
	perror(s);
	exit(1);
}
void send_confirmation(char c, Byte msgno);
static Byte rcvchar(int sockfd, QTYPE *queue);
static MESGB *q_get(QTYPE *);
static void consume_data ();


int main(int argc, char* argv[]) {
	if (argc != 2) {
		perror("Wrong Argument to start the program\nStart with: reciever [port]");
		exit(EXIT_FAILURE);
	}

	/*	Creating new socket */
	int fd;
	fd = socket(AF_INET, SOCK_DGRAM, 0);

	struct ifreq ifr;
	ifr.ifr_addr.sa_family = AF_INET;
	strncpy(ifr.ifr_name, "wlp3s0", IFNAMSIZ-1);
	ioctl(fd, SIOCGIFADDR, &ifr);
	close(fd);


	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("cannot create socket\n");
        exit(EXIT_FAILURE);
    }
	memset((char *)&my_addr, 0, sizeof(my_addr));
	my_addr.sin_family = AF_INET;
    my_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	my_addr.sin_port = htons(atoi(argv[1]));


	remote_addr.sin_family = AF_INET;

	/* Binding socket */
	if (bind(sockfd, (struct sockaddr *)&my_addr, sizeof(my_addr)) < 0)
        exit_message("Error on binding:");

	printf("Binding pada %s:%s ...\n", inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr), argv[1]);

	/* Create child process */
	pthread_t consuming_thread;
	if ((pthread_create(&consuming_thread,NULL,(void *)&consume_data,NULL)) != 0)
		exit_message("Error creating thread : ");

	Byte c;
	while (1) {
		c = rcvchar(sockfd, recv_queue);
	}
	return 0;
}

/*	This function sends confimation c [ACK|NAK] to frame number msgno
*/
void send_confirmation(char c, Byte msgno) {
	Byte buf[4];

	buf[0] = c;
	buf[1] = msgno;
	buf[2] = checksum(buf, 2);
	buf[3] = 0;

#if defined(DEBUG_LOG)
		printf("    Sending ");
		if (c == ACK)
			printf("ACK ");
		else
			printf("NAK ");
		printf("on msgno %i\n", msgno);
#endif

	if (sendto(sockfd, &buf, 3, 0, (struct sockaddr *) &remote_addr, addrlen) == -1)
		exit_message("Error sending message confirm: ");

}

/*	This function handles receiving data from sockfd.
*/
static Byte rcvchar(int sockfd, QTYPE *queue) {
	Byte buf[MAXBUFF];
	memset(buf,'\0', MAXBUFF);
	int recvlen = recvfrom(sockfd, buf, 7, 0, (struct sockaddr *)&remote_addr, (socklen_t*) &addrlen);
	if (recvlen > 0) {
		//parse data
		MESGB* msg = frame_parse(buf);

		// IF failed to parse, send NAK
		if (msg == NULL) {
			send_confirmation(NAK, buf[1]);
			return '\0';
		} else {
			// Find offset from rear
			int curr_rear = recv_queue->data[recv_queue->rear].msgno;
			int offset = msg->msgno - curr_rear;

			#if defined(DEBUG_LOG)
				printf("Menerima byte ke-%i - %c\n", msg->msgno, msg->data);
				printf("//////////curr_rear %i | recieved %i | offset %i\n", curr_rear, msg->msgno, offset);
			#endif

			if (offset == 1) {
				// Add to queue, normally
				q_add(recv_queue, *msg);
				// Update rear & count
				q_update_count(recv_queue);
				// Send ACK with msgno according on queue rear
				Byte msgno = recv_queue->data[recv_queue->rear].msgno;
				send_confirmation(ACK, msgno);
			} else {
				// Add to queue w/ offset
				q_addn(recv_queue, *msg, offset);
			}
			#if defined(DEBUG_LOG)
				q_print(recv_queue);
			#endif
			Byte c = msg->data;
			if (queue->count >= MAXLIMIT) {
				//send XOFF
				char msg = XOFF;
				if (sendto(sockfd, &msg, 1, 0, (struct sockaddr *) &remote_addr, addrlen) == -1)
			        exit_message("Error sending message recv: ");

					if (sent_xonxoff == XON) {
					sent_xonxoff = XOFF;
					printf("Buffer > minimum upperlimit\n");
					printf("Mengirim XOFF\n");
				}
			}

			return c;
		}
	}
}

/*	This function consume buffer data.
*/
static void consume_data () {
	int counter = 0;
	MESGB* msg_consumed;

	while (1) {
		/* Call q_get */
		msg_consumed = (q_get(recv_queue));

		if (msg_consumed != NULL) {
			if ( (msg_consumed->data == CR || msg_consumed->data == LF || msg_consumed->data >= 32)) {
				printf("\n++++Mengkonsumsi byte ke-%d: '%c'\n", msg_consumed->msgno, msg_consumed->data);
			}
			if (msg_consumed->data == ENDFILE) {
				printf("Get EOF\n");
				exit(0);
			}
		}
		/* Can introduce some delay here. */
		usleep(DELAY * 1000);
	}
}

/*  q_get returns a pointer to the buffer where data is read or NULL if
	buffer is empty.
	q_get also sends XON if the buffer count is less than MINLIMIT
*/
MESGB *q_get(QTYPE *queue){
	if (queue->count <= 0)
		return NULL;

	MESGB* c = q_rem(queue);

	if (queue->count < MINLIMIT) {
		//send XON
		char msg = XON;
		if (sendto(sockfd, &msg, 1, 0, (struct sockaddr *) &remote_addr, addrlen) == -1)
	        exit_message("Error sending message q_get: ");
		if (sent_xonxoff == XOFF ) {
			sent_xonxoff = XON;
			printf("Buffer < maximum lowerlimit\n");
			printf("Mengirim XON\n");
		}
	}

	return c;
}
