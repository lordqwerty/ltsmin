// System Headers
#include <hre/config.h>

// External Dependencies
#include <algorithm>
#include <iterator>
#include <iostream>
#include <memory>
#include <set>
#include <stack>
#include <string>
#include <vector>

extern "C" {
#include <popt.h>
#include <sys/stat.h>
#include <dm/dm.h>
#include <hre/unix.h>
#include <hre/user.h>
#include <limits.h>
#include <stdlib.h>

#include "../../ProBWrapper/include/bprovider.h"

// LTSmin Headers
#include <pins-lib/b-pins.h>
#include <ltsmin-lib/ltsmin-standard.h>
}

namespace ltsmin {

    class pins {

    };

}

extern "C" {

void
BinitGreybox (model_t model,const char*name)
{
    Warning(debug,"B init");
    (void)model;
    (void)name;
}

static int
BgetTransitionsLong (model_t model, int group, int *src, TransitionCB cb, void *ctx)
{
    return 0;
}

static int
BgetTransitionsAll (model_t model, int* src, TransitionCB cb, void *ctx)
{
    return 0;
}

static int
BtransitionInGroup (model_t model, int* labels, int group)
{
    return 0;
}

void
Bexit ()
{
    
}

void
BloadGreyboxModel (model_t model, const char *model_name)
{
    char abs_filename[PATH_MAX];
    const char *ret_filename = realpath (model_name, abs_filename);
    
    // check file exists
    struct stat st;
    if (stat(ret_filename, &st) != 0)
        Abort ("File does not exist: %s", ret_filename);

    GBsetContext(model,model);

    // var count
    int rootState[0];

    // Var count
    int tmp[0];
    GBsetInitialState(model, tmp);

    GBsetNextStateLong (model, BgetTransitionsLong);
    GBsetNextStateAll (model, BgetTransitionsAll);

    atexit(Bexit);
}

}