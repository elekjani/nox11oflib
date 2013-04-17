#include "basePP.hh"

namespace ericsson {

    JsonDispatcher* BasePP::jsonDisp = NULL;

    uint32_t BasePP::max_proc_id = 0;

    uint32_t BasePP::next_proc_id() {
        return max_proc_id++;
    }

}//namespace ericsson
