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

    public:

        void load_machine(const char* machine)
        {
            b->load_b_machine(machine);   
        }

        int get_variable_count() 
        {
            return b->get_variable_count();
        }

        int* get_initial_state() 
        {
            return b->get_initial_state();
        }

        int* get_next_state_long(int* id)
        {
            return b->get_next_state_long(id);
        }

    private:
        BProvider* b = new BProvider(); 

    };

}

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
        GBregisterPreLoader("mch", BinitGreybox);
        GBregisterLoader("mch", BloadGreyboxModel);

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

ltsmin::pins *pins;

void
BinitGreybox (model_t model, const char* model_name)
{
    Warning(debug,"B init");

    char abs_filename[PATH_MAX];
    char *ret_filename = realpath (model_name, abs_filename);

    std::cout << ret_filename;
    
    // check file exists
    struct stat st;
    if (stat(ret_filename, &st) != 0)
        Abort ("File does not exist: %s", ret_filename);

    pins->load_machine(ret_filename);

    (void)model;
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
    delete pins;
}

void
BloadGreyboxModel (model_t model, const char* model_name)
{
    // create the LTS type LTSmin will generate
    lts_type_t ltstype = lts_type_create();

    // set the length of the state
    lts_type_set_state_length(ltstype, pins->get_variable_count());

    // done with ltstype
    lts_type_validate(ltstype);

    // make sure to set the lts-type before anything else in the GB
    GBsetLTStype(model, ltstype);

    GBsetContext(model, pins);

    GBsetInitialState(model, pins->get_initial_state());

    GBsetNextStateLong (model, BgetTransitionsLong);
    GBsetNextStateAll (model, BgetTransitionsAll);

    atexit(Bexit);
}

}