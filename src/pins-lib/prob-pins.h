#ifndef PROB_GREYBOX_H
#define PROB_GREYBOX_H

#include <popt.h>
#include <pins-lib/pins.h>

#include "../../prob_link_library/src/pins.h"

// ProB Link Prototypes
extern void start_prob();
extern void stop_prob();
extern int *get_initial_state(void);
extern int **get_next_state(int*, char*, int*);

extern struct poptOption prob_options[];

#endif