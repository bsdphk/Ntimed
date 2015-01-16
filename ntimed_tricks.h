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
 */

#ifdef NTIMED_TRICKS_H
#error "ntimed_tricks.h included multiple times"
#endif
#define NTIMED_TRICKS_H

#include <assert.h>

/**********************************************************************
 * Assert and friends
 *
 * We always runs with asserts enabled, and we assert liberally.
 *
 * Tests which are too expensive for production use should be wrapped
 * in "#ifdef DIAGNOSTICS"
 *
 */

#undef NDEBUG		// Asserts *always* enabled.

#define AZ(foo)		do { assert((foo) == 0); } while (0)
#define AN(foo)		do { assert((foo) != 0); } while (0)

#define WRONG(foo)				\
	do {					\
		/*lint -save -e506 */		\
		assert(0 == (uintptr_t)foo);	\
		/*lint -restore */		\
	} while (0)

/**********************************************************************
 * Safe printfs into compile-time fixed-size buffers.
 */

#define bprintf(buf, fmt, ...)						\
	do {								\
		assert(snprintf(buf, sizeof buf, fmt, __VA_ARGS__)	\
		    < (int)sizeof buf);					\
	} while (0)

#define vbprintf(buf, fmt, ap)						\
	do {								\
		assert(vsnprintf(buf, sizeof buf, fmt, ap)		\
		    < (int)sizeof buf);					\
	} while (0)

/**********************************************************************
 * FlexeLint shutuppery
 *
 * Flexelint is a commercial LINT program from gimpel.com, and a very
 * good one at that.  We need a few special-case markups to tell it
 * our intentions.
 */

/*
 * In OO-light situations, functions have to match their prototype
 * even if that means not const'ing a const'able argument.
 * The typedef should be specified as argument to the macro.
 *
 */
#define __match_proto__(xxx)		/*lint -e{818} */

/*
 * Some functions never return, and there's nothing Turing can do about it.
 * Some compilers think you should still include a final return(bla) in
 * such functions, while other tools complain about unreachable code.
 * Wrapping in a macro means we can shut the tools up.
 */

#define NEEDLESS_RETURN(foo)	return(foo)

/**********************************************************************
 * Compiler tricks
 */

#ifndef __printflike
#define __printflike(a, b)
#endif

/**********************************************************************
 * Mini object type checking
 *
 * This is a trivial struct type checking framework I have used with
 * a lot of success in a number of high-rel software projects over the
 * years.  It is particularly valuable when you pass things trough void
 * pointers or opaque APIs.
 *
 * Define your struct like this:
 *	struct foobar {
 *		unsigned		magic;
 *	#define FOOBAR_MAGIC		0x23923092
 *		...
 *	}
 *
 * The "magic" element SHALL be the first element of the struct.
 *
 * The MAGIC number you get from "od -x < /dev/random | head -1" or
 * similar.  It is important that each structure has its own distinct
 * value.
 *
 */

#define INIT_OBJ(to, type_magic)					\
	do {								\
		(void)memset(to, 0, sizeof *to);			\
		(to)->magic = (type_magic);				\
	} while (0)

#define ALLOC_OBJ(to, type_magic)					\
	do {								\
		(to) = calloc(1L, sizeof *(to));			\
		if ((to) != NULL)					\
			(to)->magic = (type_magic);			\
	} while (0)

#define FREE_OBJ(to)							\
	do {								\
		(to)->magic = (0);					\
		free(to);						\
	} while (0)

#define VALID_OBJ(ptr, type_magic)					\
	((ptr) != NULL && (ptr)->magic == (type_magic))

#define CHECK_OBJ(ptr, type_magic)					\
	do {								\
		assert((ptr)->magic == type_magic);			\
	} while (0)

#define CHECK_OBJ_NOTNULL(ptr, type_magic)				\
	do {								\
		assert((ptr) != NULL);					\
		assert((ptr)->magic == type_magic);			\
	} while (0)

#define CHECK_OBJ_ORNULL(ptr, type_magic)				\
	do {								\
		if ((ptr) != NULL)					\
			assert((ptr)->magic == type_magic);		\
	} while (0)

#define CAST_OBJ(to, from, type_magic)					\
	do {								\
		(to) = (from);						\
		if ((to) != NULL)					\
			CHECK_OBJ((to), (type_magic));			\
	} while (0)

#define CAST_OBJ_NOTNULL(to, from, type_magic)				\
	do {								\
		(to) = (from);						\
		assert((to) != NULL);					\
		CHECK_OBJ((to), (type_magic));				\
	} while (0)
