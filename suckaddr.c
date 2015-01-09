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
 * The world has not enough cuss-words to precisely convey how broken the
 * the struct sockaddr concept is.
 */

#include <string.h>

#include <sys/types.h>		/* Compat for OpenBSD */
#include <sys/socket.h>

#include <netinet/in.h>

#include "ntimed.h"

int
SA_Equal(const void *sa1, size_t sl1, const void *sa2, size_t sl2)
{
	const struct sockaddr *s1, *s2;
	const struct sockaddr_in *s41, *s42;
	const struct sockaddr_in6 *s61, *s62;

	AN(sa1);
	AN(sa2);
	assert(sl1 >= sizeof(struct sockaddr));
	assert(sl2 >= sizeof(struct sockaddr));

	s1 = sa1;
	s2 = sa2;
	if (s1->sa_family != s2->sa_family)
		return (0);

	if (s1->sa_family == AF_INET) {
		assert(sl1 >= sizeof(struct sockaddr_in));
		assert(sl2 >= sizeof(struct sockaddr_in));
		s41 = sa1;
		s42 = sa2;
		if (s41->sin_port != s42->sin_port)
			return (0);
		if (memcmp(&s41->sin_addr, &s42->sin_addr,
		      sizeof s41->sin_addr))
			return (0);
		return (1);
	}

	if (s1->sa_family == AF_INET6) {
		assert(sl1 >= sizeof(struct sockaddr_in6));
		assert(sl2 >= sizeof(struct sockaddr_in6));
		s61 = sa1;
		s62 = sa2;
		if (s61->sin6_port != s62->sin6_port)
			return (0);
		if (s61->sin6_scope_id != s62->sin6_scope_id)
			return (0);
		if (memcmp(&s61->sin6_addr, &s62->sin6_addr,
		    sizeof s61->sin6_addr))
			return (0);
		return (1);
	}
	return (0);
}
