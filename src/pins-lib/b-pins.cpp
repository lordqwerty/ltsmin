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
#include <jni.h>

#include "../../ProBWrapper/include/bprovider.h"

// LTSmin Headers
#include <pins-lib/b-pins.h>
#include <ltsmin-lib/ltsmin-standard.h>
}

namespace ltsmin {

    class pins {

    public:

        BProvider* b = new BProvider();
        JavaVM* vm = b->get_jvm();

        /**
         *  For every call to the Java Wrapper we wish to make we have to create
         *  a new JNIEnv to make the call and attach it. Remember to detach after every
         *  call. 
         */
        void attach_thread()
        {
            JavaVMAttachArgs args;
            args.version = JNI_VERSION_1_6;
            JNIEnv* newEnv;
            vm->AttachCurrentThread((void**)&newEnv, &args);
        }  

        /**
         *  Detaches the current query to the Java Wrapper.
         *  Failure to detach after calls will result in JNI thread in non-java thread
         */
        void unattach_thread()
        {
            vm->DetachCurrentThread();
        }

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

        int* get_next_state_long(int id, TransitionCB cb)
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

ltsmin::pins *pins = new ltsmin::pins();

void
BinitGreybox (model_t model, const char* model_name)
{
    // Nothing to be done. 
    // Issue with shared GBloadFile in sym checker not using shared. 
    // Therefore initGreybox does not get run. Moving to loadGreyboxModel
}

static int
BgetTransitionsLong (model_t model, int group, int *src, TransitionCB cb, void *ctx)
{
    // int dst[pins->get_variable_count()]; // next state values

    // int action[1];  
    // transition_info_t transition_info = {action, group};

    
    // pins->get_next_state_long(src, cb);

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
    pins->unattach_thread();
    delete pins;
}

void
BloadGreyboxModel (model_t model, const char* model_name)
{
    Warning(info,"B init");

    // Attaches LTSmin C thread to JNI Wrapper
    pins->attach_thread();

    char abs_filename[PATH_MAX];
    char* ret_filename = realpath (model_name, abs_filename);
    
    // check file exists
    struct stat st;
    if (stat(ret_filename, &st) != 0)
        Abort ("File does not exist: %s", ret_filename);

    pins->load_machine(ret_filename);

    // create the LTS type LTSmin will generate
    lts_type_t ltstype = lts_type_create();

    int var_count = pins->get_variable_count();

    // set the length of the state
    lts_type_set_state_length(ltstype, var_count);

    // add an "int" type for a state slot
    int int_type = lts_type_add_type(ltstype, "int", NULL);
    lts_type_set_format (ltstype, int_type, LTStypeDirect);

    // set state name & type
    for (size_t i = 0; i < var_count; i++) {
       char buf[200];
       snprintf(buf, 200, "%d", i);
       lts_type_set_state_name(ltstype, i, buf);
       lts_type_set_state_typeno(ltstype, i, int_type);
    }

    printf("%s", "Out of for loop\n");

    // edge label types
    // lts_type_set_edge_label_count (ltstype, 1);

    // done with ltstype
    lts_type_validate(ltstype);
    printf("%s", "LTSType validated\n");

    // make sure to set the lts-type before anything else in the GB
    GBsetLTStype(model, ltstype);
    printf("%s", "LTSType Set\n");

    GBsetContext(model, pins);
    printf("%s", "Context Set \n");

    matrix_t *p_dm_info       = new matrix_t;
    
    // Sets the B Transition group to just one with read/write access
    dm_create(p_dm_info, 1, var_count);
    printf("%s", "Matrix made\n");

    for (size_t i = 0; i < var_count; i++) {
        dm_set (p_dm_info, 0, i);
    }
    printf("%s", "Matrix set\n");

    GBsetDMInfo (model, p_dm_info);
    printf("%s", "DM Info set\n");

    GBsetInitialState(model, pins->get_initial_state());
    printf("%s", "Set initial state\n");

    GBsetNextStateLong (model, BgetTransitionsLong);
    printf("%s", "Called BgetTransitionsLong\n");

    GBsetNextStateAll (model, BgetTransitionsAll);
    printf("%s", "Called BgetTransitionsAll\n");

    atexit(Bexit);
}

}