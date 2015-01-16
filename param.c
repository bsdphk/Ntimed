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
 * Routines to deal with parameters
 *
 */

#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include "ntimed.h"

static TAILQ_HEAD(, param_tbl) param_tbl = TAILQ_HEAD_INITIALIZER(param_tbl);

void
Param_Register(struct param_tbl *pt)
{

	for (;pt->name != NULL; pt++)
		TAILQ_INSERT_TAIL(&param_tbl, pt, list);
}

static void
param_wrapline(struct ocx *ocx, const char *b)
{
	int n = 0;
	const char *e, *w;
	const int tabs = 8;
	const int wrap_at = 64;

	AN(b);
	Put(ocx, OCX_DIAG, "\t");
	e = strchr(b, '\0');
	while (b < e) {
		if (!isspace((int)*b)) {
			Put(ocx, OCX_DIAG, "%c", *b);
			b++;
			n++;
		} else if (*b == '\t') {
			do {
				Put(ocx, OCX_DIAG, " ");
				n++;
			} while ((n % tabs) != 1);
			b++;
		} else if (*b == '\n') {
			Put(ocx, OCX_DIAG, "\n");
			param_wrapline(ocx, b + 1);
			return;
		} else {
			assert (*b == ' ');
			for (w = b + 1; w < e; w++)
				if (isspace((int)*w))
					break;
			if (n + (w - b) < wrap_at) {
				Put(ocx, OCX_DIAG, "%.*s", (int)(w - b), b);
				n += (w - b);
				b = w;
			} else {
				Put(ocx, OCX_DIAG, "\n");
				param_wrapline(ocx, b + 1);
				return;
			}
		}
	}
}

void
Param_Tweak(struct ocx *ocx, const char *arg)
{
	struct param_tbl *pt;
	const char *q;
	char *r;
	double d;
	size_t l;

	if (!strcmp(arg, "?")) {
		Put(ocx, OCX_DIAG, "List of available parameters:\n");
		TAILQ_FOREACH(pt, &param_tbl, list)
			Put(ocx, OCX_DIAG, "\t%s\n", pt->name);
		Fail(ocx, 0, "Stopping after parameter query.\n");
	}

	q = strchr(arg, '=');
	if (q == NULL) {
		TAILQ_FOREACH(pt, &param_tbl, list)
			if (!strcmp(pt->name, arg))
				break;
		if (pt == NULL)
			Fail(ocx, 0, "-p unknown parameter '%s' (try -p '?')",
			    arg);
		Put(ocx, OCX_DIAG, "Parameter:\n\t%s\n", pt->name);
		Put(ocx, OCX_DIAG, "Minimum:\n\t%.3e\n", pt->min);
		Put(ocx, OCX_DIAG, "Maximum:\n\t%.3e\n", pt->max);
		Put(ocx, OCX_DIAG, "Default:\n\t%.3e\n", pt->def);
		Put(ocx, OCX_DIAG, "Description:\n");
		param_wrapline(ocx, pt->doc);
		Put(ocx, OCX_DIAG, "\n\n");
		Fail(ocx, 0, "Stopping after parameter query.\n");
	}

	assert (q >= arg);
	l = (unsigned)(q - arg);

	TAILQ_FOREACH(pt, &param_tbl, list) {
		if (strlen(pt->name) != l)
			continue;
		if (!strncmp(pt->name, arg, l))
			break;
	}
	if (pt == NULL)
		Fail(ocx, 0, "-p unknown parameter '%.*s' (try -p '?')",
		    (int)(q - arg), arg);

	r = NULL;
	d = strtod(q + 1, &r);
	if (*r != '\0')
		Fail(ocx, 0, "-p '%.*s' bad value '%s'\n",
		    (int)(q - arg), arg, q + 1);
	if (d < pt->min)
		Fail(ocx, 0, "-p '%.*s' below min value (%g)\n",
		    (int)(q - arg), arg, pt->min);
	if (d > pt->max)
		Fail(ocx, 0, "-p '%.*s' above max value (%g)\n",
		    (int)(q - arg), arg, pt->max);
	Put(ocx, OCX_DIAG, "# Tweak(%s -> %.3e)\n", arg, d);
	*(pt->val) = d;
	return;
}

void
Param_Report(struct ocx *ocx, enum ocx_chan chan)
{
	struct param_tbl *pt;

	TAILQ_FOREACH(pt, &param_tbl, list)
		Put(ocx, chan, "# param %s %g # min %g, max %g, default %g\n",
		    pt->name, *pt->val, pt->min, pt->max, pt->def);
}
