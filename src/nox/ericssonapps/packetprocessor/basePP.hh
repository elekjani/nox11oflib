#ifndef BASE_PP_HH
#define BASE_PP_HH

#include "netinet++/datapathid.hh"
#include "../jsondispatcher/jsondispatcher.hh"

#include <map>
#include <set>
#include <list>

using namespace vigil;
using namespace std;

namespace ericsson {

    class BasePP {
        public :
            bool enabled;
            static JsonDispatcher *jsonDisp;

        protected:
            static uint32_t max_proc_id;

            map<datapathid, set<uint32_t> >  proc_id_registry;

        public :
            BasePP() : enabled(false) { };

            virtual void barrier_handle(const datapathid &dpid, list<struct ofl_msg_flow_mod*> &flow_mod) = 0;
            virtual void join_handle(const datapathid &dpid, list<struct ofl_msg_processor_mod*> &proc_mod) = 0;
            virtual void ctrl_handle(const datapathid &dpid, uint32_t proc_id, uint8_t *data, size_t data_length) = 0;
            virtual void dp_leaved(const datapathid &dpid, uint32_t proc_id) = 0;

            uint32_t next_proc_id();
    };

} //namespace ericsson


#endif
