#include "dcomm.h"

/*  This function remove the first element of the queue then
	return the address of the removed value
	IF the queue is empty, returns NULL
 */
MESGB* q_rem(QTYPE *queue) {
	if (queue->count == 0) {
		return NULL;
	} else {

		MESGB* c = &(queue->data[queue->front]);

		queue->count--;
		queue->front++;

		if (queue->front >= MAXBUFF) {
			queue->front = 0;
		}

		return c;
	}
}

/*  This function add MESGB c to the queue rear,
	only if the queue is not full
 */
void q_add(QTYPE *queue, MESGB c) {
	if (queue->count < queue->maxsize) {
		queue->rear++;
		queue->count++;

		if (queue->rear >= queue->maxsize) {
			queue->rear = 0;
		}

		msg_cpy(c, &(queue->data[queue->rear]));

		if (queue->count == 1) {
			queue->front = queue->rear;
		}
	}
}

/*  This function put MESGB msg to the queue rear + offset,
	only if queue count + offset is less than queue maxsize
 */
void q_addn(QTYPE *queue, MESGB msg, int offset) {
	if (queue->count + offset < queue->maxsize) {
		int loc = queue->rear + offset;
		if (loc >= queue->maxsize)
			loc -= queue->maxsize;
		msg_cpy(msg, &(queue->data[loc]));
	}
}

/*  This function prints the content of the queue to stdout
 */
void q_print(QTYPE *queue) {
	int i;
	printf(";;Isi queue\n");
	for (i = 0; i < MAXBUFF; i++) {
		printf("  ;;No: %02d Data: %c\n", queue->data[i].msgno, queue->data[i].data);
	}
	printf("\n");
}

/*  This function update the queue count and rear
	this function is called when element after queue rear is sparse
 */
void q_update_count(QTYPE *queue) {
	MESGB* data = queue->data;
	int desired_msg = data[queue->rear].msgno;
	int new_count = queue->count;
	int new_rear = queue->rear;

	while (new_count < queue->maxsize) {
		if (data[new_rear + 1].msgno == desired_msg) {
			new_rear++;
			desired_msg++;
			new_count++;
			if (new_rear >= queue->maxsize)
				new_rear = 0;
		} else {
			break;
		}
	}
	queue->count = new_count;
	queue->rear = new_rear;
}

/*  This function returns the address of queue element
	where the element->msgno is equal to n
 */
MESGB *q_peek(QTYPE *queue, int n) {
	if (queue->count == 0)
		return NULL;

	int i = queue->front	;
	while (queue->data[i].msgno < n) {
		i++;
		if (i >= queue->maxsize) {
			i = 0;
		}
	}

	return &(queue->data[i]);
}

/*  This function checks if the queue is full
 */
int is_full(QTYPE queue) {
	return (queue.count >= queue.maxsize);
}

/*  This function checks if the queue is empty
 */
int is_empty(QTYPE queue) {
	return (queue.count == 0);
}

/*  This function create a new MESGB given the MESGB.data c and MESGB.msgno no
 */
MESGB* msg_create(char c, int no) {
	MESGB* msg = (MESGB *) malloc(sizeof(MESGB));
	msg->soh = SOH;
	msg->stx = STX;
	msg->etx = ETX;
	msg->data = c;
	msg->msgno = no;
	msg->checksum = checksum(&c, 1);

	return msg;
}

/*  This function copy the content of MESGB src to MESGB dest
 */
void msg_cpy(MESGB src, MESGB *dest) {
	dest->soh = src.soh;
	dest->stx = src.etx;
	dest->checksum = src.checksum;
	dest->msgno = src.msgno;
	dest->data = src.data;
}

/*  This function create a sending frame when the content is a MESGB msg
 */
Byte* frame_create(MESGB msg) {
	Byte* frame = malloc((6 + 1)*sizeof(Byte));
	frame[0] = SOH;
	frame[1] = msg.msgno;
	frame[2] = STX;
	frame[3] = msg.data;
	frame[4] = ETX;
	frame[5] = (Byte) msg.checksum;
	frame[6] = '\0';

	return frame;
}

/*  This function parse the buffer frame
	IF parsing is success, then return the address of the created MESGB
	IF parsing is failed, return NULL
 */
MESGB* frame_parse(Byte *frame) {
#if defined(MSG_CORRUPT_TEST)
		time_t t;
		srand((unsigned) time(&t));
		if (rand() % 50 > 35) {
			printf("!!!!!MSG_CORRUPT on no.%02i\n",frame[1]);
			return NULL;
		}
#endif

	if (frame[0] != SOH && frame[2] != STX && frame[4] != ETX)
		return NULL;

	Byte c = frame[3];
	Byte check = checksum(&c, 1);
	if (check != frame[5])
		return NULL;
	return msg_create(frame[3], frame[1]);
}

/*  This function calculate the 8-bit checksum from a certain buffer
 */
Byte checksum(Byte *buffer, Byte size) {
    unsigned long cksum=0;
    while(size >1) {
        cksum+=*buffer++;
        size -=sizeof(Byte);
    }

    if(size)
        cksum += *(Byte*)buffer;

    return (Byte)(~cksum);
}
