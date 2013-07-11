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
	CAN_ISOBUS_ERR_FILTER,	/* set filter for error frames       */
	CAN_ISOBUS_LOOPBACK,	/* local loopback (default:on)       */
	CAN_ISOBUS_RECV_OWN_MSGS,	/* receive my own msgs (default:off) */
	CAN_ISOBUS_FD_FRAMES,	/* allow CAN FD frames (default:off) */
};

/* Unique identifier on an ISOBUS network */
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
	/* CAN interface */
	int ifindex;
	/* Priority */
	__u8 pri, pri_mask : 3;
	/* PGN */
	pgn_t pgn, pgn_mask;
	/* Directed address, not meaningful with some PGNs */
	__u8 daddr, daddr_mask;
	/* Source address */
	__u8 saddr, saddr_mask;
	/* Flag to invert this filter (excluding interface */
	unsigned int inverted : 1;
};

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
#define CAN_ISOBUS_NULL_ADDR	254
#define CAN_ISOBUS_GLOBAL_ADDR	255
#define CAN_ISOBUS_MIN_ADDR	127
#define CAN_ISOBUS_MAX_ADDR	247
#define CAN_ISOBUS_ADDR_CLAIM_TIMEOUT	250000000LU
#define ISOBUS_PGN_REQUEST	59904LU
#define ISOBUS_PGN_ADDR_CLAIMED	60928LU
#define ISOBUS_PGN_COMMANDED_ADDR	65240LU
/* Ancillary data */
enum {
	CAN_ISOBUS_SADDR,
	CAN_ISOBUS_DADDR
};

#endif

