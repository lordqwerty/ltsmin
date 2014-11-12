#ifndef B_GREYBOX_H
#define B_GREYBOX_H

#ifdef __cplusplus
extern "C" {
#endif

#include <popt.h>
#include <pins-lib/pins.h>

extern struct poptOption b_options[];

extern void BinitGreybox(model_t model,const char*name);
extern void BloadGreyboxModel(model_t model,const char*name);

#ifdef __cplusplus
}
#endif

#endif