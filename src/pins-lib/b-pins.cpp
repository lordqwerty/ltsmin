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

// Global access. Reduces number of VMs
BProvider* b = new BProvider();

namespace ltsmin {

    class pins {

    public:

        int varCount;

        void load_machine(const char* machine)
        {
            b->load_b_machine(machine);   
        }

        int get_variable_count() 
        {
            if(varCount == NULL) {
                varCount = b->get_variable_count();
            }

            return varCount;
        }

        int* get_initial_state() 
        {
            return b->get_initial_state();
        }

        int* get_next_state_long(int* id)
        {
            return b->get_next_state_long(id);
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
    Warning(info,"B init");

    char abs_filename[PATH_MAX];
    const char* ret_filename = realpath (model_name, abs_filename);
    
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
    int dst[pins->get_variable_count()]; // next state values
    memcpy(dst, src, sizeof(int) * pins->get_variable_count());

    int action[1];  
    transition_info_t transition_info = {action, group};

    cb(ctx, &transition_info, dst, NULL);
    pins->get_next_state_long(src);

    return 1;
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
    delete b;
}

void
BloadGreyboxModel (model_t model, const char* model_name)
{
    // create the LTS type LTSmin will generate
    lts_type_t ltstype = lts_type_create();

    // set the length of the state
    lts_type_set_state_length(ltstype, pins->get_variable_count());

    // add an "int" type for a state slot
    int int_type = lts_type_add_type(ltstype, "int", NULL);
    lts_type_set_format (ltstype, int_type, LTStypeDirect);

    // edge label types
    lts_type_set_edge_label_count (ltstype, pins->get_variable_count());

    // done with ltstype
    lts_type_validate(ltstype);

    // make sure to set the lts-type before anything else in the GB
    GBsetLTStype(model, ltstype);

    GBsetContext(model, pins);

    matrix_t *p_dm_info       = new matrix_t;
    
    dm_create(p_dm_info, pins->get_variable_count(),
              pins->get_variable_count());

    for (size_t i = 0; i < pins->get_variable_count(); i++) {
        dm_set (p_dm_info, 0, i);
    }

    GBsetDMInfo (model, p_dm_info);

    GBsetInitialState(model, pins->get_initial_state());

    GBsetNextStateLong (model, BgetTransitionsLong);
    GBsetNextStateAll (model, BgetTransitionsAll);

    atexit(Bexit);
}

}