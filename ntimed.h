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
 * Main include file
 * =================
 */

#ifdef NTIMED_H_INCLUDED
#error "ntimed.h included multiple times"
#endif
#define NTIMED_H_INCLUDED

#include <stdint.h>
#include <unistd.h>
#include "ntimed_queue.h"
#include "ntimed_tricks.h"

struct todolist;
struct udp_socket;

/* ocx_*.c -- Operational Context *************************************/

struct ocx;	// private

enum ocx_chan {
	OCX_DIAG,		// think: stderr
	OCX_TRACE,		// think: /var/run/stats
	OCX_DEBUG,		// think: stdout
};

void Put(struct ocx *, enum ocx_chan, const char *, ...)
    __printflike(3, 4);
void PutHex(struct ocx *, enum ocx_chan, const void *, ssize_t len);

/*
 * Report "Failure: " + args + "\n" [+ errno-line] + "\n".  exit(1);
 */
void Fail(struct ocx *, int err, const char *, ...) \
    __attribute__((__noreturn__))
    __printflike(3, 4);

#define Debug(ocx, ...)		Put(ocx, OCX_DEBUG, __VA_ARGS__)
#define DebugHex(ocx, ptr, len)	PutHex(ocx, OCX_DEBUG, ptr, len)

void ArgTracefile(const char *fn);

/* param.c -- Parameters **********************************************/

struct param_tbl {
	const char		*name;
	double			*val;
	double			min;
	double			max;
	double			def;
	const char		*doc;
	TAILQ_ENTRY(param_tbl)	list;
};

void Param_Register(struct param_tbl *pt);
void Param_Tweak(struct ocx *, const char *arg);
void Param_Report(struct ocx *ocx, enum ocx_chan);

/* pll_std.c -- Standard PLL ******************************************/

typedef void pll_f(struct ocx *ocx, double offset, double weight);
extern pll_f *PLL;

void PLL_Init(void);

/* suckaddr.c -- Sockaddr utils ***************************************/

int SA_Equal(const void *sa1, size_t sl1, const void *sa2, size_t sl2);

/* time_sim.c -- Simulated timebase ***********************************/

extern double Time_Sim_delta;
void Time_Sim(struct todolist *);
void Time_Sim_Bump(struct todolist *, double when, double freq, double phase);

/* time_unix.c -- UNIX timebase ***************************************/

void Time_Unix(struct todolist *);
void Time_Unix_Passive(void);

/* time_stuff.c -- Timebase infrastructure ****************************/

struct timestamp {
	unsigned	magic;
#define TIMESTAMP_MAGIC	0x344cd213
	uint64_t	sec;		// Really:  time_t
	uint64_t	frac;
};

typedef int tb_sleep_f(double dur);
typedef struct timestamp *tb_now_f(struct timestamp *);
typedef void tb_step_f(struct ocx *, double offset);
typedef void tb_adjust_f(struct ocx *, double offset, double duration,
    double frequency);

extern int TB_generation;
extern tb_sleep_f *TB_Sleep;
extern tb_now_f *TB_Now;
extern tb_step_f *TB_Step;
extern tb_adjust_f *TB_Adjust;

void TS_Add(struct timestamp *ts, double dt);
struct timestamp *TS_Nanosec(struct timestamp *storage,
    int64_t sec, int64_t nsec);

struct timestamp *TS_Double(struct timestamp *storage, double);
double TS_Diff(const struct timestamp *t1, const struct timestamp *t2);
int TS_SleepUntil(const struct timestamp *);
void TS_Format(char *buf, size_t len, const struct timestamp *ts);

void TS_RunTest(struct ocx *ocx);

/* todo.c -- todo-list scheduler **************************************/


enum todo_e {
	TODO_INTR	= -2,	// Signal received
	TODO_FAIL	= -1,	// Break out of TODO_Run()
	TODO_OK		=  0,
	TODO_DONE	=  1,	// Stop repeating me
};

typedef enum todo_e todo_f(struct ocx *, struct todolist *, void *priv);

struct todolist *TODO_NewList(void);

uintptr_t TODO_ScheduleRel(struct todolist *,
    todo_f *func, void *priv,
    double when, double repeat,
    const char *fmt, ...) __printflike(6, 7);
uintptr_t TODO_ScheduleAbs(struct todolist *,
    todo_f *func, void *priv,
    const struct timestamp *when, double repeat,
    const char *fmt, ...) __printflike(6, 7);
enum todo_e TODO_Run(struct ocx *ocx, struct todolist *);
void TODO_Cancel(struct todolist *tdl, uintptr_t *);

/* combine_delta.c -- Source Combiner based on delta-pdfs *************/

struct combiner;

typedef void combine_f(struct ocx *, const struct combiner *,
    double trust, double low, double mid, double high);

struct combiner {
	unsigned	magic;
#define COMBINER_MAGIC	0xab2b239c

	combine_f	*func;
	void		*priv;
	const char	*name1;
	const char	*name2;
};

struct combine_delta *CD_New(void);
struct combiner *CD_AddSource(struct combine_delta *,
    const char *name1, const char *name2);

/**********************************************************************
 * Main functions
 */

int main_client(int argc, char *const *argv);
int main_poll_server(int argc, char *const *argv);
int main_sim_client(int argc, char *const *argv);
