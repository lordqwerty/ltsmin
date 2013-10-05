#include <hre/config.h>

#include <pins2lts-mc/algorithm/algorithm.h>
#include <pins2lts-mc/algorithm/cndfs.h>
#include <pins2lts-mc/parallel/color.h>
#include <pins2lts-mc/parallel/options.h>
#include <pins2lts-mc/parallel/permute.h>
#include <pins2lts-mc/parallel/state-info.h>
#include <pins2lts-mc/parallel/worker.h>

struct alg_shared_s {
    run_t              *rec;
    run_t              *previous;
    int                 color_bit_shift;
    int                 top_level;
};

struct alg_global_s {
    wctx_t             *rec;
    ref_t               work;           // ENDFS work for loadbalancer
    int                 done;           // ENDFS done for loadbalancer
};

extern void rec_ndfs_call (wctx_t *ctx, ref_t state);

static void
endfs_lb (wctx_t *ctx)
{
    alg_global_t           *sm = ctx->global;
    atomic_write (&sm->done, 1);
    size_t workers[W];
    int idle_count = W-1;
    for (size_t i = 0; i<((size_t)W); i++)
        workers[i] = (i==ctx->id ? 0 : 1);
    while (0 != idle_count) {
        for (size_t i = 0; i < W; i++) {
            if (0==workers[i])
                continue;
            alg_global_t           *remote = ctx->run->contexts[i]->global;
            if (1 == atomic_read(&remote->done)) {
                workers[i] = 0;
                idle_count--;
                continue;
            }
            ref_t work = atomic_read (&remote->work);
            if (SIZE_MAX == work)
                continue;
            rec_ndfs_call (ctx, work);
        }
    }
}

static void
endfs_handle_dangerous (wctx_t *ctx)
{
    alg_local_t        *loc = ctx->local;
    while ( dfs_stack_size(loc->in_stack) ) {
        raw_data_t state_data = dfs_stack_pop (loc->in_stack);
        state_info_deserialize_cheap (&ctx->state, state_data);
        if ( !global_has_color(ctx->state.ref, GDANGEROUS, loc->rec_bits) &&
              ctx->state.ref != loc->seed )
            if (global_try_color(ctx->state.ref, GRED, loc->rec_bits))
                loc->red.explored++;
    }
    if (global_try_color(loc->seed, GRED, loc->rec_bits)) {
        loc->red.explored++;
        loc->counters.accepting++;
    }
    if ( global_has_color(loc->seed, GDANGEROUS, loc->rec_bits) ) {
        rec_ndfs_call (ctx, loc->seed);
    }
}

static void
cndfs_handle_nonseed_accepting (wctx_t *ctx)
{
    alg_local_t        *loc = ctx->local;
    cndfs_alg_local_t  *cndfs_loc = (cndfs_alg_local_t *) ctx->local;
    size_t nonred, accs;
    nonred = accs = dfs_stack_size(cndfs_loc->out_stack);

    if (nonred) {
        loc->counters.waits++;
        cndfs_loc->counters.rec += accs;
        RTstartTimer (cndfs_loc->timer);
        while ( nonred && !lb_is_stopped(global->lb) ) {
            nonred = 0;
            for (size_t i = 0; i < accs; i++) {
                raw_data_t state_data = dfs_stack_peek (cndfs_loc->out_stack, i);
                state_info_deserialize_cheap (&ctx->state, state_data);
                if (!global_has_color(ctx->state.ref, GRED, loc->rec_bits))
                    nonred++;
            }
        }
        RTstopTimer (cndfs_loc->timer);
    }
    for (size_t i = 0; i < accs; i++)
        dfs_stack_pop (cndfs_loc->out_stack);
    while ( dfs_stack_size(loc->in_stack) ) {
        raw_data_t state_data = dfs_stack_pop (loc->in_stack);
        state_info_deserialize_cheap (&ctx->state, state_data);
        if (global_try_color(ctx->state.ref, GRED, loc->rec_bits))
            loc->red.explored++;
    }
}

static void
endfs_handle_red (void *arg, state_info_t *successor, transition_info_t *ti, int seen)
{
    wctx_t             *ctx = (wctx_t *) arg;
    alg_local_t        *loc = ctx->local;
    /* Find cycle back to the seed */
    nndfs_color_t color = nn_get_color (&loc->color_map, successor->ref);

    ti->por_proviso = 1; // only sequentially!
    if (proviso != Proviso_None && !nn_color_eq(color, NNBLUE))
         return; // only revisit blue states to determinize POR
    if ( nn_color_eq(color, NNCYAN) )
        ndfs_report_cycle (ctx, successor);
    /* Mark states dangerous if necessary */
    if ( Strat_ENDFS == loc->strat &&
         GBbuchiIsAccepting(ctx->model, get_state(successor->ref, ctx)) &&
         !global_has_color(successor->ref, GRED, loc->rec_bits) )
        global_try_color(successor->ref, GDANGEROUS, loc->rec_bits);
    if ( !nn_color_eq(color, NNPINK) &&
         !global_has_color(successor->ref, GRED, loc->rec_bits) ) {
        raw_data_t stack_loc = dfs_stack_push (loc->stack, NULL);
        state_info_serialize (successor, stack_loc);
    }
    (void) ti; (void) seen;
}

static void
endfs_handle_blue (void *arg, state_info_t *successor, transition_info_t *ti, int seen)
{
    wctx_t             *ctx = (wctx_t *) arg;
    alg_local_t        *loc = ctx->local;
    nndfs_color_t color = nn_get_color (&loc->color_map, successor->ref);

    if (proviso == Proviso_Stack) // only sequentially!
        ti->por_proviso = !nn_color_eq(color, NNCYAN);

    /**
     * The following lines bear little resemblance to the algorithms in the
     * respective papers (Evangelista et al./ Laarman et al.), but we must
     * store all non-red states on the stack in order to calculate
     * all-red correctly later. Red states are also stored as optimization.
     */
    if ( ecd && nn_color_eq(color, NNCYAN) &&
         (GBbuchiIsAccepting(ctx->model, ctx->state.data) ||
         GBbuchiIsAccepting(ctx->model, get_state(successor->ref, ctx))) ) {
        /* Found cycle in blue search */
        ndfs_report_cycle(ctx, successor);
    } else if ( all_red || (!nn_color_eq(color, NNCYAN) && !nn_color_eq(color, NNBLUE) &&
                            !global_has_color(successor->ref, GGREEN, loc->rec_bits)) ) {
        raw_data_t stack_loc = dfs_stack_push (loc->stack, NULL);
        state_info_serialize (successor, stack_loc);
    }
    (void) ti; (void) seen;
}

static inline void
endfs_explore_state_red (wctx_t *ctx, counter_t *cnt)
{
    alg_local_t        *loc = ctx->local;
    dfs_stack_enter (loc->stack);
    increase_level (&cnt->level_cur, &cnt->level_max);
    cnt->trans += permute_trans (ctx->permute, &ctx->state, endfs_handle_red, ctx);
    maybe_report (cnt->explored, cnt->trans, cnt->level_max, "[R] ");
}

static inline void
endfs_explore_state_blue (wctx_t *ctx, counter_t *cnt)
{
    alg_local_t        *loc = ctx->local;
    dfs_stack_enter (loc->stack);
    increase_level (&cnt->level_cur, &cnt->level_max);
    cnt->trans += permute_trans (ctx->permute, &ctx->state, endfs_handle_blue, ctx);
    cnt->explored++;
    maybe_report1 (cnt->explored, cnt->trans, cnt->level_max, "[B] ");
}

/* ENDFS dfs_red */
static void
endfs_red (wctx_t *ctx, ref_t seed)
{
    alg_local_t        *loc = ctx->local;
    cndfs_alg_local_t  *cndfs_loc = (cndfs_alg_local_t *) ctx->local;
    size_t              seed_level = dfs_stack_nframes (loc->stack);
    while ( !lb_is_stopped(global->lb) ) {
        raw_data_t          state_data = dfs_stack_top (loc->stack);
        if (NULL != state_data) {
            state_info_deserialize (&ctx->state, state_data, ctx->store);
            nndfs_color_t color = nn_get_color (&loc->color_map, ctx->state.ref);
            if ( !nn_color_eq(color, NNPINK) &&
                 !global_has_color(ctx->state.ref, GRED, loc->rec_bits) ) {
                nn_set_color (&loc->color_map, ctx->state.ref, NNPINK);
                dfs_stack_push (loc->in_stack, state_data);
                if ( Strat_CNDFS == loc->strat &&
                     ctx->state.ref != seed &&
                     GBbuchiIsAccepting(ctx->model, ctx->state.data) )
                    dfs_stack_push (cndfs_loc->out_stack, state_data);
                endfs_explore_state_red (ctx, &loc->red);
            } else {
                if (seed_level == dfs_stack_nframes (loc->stack))
                    break;
                dfs_stack_pop (loc->stack);
            }
        } else { //backtrack
            dfs_stack_leave (loc->stack);
            loc->red.level_cur--;
            /* exit search if backtrack hits seed, leave stack the way it was */
            if (seed_level == dfs_stack_nframes(loc->stack))
                break;
            dfs_stack_pop (loc->stack);
        }
    }
}

void // just for checking correctness of all-red implementation. Unused.
check (void *arg, state_info_t *successor, transition_info_t *ti, int seen)
{
    wctx_t             *ctx = arg;
    alg_local_t        *loc = ctx->local;
    HREassert (global_has_color(successor->ref, GRED, loc->rec_bits));
    (void) ti; (void) seen;
}

/* ENDFS dfs_blue */
void
endfs_blue (run_t *run, wctx_t *ctx)
{
    alg_local_t        *loc = ctx->local;
    alg_global_t       *sm = ctx->global;
    cndfs_alg_local_t  *cndfs_loc = (cndfs_alg_local_t *) ctx->local;
    HREassert (ecd, "CNDFS's correctness depends crucially on ECD");
    while ( !lb_is_stopped(global->lb) ) {
        raw_data_t          state_data = dfs_stack_top (loc->stack);
        if (NULL != state_data) {
            state_info_deserialize (&ctx->state, state_data, ctx->store);
            nndfs_color_t color = nn_get_color (&loc->color_map, ctx->state.ref);
            if ( !nn_color_eq(color, NNCYAN) && !nn_color_eq(color, NNBLUE) &&
                 !global_has_color(ctx->state.ref, GGREEN, loc->rec_bits) ) {
                if (all_red)
                    bitvector_set (&loc->all_red, loc->counters.level_cur);
                nn_set_color (&loc->color_map, ctx->state.ref, NNCYAN);
                endfs_explore_state_blue (ctx, &loc->counters);
            } else {
                if ( all_red && loc->counters.level_cur != 0 && !global_has_color(ctx->state.ref, GRED, loc->rec_bits) )
                    bitvector_unset (&loc->all_red, loc->counters.level_cur - 1);
                dfs_stack_pop (loc->stack);
            }
        } else { //backtrack
            if (0 == dfs_stack_nframes(loc->stack))
                break;
            dfs_stack_leave (loc->stack);
            loc->counters.level_cur--;
            /* call red DFS for accepting states */
            state_data = dfs_stack_top (loc->stack);
            state_info_deserialize (&ctx->state, state_data, ctx->store);
            /* Mark state GGREEN on backtrack */
            global_try_color (ctx->state.ref, GGREEN, loc->rec_bits);
            nn_set_color (&loc->color_map, ctx->state.ref, NNBLUE);
            if ( all_red && bitvector_is_set(&loc->all_red, loc->counters.level_cur) ) {
                /* all successors are red */
                //permute_trans (loc->permute, &ctx->state, check, ctx);
                set_all_red (ctx, &ctx->state);
            } else if ( GBbuchiIsAccepting(ctx->model, ctx->state.data) ) {
                loc->seed = sm->work = ctx->state.ref;
                endfs_red (ctx, loc->seed);
                if (Strat_ENDFS == loc->strat)
                    endfs_handle_dangerous (ctx);
                else
                    cndfs_handle_nonseed_accepting (ctx);
                sm->work = SIZE_MAX;
            } else if (all_red && loc->counters.level_cur > 0 &&
                       !global_has_color(ctx->state.ref, GRED, loc->rec_bits)) {
                /* unset the all-red flag (only for non-initial nodes) */
                bitvector_unset (&loc->all_red, loc->counters.level_cur - 1);
            }
            dfs_stack_pop (loc->stack);
        }
    }

    // if the recursive strategy uses global bits (global pruning)
    // then do simple load balancing (only for the top-level strategy)
    if ( Strat_ENDFS == loc->strat &&
         run->shared->top_level &&
         (Strat_LTLG & sm->rec->local->strat) )
        endfs_lb (ctx);
}

void
rec_ndfs_call (wctx_t *ctx, ref_t state)
{
    cndfs_alg_local_t  *cndfs_loc = (cndfs_alg_local_t *) ctx->local;
    alg_global_t       *sm = ctx->global;
    alg_local_t        *loc = ctx->local;
    strategy_t          rec_strat = get_strategy (ctx->run->shared->rec->alg);
    dfs_stack_push (sm->rec->local->stack, (int*)&state);
    cndfs_loc->counters.rec++;
    switch (rec_strat) {
    case Strat_ENDFS:
       endfs_blue (sm->rec->run, sm->rec); break;
    case Strat_LNDFS:
       lndfs_blue (sm->rec->run, sm->rec); break;
    case Strat_NDFS:
       ndfs_blue (sm->rec->run, sm->rec); break;
    default:
       Abort ("Invalid recursive strategy.");
    }
}

void
cndfs_local_init   (run_t *run, wctx_t *ctx)
{
    alg_local_t        *loc = RTmallocZero (sizeof(cndfs_alg_local_t));
    cndfs_alg_local_t  *cndfs_loc = (cndfs_alg_local_t *) loc;
    ctx->local = loc;
    ndfs_local_setup (run, ctx);

    if (run->shared == NULL) {
        HREassert (get_strategy(run->alg) & Strat_CNDFS,
                   "Missing recusive strategy for %s!",
                   key_search(strategies, get_strategy(run->alg)));
        return;
    }

    HREassert (ctx->global != NULL, "Run global before local init");

    alg_local_init (run->shared->rec, ctx->global->rec);

    // Recursive strategy maybe unaware of its caller, so here we update its
    // recursive bits (top-level strategy always has rec_bits == 0, which
    // is ensured by ndfs_local_setup):
    ctx->global->rec->local->rec_bits = run->shared->color_bit_shift;
    cndfs_loc->rec = ctx->global->rec->local;
}

void
cndfs_global_init   (run_t *run, wctx_t *ctx)
{
    ctx->global = RTmallocZero (sizeof(alg_global_t));
    ctx->global->work = SIZE_MAX;

    if (run->shared == NULL)
        return;

    ctx->global->rec = wctx_create (ctx->model, run);
    alg_global_init (run->shared->rec, ctx->global->rec);
}

void
cndfs_destroy   (run_t *run, wctx_t *ctx)
{ // TODO
    (void) run; (void) ctx;
}

void
cndfs_destroy_local   (run_t *run, wctx_t *ctx)
{ //TODO
    (void) run; (void) ctx;
}

void
cndfs_print_stats   (run_t *run, wctx_t *ctx)
{
    size_t              db_elts = global->stats.elts;
    size_t              accepting = run->reduced->blue.accepting;


    //RTprintTimer (info, ctx->timer, "Total exploration time ");
    Warning (info, "Total exploration time %5.3f real", run->maxtime);

    Warning (info, " ");
    Warning (info, "State space has %zu states, %zu are accepting", db_elts, accepting);

    wctx_t             *cur = ctx;
    int                 index = 1;
    while (cur != NULL) {
        cndfs_reduced_t        *creduced = (cndfs_reduced_t *) cur->run->reduced;
        alg_reduced_t          *reduced = cur->run->reduced;

        reduced->blue.explored /= W;
        reduced->blue.trans /= W;
        reduced->red.explored /= W;
        reduced->red.trans /= W;
        reduced->red.bogus_red /= W;
        ndfs_print_state_stats (cur->run, cur, index, creduced->waittime);

        if (cur->local->strat & Strat_ENDFS) {
            Warning (infoLong, " ");
            Warning (infoLong, "ENDFS recursive calls:");
            Warning (infoLong, "Calls: %zu",  creduced->rec);
            Warning (infoLong, "Waits: %zu",  reduced->blue.waits);
        }

        cur = cur->global != NULL ? cur->global->rec : NULL;
        index++;
    }

    size_t mem3 = ((double)(((((size_t)local_bits)<<dbs_size))/8*W)) / (1UL<<20);
    Warning (info, " ");
    Warning (info, "Total memory used for local state coloring: %.1fMB", mem3);

}

void
cndfs_reduce  (run_t *run, wctx_t *ctx)
{
    if (run->reduced == NULL) {
        run->reduced = RTmallocZero (sizeof (cndfs_reduced_t));
        cndfs_reduced_t        *reduced = (cndfs_reduced_t *) run->reduced;
        reduced->waittime = 0;
    }
    cndfs_reduced_t        *reduced = (cndfs_reduced_t *) run->reduced;
    cndfs_alg_local_t      *cndfs_loc = (cndfs_alg_local_t *) ctx->local;
    float                   waittime = RTrealTime(cndfs_loc->timer);
    reduced->waittime += waittime;
    reduced->rec += cndfs_loc->counters.rec;

    ndfs_reduce (run, ctx);

    if (run->shared->rec) {
        alg_global_t           *sm = ctx->global;
        alg_reduce (run->shared->rec, sm->rec);
    }
}

void
cndfs_shared_init   (run_t *run)
{
    HREassert (GRED.g == 0);
    HREassert (GGREEN.g == 1);
    HREassert (GDANGEROUS.g == 2);

    set_alg_local_init (run->alg, cndfs_local_init);
    set_alg_global_init (run->alg, cndfs_global_init);
    set_alg_global_deinit (run->alg, cndfs_destroy);
    set_alg_local_deinit (run->alg, cndfs_destroy_local);
    set_alg_print_stats (run->alg, cndfs_print_stats);
    set_alg_run (run->alg, endfs_blue);
    set_alg_state_seen (run->alg, ndfs_state_seen);
    set_alg_reduce (run->alg, cndfs_reduce);

    if (run->shared != NULL)
        return;

    run->shared = RTmallocZero (sizeof(alg_shared_t));
    run->shared->color_bit_shift = 0;
    run->shared->top_level = 1;

    int             i = 1;
    run_t          *previous = run;
    run_t          *next = NULL;
    while (strategy[i] != Strat_None) {
        next = run_create ();
        next->shared = RTmallocZero (sizeof(alg_shared_t));
        next->shared->previous = previous;
        next->shared->color_bit_shift = previous->shared->color_bit_shift +
                                        num_global_bits (strategy[i]);

        alg_shared_init_strategy (next, strategy[i]);

        previous->shared->rec = next;
        previous = next;
        i++;
    }
}
