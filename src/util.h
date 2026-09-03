#ifndef UTIL_H_
#define UTIL_H_

#include "szint.h"

#if (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L)
#define INLINE inline
#else
#define INLINE __inline
#endif

#ifdef _MSC_VER
#include <malloc.h>
#else
#include <alloca.h>
#endif

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201100L
#define THREAD_LOCAL	_Thread_local
#elif defined(__GNUC__)
#define THREAD_LOCAL	__thread
#else
#define THREAD_LOCAL
#endif


#endif	/* UTIL_H_ */
