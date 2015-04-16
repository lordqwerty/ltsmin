#ifndef PROB_GREYBOX_H
#define PROB_GREYBOX_H

#ifdef __cplusplus
extern "C" {
#endif

#include <popt.h>
#include <pins-lib/pins.h>

extern struct poptOption prob_options[];

extern void ProBinitGreybox(model_t model,const char*name);
extern void ProBloadGreyboxModel(model_t model, const char*name);

#ifdef __cplusplus
};
#endif

#endif