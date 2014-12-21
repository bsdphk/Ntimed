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
 * Instantiate a set of parameters
 * ===============================
 *
 * The #including file must #define the desired parameters from param_tbl.h
 * into existence (PARAM_INSTANCE), and #define the desired name
 * of the local parameter table (PARAM_TABLE_NAME), then include this
 * file.
 */

#define PARAM_INSTANCE(nam, vmin, vmax, vdef, docs)		\
	static double param_##nam = vdef;
#include "param_tbl.h"
#undef PARAM_INSTANCE

/*lint -save -e785 */
static struct param_tbl PARAM_TABLE_NAME[] = {
#define PARAM_INSTANCE(nam, vmin, vmax, vdef, docs)		\
	{ .name = #nam, .val = &param_##nam,			\
	  .min = vmin, .max = vmax, .def = vdef, .doc = docs },
#include "param_tbl.h"
#undef PARAM_INSTANCE
	{ .name = 0 }
};
/*lint -restore */
