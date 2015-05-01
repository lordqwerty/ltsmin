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
void ProBExit(void);

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
void convert_to_ltsmin_state(State* s, int *state, model_t model);
State *convert_to_prob_state(chunk ltsmin_chunk);

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
    chunk ltsmin_chunk = GBchunkGet(model, 0, src[0]);
    char prob_transition_group[16];
    chunk2string(ltsmin_chunk, 16, prob_transition_group);
    
    State *next = convert_to_prob_state(ltsmin_chunk);

    int nr_successors;
    State **successors = send_next_state(next, prob_transition_group, &nr_successors);

    int *next_states[nr_successors];
    for(int i = 0; i < nr_successors; i++)
    {
        convert_to_ltsmin_state(successors[i], &next_states, model);
    }

    //convert_to_ltsmin_state(next, &next_states, model);

    // transition_info_t transition_info = { &destinations[i][1], 0 };
    // cb(ctx, &transition_info, &destinations[i][0], NULL);

    return 0;
}

void
ProBExit(void)
{
    stop_prob();
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
    
    // Need to get init state so we can have counts
    State *prob_init_state = prob_get_init_state();
    int var_count = prob_get_variable_count();
    
    // create the LTS type LTSmin will generate
    lts_type_t ltstype = lts_type_create();

    // set the length of the state
    lts_type_set_state_length(ltstype, var_count);

    char **variables = prob_get_variable_names();

    for (int i = 0; i < var_count; i++) 
    {
        const char* type_name = variables[i];
        HREassert (type_name != NULL, "invalid type name");
        if (lts_type_add_type(ltstype, type_name, NULL) != i) 
        {
            Abort("Type number incorrect");
        }
        
        lts_type_set_format (ltstype, i, LTStypeChunk);
    }

    // set state name & type
    for (int i = 0; i < var_count; ++i) 
    {
        lts_type_set_state_name(ltstype, i, variables[i]);
        lts_type_set_state_typeno(ltstype, i, i);
    }

    // add an "Operation" type for edge labels
    int operation_type = lts_type_add_type(ltstype, "Operation", NULL);
    lts_type_set_format (ltstype, operation_type, LTStypeEnum);

    int sl_size = prob_get_state_label_count();
    lts_type_set_edge_label_count(ltstype, sl_size);

    char **sl_names = prob_get_state_label_names();
    for(int i = 0; i < sl_size; i++)
    {
        lts_type_set_edge_label_name(ltstype, i, sl_names[i]);
        lts_type_set_edge_label_typeno(ltstype, i, operation_type);
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
    
    // Sets the B Transition group
    dm_create(p_dm_info, prob_get_transition_group_count(), var_count);

    dm_set (p_dm_info, 0, 0);

    matrix_t *sl_info = RTmalloc (sizeof *sl_info);
    dm_create(sl_info, sl_size, 1);
    for (int i = 0; i < sl_size; i++) 
    {
        dm_set(sl_info, i, i);
    }

    int *init_state[prob_init_state->size];
    convert_to_ltsmin_state(prob_init_state, &init_state, model);

    GBsetStateLabelInfo(model, sl_info);
    GBsetDMInfo (model, p_dm_info);
    GBsetInitialState(model, &init_state);
    GBsetNextStateLong (model, ProBgetTransitionsLong);

    //atexit(ProBExit);
}

/* Helper functions */
void
convert_to_ltsmin_state(State* s, int *state, model_t model)
{
    for (int i = 0; i < s->size; i++) 
    {
        int chunk_id = GBchunkPut(model, i, chunk_str(s->chunks[i].data));
        state[i] = chunk_id;
    }
}

State
*convert_to_prob_state(chunk ltsmin_chunk)
{
    State *next_prob_state = malloc(sizeof(State));
    next_prob_state->chunks = malloc(sizeof(Chunk) * (int) ltsmin_chunk.len);
    next_prob_state->size = (int) ltsmin_chunk.len;

    printf("ltsmin_chunk size: %d", (int) ltsmin_chunk.len);

    for(int i = 0; i < ltsmin_chunk.len; i++)
    {
        next_prob_state->chunks[i].data = ltsmin_chunk.data;
        next_prob_state->chunks[i].size = ltsmin_chunk.len; 
    }

    return next_prob_state;
}