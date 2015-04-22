#ifndef PROB_GREYBOX_H
#define PROB_GREYBOX_H

#include <popt.h>
#include <pins-lib/pins.h>

#include "../../prob_link_library/src/pins.h"

// ProB Link Prototypes
extern void start_prob();
extern void stop_prob();
extern State *get_init_state(void);
extern int prob_get_variable_count(void);
extern int prob_get_transition_group_count(void);
extern int prob_get_state_label_count(void);
extern State **get_next_prob_state(State*, char*, int*);

extern struct poptOption prob_options[];

#endif