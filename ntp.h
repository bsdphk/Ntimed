/*-
 * Copyright (c) 2014 Poul-Henning Kamp
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * NTP protocol stuff
 * ==================
 *
 */

struct ntp_peer;

#ifdef NTP_H_INCLUDED
#error "ntp.h included multiple times"
#endif
#define NTP_H_INCLUDED

enum ntp_mode {
#define NTP_MODE(n, l, u)	NTP_MODE_##u = n,
#include "ntp_tbl.h"
#undef NTP_MODE
};

enum ntp_leap {
#define NTP_LEAP(n, l, u)	NTP_LEAP_##u = n,
#include "ntp_tbl.h"
#undef NTP_LEAP
};

enum ntp_state {
#define NTP_STATE(n, l, u, d)	NTP_STATE_##u = n,
#include "ntp_tbl.h"
#undef NTP_STATE
};

/* ntp_packet.c -- [De]Serialisation **********************************/

struct ntp_packet {
	unsigned		magic;
#define NTP_PACKET_MAGIC	0x78b7f0be

	enum ntp_leap		ntp_leap;
	uint8_t			ntp_version;
	enum ntp_mode		ntp_mode;
	uint8_t			ntp_stratum;
	uint8_t			ntp_poll;
	int8_t			ntp_precision;
	struct timestamp	ntp_delay;
	struct timestamp	ntp_dispersion;
	uint8_t			ntp_refid[4];
	struct timestamp	ntp_reference;
	struct timestamp	ntp_origin;
	struct timestamp	ntp_receive;
	struct timestamp	ntp_transmit;

	struct timestamp	ts_rx;
};

struct ntp_packet *NTP_Packet_Unpack(struct ntp_packet *dst, void *ptr,
    ssize_t len);
size_t NTP_Packet_Pack(void *ptr, ssize_t len, struct ntp_packet *);

/* ntp_tools.c -- Handy tools *****************************************/

void NTP_Tool_Client_Req(struct ntp_packet *);
void NTP_Tool_Format(char *p, ssize_t len, const struct ntp_packet *pkt);
int NTP_Tool_Scan(struct ntp_packet *pkt, const char *buf);

/* ntp_filter.c -- NTP sanity checking ********************************/

typedef void ntp_filter_f(struct ocx *, const struct ntp_peer *);

void NF_New(struct ntp_peer *);
void NF_Init(void);

/* ntp_peer.c -- State management *************************************/


struct ntp_peer {
	unsigned			magic;
#define NTP_PEER_MAGIC			0xbf0740a0
	char				*hostname;
	char				*ip;
	struct sockaddr			*sa;
	unsigned			sa_len;
	struct ntp_packet		*tx_pkt;
	struct ntp_packet		*rx_pkt;

	ntp_filter_f			*filter_func;
	void				*filter_priv;

	struct combiner			*combiner;

	// For ntp_peerset.c
	TAILQ_ENTRY(ntp_peer)		list;
	struct ntp_group		*group;
	enum ntp_state			state;
	const struct ntp_peer		*other;
};

struct ntp_peer *NTP_Peer_New(const char *name, const void *, unsigned);
struct ntp_peer *NTP_Peer_NewLookup(struct ocx *ocx, const char *name);
void NTP_Peer_Destroy(struct ntp_peer *np);
int NTP_Peer_Poll(struct ocx *, const struct udp_socket *,
    const struct ntp_peer *, double tmo);

/* ntp_peerset.c -- Peer set management ****************************/

struct ntp_peerset *NTP_PeerSet_New(struct ocx *);
void NTP_PeerSet_AddSim(struct ocx *, struct ntp_peerset *,
    const char *hostname, const char *ip);
int NTP_PeerSet_Add(struct ocx *, struct ntp_peerset *, const char *hostname);
void NTP_PeerSet_Poll(struct ocx *, struct ntp_peerset *, struct udp_socket *,
    struct todolist *);

struct ntp_peer *NTP_PeerSet_Iter0(const struct ntp_peerset *);
struct ntp_peer *NTP_PeerSet_IterN(const struct ntp_peerset *,
    const struct ntp_peer *);

#define NTP_PeerSet_Foreach(var, nps) \
	for(var = NTP_PeerSet_Iter0(nps); \
	var != NULL; \
	var = NTP_PeerSet_IterN(nps, var))
