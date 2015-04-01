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

        char** get_operation_names(int* length) 
        {
            return b->get_operation_names(length);
        }

        int get_operation_count()
        {
            return b->get_operation_count();
        }

        int* get_initial_state() 
        {
            return b->get_initial_state();
        }

        int** get_next_state_long(int id, int* length)
        {
            return b->get_next_state_long(id, length);
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
    int length;
    int** destinations = pins->get_next_state_long(src[0], &length);

    for(int i = 0; i < length; i++)
    {
        transition_info_t transition_info = { &destinations[i][1], 0 };
        cb(ctx, &transition_info, &destinations[i][0], NULL);
    }

    return length;
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

    // Only one since we are dealing with StateId in ProB
    int var_count = 1;

    // set the length of the state
    lts_type_set_state_length(ltstype, var_count);

    // add an "StateId" type for a state slot
    int state_id_type = lts_type_add_type(ltstype, "StateId", NULL);
    lts_type_set_format (ltstype, state_id_type, LTStypeDirect);

    // set state name & type
    lts_type_set_state_name(ltstype, 0, "StateId");
    lts_type_set_state_typeno(ltstype, 0, state_id_type);

    // add an "Operation" type for edge labels
    int operation_type = lts_type_add_type(ltstype, "Operation", NULL);
    lts_type_set_format (ltstype, operation_type, LTStypeEnum);

    // edge label types
    lts_type_set_edge_label_count(ltstype, 1);
    lts_type_set_edge_label_name(ltstype, 0, "Operation");
    lts_type_set_edge_label_type(ltstype, 0, "Operation");
    lts_type_set_edge_label_typeno(ltstype, 0, operation_type);

    // done with ltstype
    lts_type_validate(ltstype);

    // make sure to set the lts-type before anything else in the GB
    GBsetLTStype(model, ltstype);

    // Memory to store length of operations array
    int pl;

    // Char* array of names of each operation in a machine. pl passed for arr length
    char** operations = pins->get_operation_names(&pl);

    for(int i = 0; i < pl; i++)
    {
        GBchunkPut(model, operation_type, chunk_str(operations[i]));
    }

    GBsetContext(model, pins);

    matrix_t *p_dm_info       = new matrix_t;
    
    // Sets the B Transition group to just one with read/write access
    dm_create(p_dm_info, 1, var_count);

    dm_set (p_dm_info, 0, 0);

    int num_state_labels = 0;
    matrix_t *sl_info = new matrix_t;
    dm_create(sl_info, num_state_labels, 1);
    for (int i = 0; i < num_state_labels; i++) 
    {
        dm_set(sl_info, i, i);
    }

    GBsetStateLabelInfo(model, sl_info);
    GBsetDMInfo (model, p_dm_info);
    GBsetInitialState(model, pins->get_initial_state());
    GBsetNextStateLong (model, BgetTransitionsLong);

    atexit(Bexit);
}

}