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
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/socket.h>

#include "ntimed.h"
#include "udp.h"
#include "ntp.h"

struct ntp_peer *
NTP_Peer_New(const char *hostname, const void *sa, unsigned salen)
{
	struct ntp_peer *np;
	char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];

	AZ(getnameinfo(sa, salen, hbuf, sizeof(hbuf), sbuf,
	    sizeof(sbuf), NI_NUMERICHOST | NI_NUMERICSERV));

	ALLOC_OBJ(np, NTP_PEER_MAGIC);
	AN(np);

	np->sa_len = salen;
	np->sa = calloc(1, np->sa_len);
	AN(np->sa);
	memcpy(np->sa, sa, np->sa_len);

	np->hostname = strdup(hostname);
	AN(np->hostname);
	np->ip = strdup(hbuf);
	AN(np->ip);

	ALLOC_OBJ(np->tx_pkt, NTP_PACKET_MAGIC);
	AN(np->tx_pkt);

	ALLOC_OBJ(np->rx_pkt, NTP_PACKET_MAGIC);
	AN(np->rx_pkt);

	NTP_Tool_Client_Req(np->tx_pkt);

	return (np);
}

struct ntp_peer *
NTP_Peer_NewLookup(struct ocx *ocx, const char *hostname)
{
	struct addrinfo hints, *res0;
	int error;
	struct ntp_peer *np;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	error = getaddrinfo(hostname, "ntp", &hints, &res0);
	if (error)
		Fail(ocx, 0, "hostname '%s', port 'ntp': %s\n",
		    hostname, gai_strerror(error));

	np = NTP_Peer_New(hostname, res0->ai_addr, res0->ai_addrlen);
	freeaddrinfo(res0);
	return (np);
}

void
NTP_Peer_Destroy(struct ntp_peer *np)
{

	CHECK_OBJ_NOTNULL(np, NTP_PEER_MAGIC);
	free(np->sa);
	free(np->hostname);
	free(np->ip);
	free(np->tx_pkt);
	free(np->rx_pkt);
	FREE_OBJ(np);
}

int
NTP_Peer_Poll(struct ocx *ocx, const struct udp_socket *usc,
    const struct ntp_peer *np, double tmo)
{
	char buf[100];
	size_t len;
	struct sockaddr_storage rss;
	socklen_t rssl;
	ssize_t l;
	int i;
	struct timestamp t0, t1, t2;
	double d;

	AN(usc);
	CHECK_OBJ_NOTNULL(np, NTP_PEER_MAGIC);
	assert(tmo > 0.0 && tmo <= 1.0);

	len = NTP_Packet_Pack(buf, sizeof buf, np->tx_pkt);

	l = Udp_Send(ocx, usc, np->sa, np->sa_len, buf, len);
	if (l != (ssize_t)len) {
		Debug(ocx, "Tx peer %s %s got %zd (%s)\n",
		    np->hostname, np->ip, l, strerror(errno));
		return (0);
	}

	(void)TB_Now(&t0);

	while (1) {
		(void)TB_Now(&t1);
		d = TS_Diff(&t1, &t0);

		i = UdpTimedRx(ocx, usc, np->sa->sa_family, &rss, &rssl, &t2,
		    buf, sizeof buf, tmo - d);

		if (i == 0)
			return (0);

		if (i < 0)
			Fail(ocx, 1, "Rx failed\n");

		if (i != 48) {
			Debug(ocx, "Rx peer %s %s got len=%d\n",
			    np->hostname, np->ip, i);
			continue;
		}

		/* Ignore packets from other hosts */
		if (!SA_Equal(np->sa, np->sa_len, &rss, rssl))
			continue;

		AN(NTP_Packet_Unpack(np->rx_pkt, buf, i));
		np->rx_pkt->ts_rx = t2;

		/* Ignore packets which are not replies to our packet */
		if (TS_Diff(&np->tx_pkt->ntp_transmit,
		    &np->rx_pkt->ntp_origin) != 0.0) {
			continue;
		}

		return (1);
	}
}
