#include <hre/config.h>

#include <dlfcn.h>
#include <limits.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <dm/dm.h>
#include <hre/runtime.h>
#include <hre/unix.h>
#include <ltsmin-lib/ltsmin-standard.h>
#include <pins-lib/prob-pins.h>
#include <util-lib/chunk_support.h>
#include <util-lib/util.h>

#include "../../prob_link_library/src/client.h"

extern void connect_prob();

static void
prob_popt (poptContext con,
             enum poptCallbackReason reason,
             const struct poptOption *opt,
             const char *arg, void *data);

void ProBinitGreybox (model_t model, const char* model_name);
void ProBloadGreyboxModel (model_t model, const char* model_name);
static int ProBgetTransitionsLong (model_t model, int group, int *src, TransitionCB cb, void *ctx);

/* Next-state functions */
next_method_grey_t  prob_get_successor;
next_method_grey_t  prob_get_actions;
next_method_black_t prob_get_successor_all;
void        (*prob_get_initial_state)(int *to);

/* PINS dependency matrix info */
int         (*prob_get_state_size)();
int         (*prob_get_transition_groups)();
const int*  (*prob_get_actions_read_dependencies)(int t);
const int*  (*prob_get_transition_read_dependencies)(int t);
const int*  (*prob_get_transition_may_write_dependencies)(int t);
const int*  (*prob_get_transition_must_write_dependencies)(int t);

/* PINS state type/value info */
const char* (*prob_get_state_variable_name)(int var);
int         (*prob_get_state_variable_type)(int var);
const char* (*prob_get_type_name)(int type);
int         (*prob_get_type_count)();
const char* (*prob_get_type_value_name)(int type, int value);
int         (*prob_get_type_value_count)(int type);

/* PINS flexible matrices */
const int*  (*prob_get_matrix)(int m, int x);
int         (*prob_get_matrix_count)();
const char* (*prob_get_matrix_name)(int m);
int         (*prob_get_matrix_row_count)(int m);
int         (*prob_get_matrix_col_count)(int m);

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

typedef struct grey_box_context {
    int todo;
} *gb_context_t;

void
ProBinitGreybox (model_t model, const char* model_name)
{
    Warning(info,"B init");

    char abs_filename[PATH_MAX];
    char* ret_filename = realpath (model_name, abs_filename);
    
    // check file exists
    struct stat st;
    if (stat(ret_filename, &st) != 0)
        Abort ("File does not exist: %s", ret_filename);
}

static int
ProBgetTransitionsLong (model_t model, int group, int *src, TransitionCB cb, void *ctx)
{
    return 0;
}

void
ProBloadGreyboxModel (model_t model, const char* model_name)
{
    // create the LTS type LTSmin will generate
    lts_type_t ltstype = lts_type_create();

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

    for(int i = 0; i < 1; i++)
    {
        GBchunkPut(model, operation_type, chunk_str("operations[i]"));
    }

    gb_context_t ctx=(gb_context_t)RTmalloc(sizeof(struct grey_box_context));
    GBsetContext(model, ctx);

    matrix_t *p_dm_info = RTmalloc (sizeof *p_dm_info);
    
    // Sets the B Transition group to just one with read/write access
    dm_create(p_dm_info, 1, var_count);

    dm_set (p_dm_info, 0, 0);

    int num_state_labels = 0;
    matrix_t *sl_info = RTmalloc (sizeof *sl_info);;
    dm_create(sl_info, num_state_labels, 1);
    for (int i = 0; i < num_state_labels; i++) 
    {
        dm_set(sl_info, i, i);
    }

    int *foo = 1;

    GBsetStateLabelInfo(model, sl_info);
    GBsetDMInfo (model, p_dm_info);
    GBsetInitialState(model, &foo);
    GBsetNextStateLong (model, ProBgetTransitionsLong);

    connect_prob();
}