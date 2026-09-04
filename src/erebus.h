#ifndef EREBUS_H_
#define EREBUS_H_

#include <signal.h>
#include "scene.h"
#include "opt.h"
#include "util.h"

extern struct scene scn;
extern volatile sig_atomic_t quit;

extern ATOMIC_INT progr_done_tiles;
extern unsigned int progr_total_tiles;

unsigned long get_msec(void);

#endif	/* EREBUS_H_ */
