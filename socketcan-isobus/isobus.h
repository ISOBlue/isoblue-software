/*
 * linux/can/isobus.h
 *
 * Definitions for ISOBUS CAN sockets
 *
 * Authors: Alex Layton <awlayton@purdue.edu>
 *          Urs Thuermann   <urs.thuermann@volkswagen.de>
 *          Oliver Hartkopp <oliver.hartkopp@volkswagen.de>
 * Copyright (c) 2002-2007 Volkswagen Group Electronic Research
 * All rights reserved.
 *
 */

#ifndef CAN_ISOBUS_H
#define CAN_ISOBUS_H

#include "patched/can.h" /* #include <linux/can.h> */

#define SOL_CAN_ISOBUS (SOL_CAN_BASE + CAN_ISOBUS)

/* for socket options affecting the socket (not the global system) */

enum {
	CAN_ISOBUS_FILTER = 1,	/* set 0 .. n can_filter(s)          */
	CAN_ISOBUS_LOOPBACK,	/* local loopback (default:on)       */
	CAN_ISOBUS_RECV_OWN_MSGS,	/* receive my own msgs (default:off) */
	CAN_ISOBUS_SEND_PRIO,	/* ISOBUS send priority 0:hi-7:low (default:6) */
};

/* 
 * ISOBUS NAME
 *
 * bit 0-20	: Identity Number
 * bit 21-31	: Manufacturer Code
 * bit 32-34	: ECU Instance
 * bit 35-39	: Function Instance
 * bit 40-47	: Function
 * bit 48	: Reserved
 * bit 49-55	: Device Class
 * bit 56-59	: Device Class Instance
 * bit 60-62	: Industry Group
 * bit 63	: Self-Configurable Address
 */
typedef __u64 name_t;
#define CAN_ISOBUS_SC_MASK	0x8000000000000000LU
struct isobus_name {
	__u8 self_conf_addr : 1;
	__u8 industry_group : 3;
	__u8 class_instance : 4;
	__u8 device_class : 7;
	__u8 function : 8;
	__u8 function_instance : 5;
	__u8 ecu_instance : 3;
	__u16 manufacturer_code : 11;
	__u32 identity_number : 21;
};

typedef __u32 pgn_t;

/* Message Filtering */
struct isobus_filter {
	/* PGN */
	pgn_t pgn, pgn_mask;
	/* Directed address, not meaningful with some PGNs */
	__u8 daddr, daddr_mask;
	/* Source address */
	__u8 saddr, saddr_mask;
	/* Flag to invert this filter (excluding interface */
	int inverted : 1;
};
#define CAN_ISOBUS_PGN_MASK	0x03FFFFLU
#define CAN_ISOBUS_PGN1_MASK	0x03FF00LU
#define CAN_ISOBUS_ADDR_MASK	0xFFU

/* Transport Protocol */
#define ISOBUS_MAX_DLEN	1785
#define ISOBUS_MAX_PACKETS	255;
/* TODO: Figure out a good way to handle the wide range of message sizes */
struct isobus_mesg {
	pgn_t pgn;
	__u8 dlen;
	__u8 data[8] __attribute__((aligned(8)));
};

/* Network Management */
#define CAN_ISOBUS_NULL_ADDR	254U
#define CAN_ISOBUS_GLOBAL_ADDR	255U
#define CAN_ISOBUS_ANY_ADDR	CAN_ISOBUS_GLOBAL_ADDR
#define ISOBUS_PGN_REQUEST	59904LU
#define ISOBUS_PGN_ADDR_CLAIMED	60928LU
#define ISOBUS_PGN_COMMANDED_ADDR	65240LU
/* Ancillary data */
enum {
	CAN_ISOBUS_DADDR,
};

#endif

