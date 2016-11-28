/*
* File : dcomm.h
*/
#ifndef DCOMM_H
#define DCOMM_H

/* Includes */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>

/* Debugging related Macros, uncomment when you want it to be active*/
// #define DEBUG_LOG
// #define ERR_SEND_TEST
// #define MSG_CORRUPT_TEST

/* ASCII Const */
#define SOH 1 /* Start of Header Character */
#define STX 2 /* Start of Text Character */
#define ETX 3 /* End of Text Character */
#define ENQ 5 /* Enquiry Character */
#define ACK 6 /* Acknowledgement */
#define BEL 7 /* Message Error Warning */
#define LF 10 /* Carriage Return */
#define CR 13 /* Line Read */
#define NAK 21 /* Negative Acknowledgement */
#define ENDFILE 26 /* End of File Character */
#define ESC 27 /* ESC Key */

/* XON/XOFF protocol */
#define XON (0x11)
#define XOFF (0x13)

/* Const */
#define BYTESIZE 256 /* The Maximum value of a bit */
#define MAXLEN 1024 /* Maximum Messages length */
#define TIMEOUT_SECS 2
#define TIMEOUT_MSECS 0
/* Define receive buffer size */
#define MAXBUFF 8
#define MAXLIMIT 4
#define MINLIMIT 2

typedef enum { false = 0, true } Boolean;

typedef unsigned char Byte;

typedef struct MESGB {
	unsigned int soh;
	unsigned int stx;
	unsigned int etx;
	Byte checksum;
	Byte msgno;
	Byte data;
} MESGB;

typedef struct QTYPE {
	unsigned int count;
	unsigned int front;
	unsigned int rear;
	unsigned int maxsize;
	MESGB* data;
} QTYPE;

/* QTYPE related functions */
void q_print(QTYPE *);
void q_update_count(QTYPE *);
void q_add(QTYPE *, MESGB );
void q_addn(QTYPE *, MESGB , int offset);
MESGB *q_rem(QTYPE *);
MESGB *q_peek(QTYPE *, int n);
int is_full(QTYPE );
int is_empty(QTYPE );

/* MESGB related functions */
void msg_cpy(MESGB , MESGB *);
MESGB* msg_create(char, int);
MESGB* frame_parse(Byte *);
Byte* frame_create(MESGB );

/* Checksum function */
Byte checksum(Byte* , Byte );

#endif
