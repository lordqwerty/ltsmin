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

/* State Conversion functions */
chunk *convert_to_ltsmin_state(State* s);
State *convert_to_prob_state(chunk* s);

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

        Warning(info,"ProB module initialized");
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
    // Not called by LTSmin backend.
}

static int
ProBgetTransitionsLong (model_t model, int group, int *src, TransitionCB cb, void *ctx)
{
    return 0;
}

void
ProBloadGreyboxModel (model_t model, const char* model_name)
{
    Warning(info,"ProB init");

    char abs_filename[PATH_MAX];
    char* ret_filename = realpath (model_name, abs_filename);
    
    // check file exists
    struct stat st;
    if (stat(ret_filename, &st) != 0)
        Abort ("File does not exist: %s", ret_filename);

    start_prob();

    // Need to get init state so we can have var count
    State *initState = get_init_state();
    int var_count = prob_get_variable_count();

    // create the LTS type LTSmin will generate
    lts_type_t ltstype = lts_type_create();

    char **variables = prob_get_variable_names();

    // set state name & type
    for (int i=0; i < var_count; ++i) 
    {
        lts_type_set_state_name(ltstype, i, variables[i]);
        lts_type_set_state_typeno(ltstype, i, type);
    }

    // set the length of the state
    lts_type_set_state_length(ltstype, var_count);

    // edge label types
    lts_type_set_edge_label_count (ltstype, prob_get_state_label_count());
    for (int i = 0; i < prom_get_edge_count(); i++) 
    {
        lts_type_set_edge_label_name(ltstype, i, prom_get_edge_name(i));
        int typeno = prom_get_edge_type(i);
        const char* type_name = prom_get_type_name(typeno);
        lts_type_set_edge_label_type(ltstype, i, type_name);
        lts_type_set_edge_label_typeno(ltstype, i, typeno);
    }

    // done with ltstype
    lts_type_validate(ltstype);

    // make sure to set the lts-type before anything else in the GB
    GBsetLTStype(model, ltstype);

    char **trans = prob_get_transition_names();
    for(int i = 0; i < prob_get_transition_group_count(); i++)
    {
        GBchunkPut( model, operation_type, chunk_str(trans[i]) );
    }

    gb_context_t ctx=(gb_context_t)RTmalloc(sizeof(struct grey_box_context));
    GBsetContext(model, ctx);

    matrix_t *p_dm_info = RTmalloc (sizeof *p_dm_info);
    
    // Sets the B Transition group to just one with read/write access
    dm_create(p_dm_info, prob_get_transition_group_count(), var_count);

    dm_set (p_dm_info, 0, 0);

    int num_state_labels = prob_get_state_label_count();
    matrix_t *sl_info = RTmalloc (sizeof *sl_info);
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
}

/* Helper functions */

// chunk
// *convert_to_ltsmin_state(State* s)
// {
    // for (int i = 0; i < *s->size; i++) 
    // {
    //     Chunk* prob_chunk = &(*s.chunks[i]);
    //     chunk ltsmin_chunk = malloc(sizeof(chunk) + prob_chunk->size);
    //     memcpy(prob_chunk, ltsmin_chunk, prob_chunk->size + 1); 
    //     
    //     free((*s)->chunks[i].data);
    // }
    //
    // free((*s)->chunks);
    // free(*s);
    // *s = NULL;
// }

// State
// *convert_to_prob_state(chunk* s)
// {
    // for (int i = 0; i < (int) *s->len; i++) 
    // {
    //     chunk ltsmin_chunk = &(*s.data);
    //     Chunk* prob_chunk = malloc(sizeof(Chunk) + (int) ltsmin_chunk->len);
    //     memcpy(ltsmin_chunk, prob_chunk, (int) ltsmin_chunk->len + 1); 

    //     free((*s)->data);
    // }

    // free(*s);
    // *s = NULL;
// }