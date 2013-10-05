/**
 *
 */

#include <hre/config.h>

#include <stdlib.h>

#include <pins2lts-mc/algorithm/algorithm.h>
#include <pins2lts-mc/algorithm/dfs-fifo.h>
#include <pins2lts-mc/algorithm/ltl.h>
#include <pins2lts-mc/algorithm/reach.h>
#include <pins2lts-mc/algorithm/timed.h>
#include <pins2lts-mc/parallel/global.h>
#include <pins2lts-mc/parallel/state-store.h>

si_map_entry strategies[] = {
    {"none",    Strat_None},
    {"bfs",     Strat_BFS},
    {"dfs",     Strat_DFS},
    {"sbfs",    Strat_SBFS},
    {"pbfs",    Strat_PBFS},
    {"cndfs",   Strat_CNDFS},
    {"ndfs",    Strat_NDFS},
    {"lndfs",   Strat_LNDFS},
    {"endfs",   Strat_ENDFS},
    {"owcty",   Strat_OWCTY},
    {"map",     Strat_MAP},
    {"ecd",     Strat_ECD},
    {"dfsfifo", Strat_DFSFIFO},
    {NULL, 0}
};

si_map_entry provisos[] = {
    {"none",    Proviso_None},
    {"stack",   Proviso_Stack},
    {NULL, 0}
};

strategy_t       strategy[MAX_STRATEGIES] =
                {Strat_None, Strat_None, Strat_None, Strat_None, Strat_None};
proviso_t        proviso = Proviso_None;
char*            trc_output = NULL;
int              write_state = 0;
char*            label_filter = NULL;
char            *files[2];

void
options_static_setup      (model_t model, bool timed)
{
    if (strategy[0] == Strat_None)
        strategy[0] = (GBgetAcceptingStateLabelIndex(model) < 0 ?
              (strategy[0] == Strat_TA ? Strat_SBFS : Strat_BFS) : Strat_CNDFS);

    if (timed) {
        if (!(strategy[0] & (Strat_CNDFS|Strat_Reach)))
            Abort ("Wrong strategy for timed verification: %s", key_search(strategies, strategy[0]));
        strategy[0] |= Strat_TA;
    }

    if (files[1]) {
        Print1 (info,"Writing output to %s", files[1]);
        if (strategy[0] & ~Strat_PBFS) {
            Print1 (info,"Switching to PBFS algorithm for LTS write");
            strategy[0] = Strat_PBFS;
        }
    }

    if (PINS_POR && (strategy[0] & Strat_LTL & ~Strat_DFSFIFO)) {
        if (W > 1) Abort ("Cannot use POR with more than one worker.");
        if (proviso == Proviso_None) {
            Warning (info, "Forcing use of the stack cycle proviso");
            proviso = Proviso_Stack;
        }
        if (strategy[0] & Strat_NDFS) {
            Warning (info, "Warning turning off successor permutation to solve NDFS revisiting problem.");
            permutation = Perm_None;
        }
    }

    if (!ecd && strategy[1] != Strat_None)
        Abort ("Conflicting options --no-ecd and %s.", key_search(strategies, strategy[1]));

    if (Perm_Unknown == permutation) //default permutation depends on strategy
        permutation = strategy[0] & Strat_Reach ? Perm_None :
                     (strategy[0] & (Strat_TA | Strat_DFSFIFO) ? Perm_RR : Perm_Dynamic);
    if (Perm_None != permutation && refs == 0) {
        Warning (info, "Forcing use of references to enable fast successor permutation.");
        refs = 1; //The permuter works with references only!
    }

    if (!(Strat_Reach & strategy[0]) && (dlk_detect || act_detect || inv_detect)) {
        Abort ("Verification of safety properties works only with reachability algorithms.");
    }
}

void
print_options (model_t model)
{
    Warning (info, "There are %zu state labels and %zu edge labels", SL, EL);
    Warning (info, "State length is %zu, there are %zu groups", N, K);
    if (act_detect)
        Warning(info, "Detecting action \"%s\"", act_detect);
    Warning (info, "Running %s using %zu %s", key_search(strategies, strategy[0] & ~Strat_TA),
             W, W == 1 ? "core (sequential)" : "cores");
    if (db_type == HashTable) {
        Warning (info, "Using a hash table with 2^%d elements", dbs_size);
    } else
        Warning (info, "Using a%s tree table with 2^%d elements", indexing ? "" : " non-indexing", dbs_size);
    Warning (info, "Global bits: %d, count bits: %d, local bits: %d",
             global_bits, count_bits, local_bits);
    Warning (info, "Successor permutation: %s", key_search(permutations, permutation));
    if (PINS_POR) {
        int            *visibility = GBgetPorGroupVisibility (model);
        size_t          visibles = 0, labels = 0;
        for (size_t i = 0; i < K; i++)
            visibles += visibility[i];
        visibility = GBgetPorStateLabelVisibility (model);
        for (size_t i = 0; i < SL; i++)
            labels += visibility[i];
        Warning (info, "Visible groups: %zu / %zu, labels: %zu / %zu", visibles, K, labels, SL);
        Warning (info, "POR cycle proviso: %s %s", key_search(provisos, proviso), strategy[0] & Strat_LTL ? "(ltl)" : "");
    }
}

/*************************************************************************/
/* Popt                                                                  */
/*************************************************************************/

static const char      *arg_proviso = "none";
static char            *arg_strategy = "none";

static void
alg_popt (poptContext con, enum poptCallbackReason reason,
          const struct poptOption *opt, const char *arg, void *data)
{
    int                 res;
    switch (reason) {
    case POPT_CALLBACK_REASON_PRE:
        break;
    case POPT_CALLBACK_REASON_POST: {
        int i = 0, begin = 0, end = 0;
        char *strat = strdup (arg_strategy);
        char last;
        do {
            if (i > 0 && !((Strat_ENDFS | Strat_OWCTY) & strategy[i-1]))
                Abort ("Only ENDFS supports recursive repair procedures.");
            while (',' != arg_strategy[end] && '\0' != arg_strategy[end]) ++end;
            last = strat[end];
            strat[end] = '\0';
            res = linear_search (strategies, &strat[begin]);
            if (res < 0)
                Abort ("unknown search strategy %s", &strat[begin]);
            strategy[i++] = res;
            end += 1;
            begin = end;
        } while ('\0' != last && i < MAX_STRATEGIES);
        free (strat); // strdup
        if (Strat_OWCTY == strategy[0]) {
            if (ecd && Strat_None == strategy[1]) {
                Warning (info, "Defaulting to MAP as OWCTY early cycle detection procedure.");
                strategy[1] = Strat_MAP;
            }
        }
        if (Strat_ENDFS == strategy[i-1]) {
            if (MAX_STRATEGIES == i)
                Abort ("Open-ended recursion in ENDFS repair strategies.");
            Warning (info, "Defaulting to NDFS as ENDFS repair procedure.");
            strategy[i] = Strat_NDFS;
        }

        int p = proviso = linear_search (provisos, arg_proviso);
        if (p < 0) Abort ("unknown proviso %s", arg_proviso);
        return;
    }
    case POPT_CALLBACK_REASON_OPTION:
        break;
    }
    Abort ("unexpected call to alg_popt");
    (void)con; (void)opt; (void)arg; (void)data;
}

struct poptOption alg_options_extra[] = {
    {"trace", 0, POPT_ARG_STRING, &trc_output, 0, "file to write trace to", "<lts output>" },
    {"write-state", 0, POPT_ARG_VAL, &write_state, 1, "write the full state vector", NULL },
    {"filter" , 0 , POPT_ARG_STRING , &label_filter , 0 ,
     "Select the labels to be written to file from the state vector elements, "
     "state labels and edge labels." , "<patternlist>" },
    {"gran", 'g', POPT_ARG_LONGLONG | POPT_ARGFLAG_SHOW_DEFAULT | POPT_ARGFLAG_DOC_HIDDEN, &G, 0,
     "subproblem granularity ( T( work(P,g) )=min( T(P), g ) )", NULL},
    {"handoff", 0, POPT_ARG_LONGLONG | POPT_ARGFLAG_SHOW_DEFAULT | POPT_ARGFLAG_DOC_HIDDEN, &H, 0,
     "maximum balancing handoff (handoff=min(max, stack_size/2))", NULL},
    {NULL, 0, POPT_ARG_INCLUDE_TABLE, reach_options, 0, "Reachability options", NULL},
    {NULL, 0, POPT_ARG_INCLUDE_TABLE, alg_ltl_options, 0, "LTL options", NULL},
    {NULL, 0, POPT_ARG_INCLUDE_TABLE, ndfs_options, 0, "NDFS options", NULL},
    {NULL, 0, POPT_ARG_INCLUDE_TABLE, state_store_options, 0, "State store options", NULL},
    {NULL, 0, POPT_ARG_INCLUDE_TABLE, perm_options, 0, "Permutation options", NULL},
    {NULL, 0, POPT_ARG_INCLUDE_TABLE, greybox_options, 0, "Greybox options", NULL},
    POPT_TABLEEND
};

struct poptOption options[] = {
    {NULL, 0, POPT_ARG_CALLBACK | POPT_CBFLAG_POST | POPT_CBFLAG_SKIPOPTION,
     (void *)alg_popt, 0, NULL, NULL},
    {"strategy", 0, POPT_ARG_STRING | POPT_ARGFLAG_SHOW_DEFAULT,
     &arg_strategy, 0, "select the search strategy", "<bfs|sbfs|dfs|cndfs|lndfs|endfs|endfs,lndfs|endfs,endfs,ndfs|ndfs>"},
    {"proviso", 0, POPT_ARG_STRING|POPT_ARGFLAG_SHOW_DEFAULT, &arg_proviso , 0 ,
     "select proviso for ltl/por (only single core!)", "<closedset|stack>"},
    {NULL, 0, POPT_ARG_INCLUDE_TABLE, alg_options_extra, 0, NULL, NULL},
    {NULL, 0, POPT_ARG_INCLUDE_TABLE, dfs_fifo_options, 0, "DFS FIFO options", NULL},
    {NULL, 0, POPT_ARG_INCLUDE_TABLE, owcty_options, 0, "OWCTY options", NULL},
     POPT_TABLEEND
};

struct poptOption options_timed[] = {
    {NULL, 0, POPT_ARG_CALLBACK | POPT_CBFLAG_POST | POPT_CBFLAG_SKIPOPTION,
     (void *)alg_popt, 0, NULL, NULL},
    {"strategy", 0, POPT_ARG_STRING | POPT_ARGFLAG_SHOW_DEFAULT,
     &arg_strategy, 0, "select the search strategy", "<sbfs|bfs|dfs|cndfs>"},
     {NULL, 0, POPT_ARG_INCLUDE_TABLE, alg_options_extra, 0, NULL, NULL},
     {NULL, 0, POPT_ARG_INCLUDE_TABLE, timed_options, 0, "Timed Automata options", NULL},
    POPT_TABLEEND
};

