#ifndef EREBUS_H_
#define EREBUS_H_

#include <signal.h>
#include "scene.h"
#include "opt.h"

extern struct scene scn;
extern volatile sig_atomic_t quit;

unsigned long get_msec(void);

#endif	/* EREBUS_H_ */
