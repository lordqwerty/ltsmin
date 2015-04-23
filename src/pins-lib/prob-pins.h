#ifndef PROB_GREYBOX_H
#define PROB_GREYBOX_H

#include <popt.h>
#include <pins-lib/pins.h>

#include "../../prob_link_library/src/pins.h"

// ProB Link Prototypes
extern void start_prob(void);
extern void stop_prob(void);

extern State *prob_get_init_state(void);
extern State **prob_get_next_state(State*, char*, int*);

extern int prob_get_variable_count(void);
extern int prob_get_transition_group_count(void);
extern int prob_get_state_label_count(void);

extern char **prob_get_variable_names(void);
extern char **prob_get_transition_names(void);
extern char **prob_get_state_label_names(void);

extern struct poptOption prob_options[];

#endif