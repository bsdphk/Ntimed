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
 * A peer-set is the total set of NTP servers we keep track of.
 *
 * The set is composed of groups, each of which is all the IP# we
 * get from resolving a single argument.
 *
 * Within each group we poll a maximum of one ${param} servers,
 * picking the best.
 *
 * Across the set we keep an hairy eyeball for servers appearing
 * more than once, because it would deceive our quorum.  Spotting
 * the same IP# is trivial, multihomed servers can be spotted on
 * {stratum,refid,reftime} triplet.
 *
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/socket.h>

#include "ntimed.h"
#include "ntp.h"

struct ntp_group {
	unsigned			magic;
#define NTP_GROUP_MAGIC			0xdd5f58de
	TAILQ_ENTRY(ntp_group)		list;

	char				*hostname;
	int				npeer;
};

struct ntp_peerset {
	unsigned			magic;
#define NTP_PEERSET_MAGIC		0x0bf873d0

	TAILQ_HEAD(,ntp_peer)		head;
	int				npeer;

	TAILQ_HEAD(,ntp_group)		group;
	int				ngroup;

	struct udp_socket		*usc;
	double				t0;
	double				init_duration;
	double				poll_period;
	double				init_packets;
};

/**********************************************************************/

struct ntp_peerset *
NTP_PeerSet_New(struct ocx *ocx)
{
	struct ntp_peerset *nps;

	(void)ocx;
	ALLOC_OBJ(nps, NTP_PEERSET_MAGIC);
	AN(nps);

	TAILQ_INIT(&nps->head);
	TAILQ_INIT(&nps->group);
	return (nps);
}

/**********************************************************************
 * Iterator helpers
 */

struct ntp_peer *
NTP_PeerSet_Iter0(const struct ntp_peerset *nps)
{

	CHECK_OBJ_NOTNULL(nps, NTP_PEERSET_MAGIC);
	return (TAILQ_FIRST(&nps->head));
}

struct ntp_peer *
NTP_PeerSet_IterN(const struct ntp_peerset *nps, const struct ntp_peer *np)
{

	CHECK_OBJ_NOTNULL(nps, NTP_PEERSET_MAGIC);
	CHECK_OBJ_NOTNULL(np, NTP_PEER_MAGIC);
	return (TAILQ_NEXT(np, list));
}

/**********************************************************************/

static int
ntp_peerset_fillgroup(struct ocx *ocx, struct ntp_peerset *nps,
    struct ntp_group *ng, const char *lookup)
{
	struct addrinfo hints, *res, *res0;
	int error, n = 0;
	struct ntp_peer *np, *np2;

	CHECK_OBJ_NOTNULL(nps, NTP_PEERSET_MAGIC);
	CHECK_OBJ_NOTNULL(ng, NTP_GROUP_MAGIC);

	memset(&hints, 0, sizeof hints);
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	error = getaddrinfo(lookup, "ntp", &hints, &res0);
	if (error)
		Fail(ocx, 1, "hostname '%s', port 'ntp': %s\n",
		    lookup, gai_strerror(error));
	for (res = res0; res; res = res->ai_next) {
		if (res->ai_family != AF_INET && res->ai_family != AF_INET6)
			continue;
		np = NTP_Peer_New(ng->hostname, res->ai_addr, res->ai_addrlen);
		AN(np);
		TAILQ_FOREACH(np2, &nps->head, list)
			if (SA_Equal(np->sa, np->sa_len, np2->sa, np2->sa_len))
				break;
		if (np2 != NULL) {
			/* All duplicates point to the same "master" */
			np->state = NTP_STATE_DUPLICATE;
			np->other = np2->other;
			if (np->other == NULL)
				np->other = np2;
			TAILQ_INSERT_TAIL(&nps->head, np, list);
			Debug(ocx, "Peer {%s %s} is duplicate of {%s %s}\n",
			    np->hostname, np->ip, np2->hostname, np2->ip);
		} else {
			np->state = NTP_STATE_NEW;
			TAILQ_INSERT_HEAD(&nps->head, np, list);
		}
		nps->npeer++;
		np->group = ng;
		ng->npeer++;
		n++;
	}
	freeaddrinfo(res0);
	return (n);
}

/**********************************************************************/

static struct ntp_group *
ntp_peerset_add_group(struct ntp_peerset *nps, const char *name)
{
	struct ntp_group *ng;

	ALLOC_OBJ(ng, NTP_GROUP_MAGIC);
	AN(ng);
	ng->hostname = strdup(name);
	AN(ng->hostname);
	TAILQ_INSERT_TAIL(&nps->group, ng, list);
	nps->ngroup++;
	return (ng);
}

/**********************************************************************
 * Add a peer with a specific hostname+ip combination without actually
 * resolving the hostname.
 */

void
NTP_PeerSet_AddSim(struct ocx *ocx, struct ntp_peerset *nps,
    const char *hostname, const char *ip)
{
	struct ntp_group *ng;

	CHECK_OBJ_NOTNULL(nps, NTP_PEERSET_MAGIC);
	TAILQ_FOREACH(ng, &nps->group, list)
		if (!strcasecmp(ng->hostname, hostname))
			break;
	if (ng == NULL)
		ng = ntp_peerset_add_group(nps, hostname);
	assert(ntp_peerset_fillgroup(ocx, nps, ng, ip) == 1);
}

/**********************************************************************
 * Create a new group and add whatever peers its hostname resolves to
 */

int
NTP_PeerSet_Add(struct ocx *ocx, struct ntp_peerset *nps, const char *hostname)
{
	struct ntp_group *ng;

	CHECK_OBJ_NOTNULL(nps, NTP_PEERSET_MAGIC);

	TAILQ_FOREACH(ng, &nps->group, list)
		if (!strcasecmp(ng->hostname, hostname))
			Fail(ocx, 0, "hostname %s is duplicated\n", hostname);

	ng = ntp_peerset_add_group(nps, hostname);

	if (ntp_peerset_fillgroup(ocx, nps, ng, hostname) == 0)
		Fail(ocx, 0, "hostname %s no IP# found.\n", hostname);

	return (ng->npeer);
}

/**********************************************************************
 * This function is responsible for polling the peers in the set.
 */

static enum todo_e __match_proto__(todo_f)
ntp_peerset_poll(struct ocx *ocx, struct todolist *tdl, void *priv)
{
	struct ntp_peerset *nps;
	struct ntp_peer *np;
	double d, dt;

	(void)ocx;
	CAST_OBJ_NOTNULL(nps, priv, NTP_PEERSET_MAGIC);
	AN(tdl);

	np = TAILQ_FIRST(&nps->head);
	if (np == NULL)
		return(TODO_DONE);

	CHECK_OBJ_NOTNULL(np, NTP_PEER_MAGIC);
	TAILQ_REMOVE(&nps->head, np, list);
	TAILQ_INSERT_TAIL(&nps->head, np, list);

	d = nps->poll_period / nps->npeer;
	if (nps->t0 < nps->init_duration) {
		dt = exp(
		    log(nps->init_duration) / (nps->init_packets * nps->npeer));
		if (nps->t0 * dt < nps->init_duration)
			d = nps->t0 * dt - nps->t0;
	}
	nps->t0 += d;
	TODO_ScheduleRel(tdl, ntp_peerset_poll, nps, d, 0.0, "NTP_PeerSet");
	if (NTP_Peer_Poll(ocx, nps->usc, np, 0.8)) {
		if (np->filter_func != NULL)
			np->filter_func(ocx, np);
	}

	return (TODO_OK);
}

/**********************************************************************
 * This function will be responsible for maintaining the peers in the set
 *
 * XXX: Pick only the best N (3?) servers from any hostname as active.
 * XXX: Re-lookup hostnames on periodic basis to keep the set current
 * XXX: Implement (a future) pool.ntp.org load-balancing protocol
 */

static enum todo_e
ntp_peerset_herd(struct ocx *ocx, struct todolist *tdl, void *priv)
{
	(void)ocx;
	(void)tdl;
	(void)priv;
	return (TODO_DONE);
}

/**********************************************************************/

static uintptr_t poll_hdl;
static uintptr_t herd_hdl;

void
NTP_PeerSet_Poll(struct ocx *ocx, struct ntp_peerset *nps,
    struct udp_socket *usc,
    struct todolist *tdl)
{
	struct ntp_peer *np;

	(void)ocx;
	CHECK_OBJ_NOTNULL(nps, NTP_PEERSET_MAGIC);
	AN(usc);
	AN(tdl);

	TAILQ_FOREACH(np, &nps->head, list)
		np->state = NTP_STATE_NEW;
	nps->usc = usc;
	nps->t0 = 1.0;
	nps->init_duration = 64.;
	nps->init_packets = 6.;
	nps->poll_period = 64.;

	if (poll_hdl != 0)
		TODO_Cancel(tdl, &poll_hdl);
	poll_hdl = TODO_ScheduleRel(tdl, ntp_peerset_poll, nps, 0.0, 0.0,
		"NTP_PeerSet Poll");

	if (herd_hdl != 0)
		TODO_Cancel(tdl, &herd_hdl);
	herd_hdl = TODO_ScheduleRel(tdl, ntp_peerset_herd, nps,
	    15. * 60. / nps->ngroup, 0.0, "NTP_PeerSet Herd");

}
