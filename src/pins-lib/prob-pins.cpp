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

// LTSmin Headers
#include <pins-lib/prob-pins.h>
#include <ltsmin-lib/ltsmin-standard.h>
}

namespace ltsmin {

    class pins {

    public:

        void something()
        {

        }
    };
};

static void
prob_popt (poptContext con,
             enum poptCallbackReason reason,
             const struct poptOption *opt,
             const char *arg, void *data)
{
    (void)con;(void)opt;(void)arg;(void)data;

    switch(reason){
    case POPT_CALLBACK_REASON_PRE:
        break;
    case POPT_CALLBACK_REASON_POST:
        GBregisterPreLoader("mch", ProBinitGreybox);
        GBregisterLoader("mch", ProBloadGreyboxModel);

        Warning(info,"B machine module initialized");
        return;
    case POPT_CALLBACK_REASON_OPTION:
        break;
    }

    Abort("unexpected call to prob_popt");
}

struct poptOption prob_options[]= {
    { NULL, 0 , POPT_ARG_CALLBACK|POPT_CBFLAG_POST|POPT_CBFLAG_SKIPOPTION , (void*)&prob_popt, 0 , NULL , NULL },
    POPT_TABLEEND
};

extern "C" {

ltsmin::pins *pins = new ltsmin::pins();

void
ProBinitGreybox (model_t model, const char* model_name)
{
    // Nothing to be done. 
    // Issue with shared GBloadFile in sym checker not using shared. 
    // Therefore initGreybox does not get run. Moving to loadGreyboxModel
}

static int
ProBgetTransitionsLong (model_t model, int group, int *src, TransitionCB cb, void *ctx)
{
    return 0;
}

void
Bexit ()
{
    delete pins;
}

void
ProBloadGreyboxModel (model_t model, const char* model_name)
{
    Warning(info,"B init");

    char abs_filename[PATH_MAX];
    char* ret_filename = realpath (model_name, abs_filename);
    
    // check file exists
    struct stat st;
    if (stat(ret_filename, &st) != 0)
        Abort ("File does not exist: %s", ret_filename);

    // create the LTS type LTSmin will generate
    lts_type_t ltstype = lts_type_create();

    //GBsetInitialState(model, pins->get_initial_state());
    GBsetNextStateLong (model, ProBgetTransitionsLong);

    atexit(Bexit);
}

}