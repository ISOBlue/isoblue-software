/*
 * linux/can/pdu.h
 *
 * Definitions for pdu CAN sockets
 *
 * Authors: Oliver Hartkopp <oliver.hartkopp@volkswagen.de>
 *          Urs Thuermann   <urs.thuermann@volkswagen.de>
 * Copyright (c) 2002-2007 Volkswagen Group Electronic Research
 * All rights reserved.
 *
 */

#ifndef CAN_PDU_H
#define CAN_PDU_H

#include "patched/can.h" /* #include <linux/can.h> */

#define SOL_CAN_PDU (SOL_CAN_BASE + CAN_PDU)

/* for socket options affecting the socket (not the global system) */

enum {
	CAN_PDU_FILTER = 1,	/* set 0 .. n can_filter(s)          */
	CAN_PDU_ERR_FILTER,	/* set filter for error frames       */
	CAN_PDU_LOOPBACK,	/* local loopback (default:on)       */
	CAN_PDU_RECV_OWN_MSGS,	/* receive my own msgs (default:off) */
	CAN_PDU_FD_FRAMES,	/* allow CAN FD frames (default:off) */
};

struct pdu {
	__u8	priority : 3;
	__u8	extended_data_page : 1;
	__u8	data_page : 1;
	__u8	format;
	__u8	specific;
	__u8	source_address;
	__u8	data_len;
	__u8    data[CAN_MAX_DLEN] __attribute__((aligned(8)));
};

#endif
