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

typedef struct grey_box_context {
    int num_vars;
    int op_type_no;
} gb_context_t;

/* State Conversion functions */
static void
convert_to_ltsmin_state(State* s, int *state, model_t model)
{
    for (int i = 0; i < s->size; i++)
    {
        chunk c;
        c.data = s->chunks[i].data;
        c.len = s->chunks[i].size;
        int chunk_id = GBchunkPut(model, i, c);
        state[i] = chunk_id;
    }
}

static State
*convert_to_prob_state(model_t model, int* src)
{
    State *next_prob_state = malloc(sizeof(State));

    gb_context_t* ctx = (gb_context_t*) GBgetContext(model);

    int vars = ctx->num_vars;

    next_prob_state->chunks = malloc(sizeof(Chunk) * vars);
    next_prob_state->size = vars;

    for (int i = 0; i < vars; i++) {
        chunk c = GBchunkGet(model, i, src[i]);
        next_prob_state->chunks[i].data = c.data;
        next_prob_state->chunks[i].size = c.len;
    }

    return next_prob_state;
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

void
ProBinitGreybox (model_t model, const char* model_name)
{
    (void)model; (void)model_name;
}

static int
ProBgetTransitionsLong (model_t model, int group, int *src, TransitionCB cb, void *ctx)
{
    gb_context_t* gb_ctx = (gb_context_t*) GBgetContext(model);

    int operation_type = gb_ctx->op_type_no;

    chunk op_name = GBchunkGet(model, operation_type, group);

    State *next = convert_to_prob_state(model, src);

    int nr_successors;
    State **successors = send_next_state(next, op_name.data, &nr_successors);

    int vars = gb_ctx->num_vars;

    for(int i = 0; i < nr_successors; i++)
    {
        int s[vars];

        int transition_labels[1] = { group };
        transition_info_t transition_info = { transition_labels, group, 0};

        convert_to_ltsmin_state(successors[i], s, model);
        cb(ctx, &transition_info, s, NULL);
    }

    free(next);

    return nr_successors;
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
    lts_type_set_format (ltstype, operation_type, LTStypeChunk);

    lts_type_set_edge_label_count(ltstype, 1);
    lts_type_set_edge_label_name(ltstype, 0, "operation");
    lts_type_set_edge_label_typeno(ltstype, 0, operation_type);


    // init state labels
    int sl_size = prob_get_state_label_count();
    char **sl_names = prob_get_state_label_names();
    lts_type_set_state_label_count (ltstype, sl_size);
    int bool_is_new, bool_type = lts_type_add_type(ltstype, LTSMIN_TYPE_BOOL, &bool_is_new);

    for (int i = 0; i < sl_size; i++) {
        lts_type_set_state_label_name (ltstype, i, sl_names[i]);
        lts_type_set_state_label_typeno (ltstype, i, bool_type);
    }

    // done with ltstype
    lts_type_validate(ltstype);

    // make sure to set the lts-type before anything else in the GB
    GBsetLTStype(model, ltstype);

    if (bool_is_new) {
        GBchunkPutAt(model, bool_type, chunk_str(LTSMIN_VALUE_BOOL_FALSE), 0);
        GBchunkPutAt(model, bool_type, chunk_str(LTSMIN_VALUE_BOOL_TRUE ), 1);
    }

    char **trans = prob_get_transition_names();
    for(int i = 0; i < prob_get_transition_group_count(); i++)
    {
        GBchunkPut( model, operation_type, chunk_str(trans[i]) );
    }

    gb_context_t* ctx = (gb_context_t*) RTmalloc(sizeof(gb_context_t));
    ctx->num_vars = var_count;
    ctx->op_type_no = operation_type;
    GBsetContext(model, ctx);

    matrix_t *p_dm_info = RTmalloc (sizeof(matrix_t));
    dm_create(p_dm_info, prob_get_transition_group_count(), var_count);
    for (int i = 0; i < prob_get_transition_group_count(); i++)
    {
        for (int j = 0; j < var_count; j++)
        {
            dm_set(p_dm_info, i, j);
        }
    }

    matrix_t *sl_info = RTmalloc (sizeof(matrix_t));
    dm_create(sl_info, sl_size, var_count);
    for (int i = 0; i < sl_size; i++) 
    {
        for (int j = 0; j < var_count; j++) {
            dm_set(sl_info, i, j);
        }
    }

    // set the label group implementation
    sl_group_t* sl_group_all = malloc(sizeof(sl_group_t) + (sl_size) * sizeof(int));
    sl_group_all->count = sl_size;
    for(int i=0; i < sl_group_all->count; i++) sl_group_all->sl_idx[i] = i;
    GBsetStateLabelGroupInfo(model, GB_SL_ALL, sl_group_all);

    int init_state[prob_init_state->size];
    convert_to_ltsmin_state(prob_init_state, init_state, model);

    GBsetStateLabelInfo(model, sl_info);
    GBsetDMInfo (model, p_dm_info);
    GBsetInitialState(model, init_state);
    GBsetNextStateLong (model, ProBgetTransitionsLong);

    atexit(ProBExit);
}