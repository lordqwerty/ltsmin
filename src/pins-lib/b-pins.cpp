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
#include <hre/user.h>

// LTSmin Headers
#include <pins-lib/b-pins.h>
#include <ltsmin-lib/ltsmin-standard.h>
}

namespace ltsmin {

class pins {
public:
    typedef ltsmin_state_type state_vector;
    typedef int *label_vector;

    pins(model_t& model, std::string bmachinename)
        : probwrapper::b::pins(bmachinename), model_(model) 
    {
        // Call ProBWrapper to get B machine variable count
        map_.resize(get_variable_count());
        rmap_.resize(get_variable_count());
    }

    template <typename callback>
    void next_state_long(state_vector const& src, std::size_t group, callback& f,
                         state_vector const& dest, label_vector const& labels)
    {
        int state[];
        for (size_t i = 0; i < ; ++i) {

        }
        probwrapper::b::pins::next_state_long (state, group, f, dest, labels);
    }

    /*
        This function calls the B wrapper to discover the 
        next state. This is iteratively calling next_state_long 
     */
    template <typename callback>
    void next_state_all(state_vector const& src, callback& f,
                        state_vector const& dest, label_vector const& labels)
    {
        int state[];
        for (size_t i = 0; i < ; ++i) {
            int mt;
            int pt;
            state[i];
        }
        probwrapper::b::pins::next_state_all (state, f, dest, labels);   
    }

    void make_pins_edge_labels(label_vector const& src, label_vector const& dst)
    {
        
    }

    void populate_type_index(int mt, int pt)
    {
        
    }

    int transition_in_group (label_vector const& labels, int group)
    {
        return 1;
    }

private:
    static const int IDX_NOT_FOUND;
    model_t model_;
    std::vector< std::map<int,int> > rmap_;
    std::vector< std::vector<int> > map_;
};

const int pins::IDX_NOT_FOUND = -1;

struct state_cb
{
    typedef ltsmin::pins::state_vector state_vector;
    typedef ltsmin::pins::label_vector label_vector;
    ltsmin::pins *pins;
    TransitionCB& cb;
    void* ctx;
    size_t count;

    state_cb (ltsmin::pins *pins_, TransitionCB& cb_, void *ctx_)
        : pins(pins_), cb(cb_), ctx(ctx_), count(0)
    {}

    void operator()(state_vector const& next_state,
                    label_vector const& edge_labels, int group = -1)
    {
        int lbl[pins->edge_label_count()];
        pins->make_pins_edge_labels(edge_labels, lbl);
        int dst[pins->process_parameter_count()];
        pins->make_pins_state(next_state, dst);
        transition_info_t ti = GB_TI(lbl,group);
        cb (ctx, &ti, dst);
        ++count;
    }

    size_t get_count() const
    {
        return count;
    }

};

}

extern "C" {

void
BinitGreybox (int argc,const char *argv[],void* stack_bottom)
{
    Warning(debug,"B init");
    (void)argc;
    (void)argv;
    (void)stack_bottom;
}

static int
BgetTransitionsLong (model_t m, int group, int *src, TransitionCB cb, void *ctx)
{

}

static int
BgetTransitionsAll (model_t m, int* src, TransitionCB cb, void *ctx)
{

}

static int
BtransitionInGroup (model_t m, int* labels, int group)
{

}

ltsmin::pins *pins;

void
Bexit ()
{
    delete pins;
}

void
BloadGreyboxModel (model_t m, const char *model_name)
{

    pins = new ltsmin::pins(m, std::string(model_name));
    GBsetContext(m,pins);

    /**
     *  Todo
     */
    int rootState[0];
    ltsmin::pins::state_vector p_rootState = rootState;
    pins->get_initial_state(p_rootState);

    int tmp[0];
    pins->make_pins_state(rootState,tmp);
    GBsetInitialState(m, tmp);

    GBsetNextStateLong(m, BgetTransitionsLong);
    GBsetNextStateAll(m, BgetTransitionsAll);

    atexit(Bexit);
}

}