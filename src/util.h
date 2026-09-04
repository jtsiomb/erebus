#ifndef UTIL_H_
#define UTIL_H_

#include "szint.h"

#if __STDC_VERSION__ >= 199901L
#define INLINE inline
#else
#define INLINE __inline
#endif

#ifdef _MSC_VER
#include <malloc.h>
#else
#include <alloca.h>
#endif

#if (__STDC_VERSION__ >= 201112L) && !defined(__STDC_NO_THREADS__)
#define THREAD_LOCAL	_Thread_local

#elif defined(__GNUC__)
#define THREAD_LOCAL	__thread

#else
#define THREAD_LOCAL
#endif


/* --- atomic increment --- */

#if (__STDC_VERSION__ >= 201112L) && !defined(__STDC_NO_ATOMICS__)
#include <stdatomic.h>

typedef atomic_int ATOMIC_INT;
#define atomic_int_zero(x)	atomic_store((x), 0)
#define atomic_int_inc(x)	atomic_fetch_add((x), 1)
#define atomic_int_value(x)	atomic_load((x))

#elif defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 7))

typedef volatile int ATOMIC_INT;
#define atomic_int_zero(x)	__atomic_store_n((x), 0, __ATOMIC_SEQ_CST)
#define atomic_int_inc(x)	__atomic_add_fetch((x), 1, __ATOMIC_SEQ_CST)
#define atomic_int_value(x)	__atomic_load_n((x), __ATOMIC_SEQ_CST)

#elif defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 1))

typedef volatile int ATOMIC_INT;

static INLINE void atomic_int_zero(ATOMIC_INT *ai)
{
	int prev;
	do {
		prev = *ai;
	} while(!__sync_bool_compare_and_swap(ai, prev, 0));
}

#define atomic_int_inc(x)	__sync_add_and_fetch((x), 1)
#define atomic_int_value(x)	__sync_val_compare_and_swap((x), 0, 0)

#elif defined(_MSC_VER)
#include <intrin.h>

typedef volatile long ATOMIC_INT;
#define atomic_int_zero(x)	_InterlockedExchange((x), 0)
#define atomic_int_inc(x)	_InterlockedExchangeAdd((x), 1)
#define atomic_int_value(x)	_InterlockedExchangeAdd((x), 0)

#elif defined(__sgi)
#include <mutex.h>

typedef volatile int ATOMIC_INT;

static INLINE void atomic_int_zero(ATOMIC_INT *ai)
{
	int prev;
	do {
		prev = *ai;
	} while(!compare_and_swap(ai, prev, 0));
}

#define atomic_int_inc(x)	add_then_test((x), 1)
#define atomic_int_value(x)	add_then_test((x), 0)

#else

#error "I have no atomic_int_inc implementation on this platform yet"

#endif


#endif	/* UTIL_H_ */
