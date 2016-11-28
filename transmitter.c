#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include "dcomm.h"

#define DURATION 500

/* Global variables */
struct sockaddr_in remote_addr;
char currentchar;
char lastchar;
int sock_fd, i;
int stop;
unsigned int slen = sizeof(remote_addr);

MESGB sxbuf[MAXBUFF];
QTYPE sndq = { 0, 0, 0, MAXBUFF, sxbuf };
QTYPE *q_send = &sndq;

static void recvdata();
void handle_data(char *c, int no);
void snd(MESGB m);
void exit_message(char *s) {
	perror(s);
	exit(1);
}

int main(int argc, char *argv[]) {

	// Arguments checking
	if (argc != 4)
		exit_message("Wrong Argument to start the program\nStart with: test_send [ip_addr] [port] [file]");

    int port = atoi(argv[2]);

	// Creating socket
    if ( (sock_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
		exit_message("Error creating socket");

	// Set remote address
	memset((char *) &remote_addr, 0, sizeof(remote_addr));
    remote_addr.sin_family = AF_INET;
    remote_addr.sin_port = htons(port);
    if (inet_aton(argv[1] , &remote_addr.sin_addr) == 0)
		exit_message("Error inet_aton :");

	printf("Membuat socket untuk koneksi ke %s:%s ...\n", argv[1], argv[2]);

	FILE *file;
	if ((file = fopen(argv[3], "r")) == NULL)
		exit_message("Error opening file : ");

	// Creating new thread to receive data
	pthread_t recvthread;
	if ((pthread_create(&recvthread,NULL,(void *)&recvdata,NULL)) != 0)
		exit_message("Error creating thread : ");

	// Initialize the current character
	currentchar = XON;

	char c = 'a';
	int count = 1;
	stop = 0;
	int i = 0;
	while (!stop) {
		if (currentchar == XON && !is_full(*q_send)) {
			c = getc(file);
			if (c == EOF) {
				printf("EOF\n");
				c = ENDFILE;
				stop = 1;
			}
			char* message = &c;
			handle_data(message, count++);

		} else if (currentchar == XOFF){
			printf("Menunggu XON... %i\n", i++);
			usleep(DURATION * 1000 * 2);
		} else if (is_full(*q_send)) {

#if defined(DEBUG_LOG)
			printf("  Send buffer is full, waiting...\n");
#endif

			usleep(DURATION * 1000 * 2);
		}
		usleep(DURATION * 1000);
	}
	fclose(file);

	pthread_join(recvthread, NULL);

    close(sock_fd);
    return 0;
}

/*	This function add the data c and message number no to q_send,
	and send it through sock_fd
*/
void handle_data(char *c, int no) {
	MESGB* m = msg_create(*c, no);

	q_add(q_send, *m);
	snd(*m);

	free(m);
}

/*	This function send the converted MESGB through sock_fd
*/
void snd(MESGB m) {
	// Create frame
	Byte* frame = frame_create(m);

	// While XOFF wait, else send frame
	while (currentchar == XOFF) {

	}

#if defined(ERR_SEND_TEST)
	time_t t;
	srand((unsigned) time(&t));
	if (rand() % 50 < 35) {
#endif

		if (sendto(sock_fd, frame, strlen(frame), 0 , (struct sockaddr *) &remote_addr, slen) == -1)
			exit_message("Error on sendto");

			printf("    Sending: %i. %c - 0x%x\n", frame[1], frame[3], frame[5]);

#if defined(ERR_SEND_TEST)
	} else {
		printf("!!!!Blocked %i\n", frame[1]);
	}
#endif

	free(frame);
}

/*	This thread function handles receiving data from sockfd.
	IF received ACK, delete data from buffer
	IF received NAK, resend the related frame
	IF recieved XON|XOFF, toggle the currentchar
	IF timeout, resend the buffer head
*/
static void recvdata() {
	char buf[MAXBUFF];

	// Set timeout option
	struct timeval tv;
	tv.tv_sec = TIMEOUT_SECS;
	tv.tv_usec = TIMEOUT_MSECS;
	if (setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0) {
	    exit_message("Error on setsockopt");
	}

	while (!stop || !is_empty(*q_send)) {
		memset(buf,'\0', MAXBUFF);

		int recvlen = recvfrom(sock_fd, buf, MAXBUFF, 0, (struct sockaddr *) &remote_addr, &slen);

		// IF not Timeout
		if (recvlen > 0) {
			if (recvlen == 3) {
				// IF ACK n, remove data from queue until head is on msg number n + 1
				if (buf[0] == ACK) {
					printf("received ACK %i\n", buf[1]);

					Byte msgno = buf[1];
					while (q_send->data[q_send->front].msgno <= msgno && !is_empty(*q_send)) {
						q_rem(q_send);
					}

				// IF NAK n, resend data n from queue
				} else if (buf[0] == NAK) {
					printf("recieved NAK %i\n", buf[1]);
					Byte msgno = buf[1];
					MESGB* m = q_peek(q_send, msgno);
					snd(*m);
				}

			} else {
				if (currentchar != buf[0]) {
					if (buf[0] == XON)
						printf("XON");
					else
						printf("XOFF");
					printf(" diterima.\n");
				}
				currentchar = buf[0];
			}
		} else {
			if (!is_empty(*q_send)) {
				MESGB m = (q_send->data[q_send->front]);
				printf("Timeout.\nResending msgno %i\n", m.msgno);
				snd(m);
			}
		}
	}

	printf("Thread quit.\n");
}
