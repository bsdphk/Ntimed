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
 * todo-list scheduler
 * ===================
 *
 * This is a simple "TODO-list" scheduler for calling things at certain
 * times.  Jobs can be one-shot or repeated and repeated jobs can abort.
 *
 * For ease of debugging, TODO jobs have a name.
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "ntimed.h"

struct todo {
	unsigned		magic;
#define TODO_MAGIC		0x5279009a
	TAILQ_ENTRY(todo)	list;

	todo_f			*func;
	void			*priv;
	struct timestamp	when;
	double			repeat;

	char			what[40];
};

struct todolist {
	unsigned		magic;
#define TODOLIST_MAGIC		0x7db66255
	TAILQ_HEAD(,todo)	todolist;
};

struct todolist *
TODO_NewList(void)
{
	struct todolist *tdl;

	ALLOC_OBJ(tdl, TODOLIST_MAGIC);
	AN(tdl);
	TAILQ_INIT(&tdl->todolist);
	return (tdl);
}

static void
todo_insert(struct todolist *tdl, struct todo *tp)
{
	struct todo *tp1;

	TAILQ_FOREACH(tp1, &tdl->todolist, list) {
		if (TS_Diff(&tp1->when, &tp->when) > 0.0) {
			TAILQ_INSERT_BEFORE(tp1, tp, list);
			return;
		}
	}
	TAILQ_INSERT_TAIL(&tdl->todolist, tp, list);
}

/**********************************************************************
 */

void
TODO_Cancel(struct todolist *tdl, uintptr_t *tp)
{
	struct todo *tp2;

	CHECK_OBJ_NOTNULL(tdl, TODOLIST_MAGIC);
	AN(tp);
	AN(*tp);

	TAILQ_FOREACH(tp2, &tdl->todolist, list)
		if ((uintptr_t)tp2 == *tp)
			break;
	CHECK_OBJ_NOTNULL(tp2, TODO_MAGIC);
	TAILQ_REMOVE(&tdl->todolist, tp2, list);
	FREE_OBJ(tp2);
	*tp = 0;
}

/**********************************************************************
 */

uintptr_t
TODO_ScheduleAbs(struct todolist *tdl, todo_f *func, void *priv,
    const struct timestamp *when, double repeat, const char *fmt, ...)
{
	struct todo *tp;
	va_list ap;

	CHECK_OBJ_NOTNULL(tdl, TODOLIST_MAGIC);
	AN(func);
	CHECK_OBJ_NOTNULL(when, TIMESTAMP_MAGIC);
	assert(repeat >= 0.0);
	AN(fmt);

	ALLOC_OBJ(tp, TODO_MAGIC);
	AN(tp);
	tp->func = func;
	tp->priv = priv;
	tp->when = *when;
	tp->repeat = repeat;
	va_start(ap, fmt);
	(void)vsnprintf(tp->what, sizeof tp->what, fmt, ap);
	va_end(ap);
	todo_insert(tdl, tp);
	return ((uintptr_t)tp);
}

uintptr_t
TODO_ScheduleRel(struct todolist *tdl, todo_f *func, void *priv,
    double when, double repeat, const char *fmt, ...)
{
	struct todo *tp;
	va_list ap;

	CHECK_OBJ_NOTNULL(tdl, TODOLIST_MAGIC);
	AN(func);
	assert(when >= 0.0);
	assert(repeat >= 0.0);
	AN(fmt);

	ALLOC_OBJ(tp, TODO_MAGIC);
	AN(tp);
	tp->func = func;
	tp->priv = priv;
	TB_Now(&tp->when);
	TS_Add(&tp->when, when);
	tp->repeat = repeat;
	va_start(ap, fmt);
	(void)vsnprintf(tp->what, sizeof tp->what, fmt, ap);
	va_end(ap);
	todo_insert(tdl, tp);
	return ((uintptr_t)tp);
}

/**********************************************************************
 * Schedule TODO list until failure or empty
 */

enum todo_e
TODO_Run(struct ocx *ocx, struct todolist *tdl)
{
	struct todo *tp;
	enum todo_e ret = TODO_OK;
	char buf[40];
	int i;

	CHECK_OBJ_NOTNULL(tdl, TODOLIST_MAGIC);
	while(!TAILQ_EMPTY(&tdl->todolist)) {
		tp = TAILQ_FIRST(&tdl->todolist);
		i = TS_SleepUntil(&tp->when);
		if (i == 1)
			return (TODO_INTR);
		AZ(i);
		TS_Format(buf, sizeof buf, &tp->when);
		Put(ocx, OCX_TRACE, "Now %s %s\n", buf, tp->what);
		ret = tp->func(ocx, tdl, tp->priv);
		if (ret == TODO_FAIL)
			break;
		if (ret == TODO_DONE || tp->repeat == 0.0) {
			TAILQ_REMOVE(&tdl->todolist, tp, list);
			FREE_OBJ(tp);
		} else if (ret == TODO_OK) {
			TS_Add(&tp->when, tp->repeat);
			TAILQ_REMOVE(&tdl->todolist, tp, list);
			todo_insert(tdl, tp);
		} else {
			WRONG("Invalid Return from todo->func");
		}
	}
	return (ret);
}
