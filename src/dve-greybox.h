#ifndef DIVINE_GREYBOX_H
#define DIVINE_GREYBOX_H

/**
\file dveC-greybox.h
*/

#include <popt.h>
#include "greybox.h"

extern struct poptOption dve_options[];

/**
Load an dveC model.
*/
extern void DVEloadGreyboxModel(model_t model,const char*name);


#endif
