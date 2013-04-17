#ifndef PACKETPROCEsSOR_HH
#define PACKETPROCESSOR_HH

#include <boost/bind.hpp>
#include <boost/foreach.hpp>
#include <boost/shared_array.hpp>

#include <string>
#include <map>
#include <list>
#include <netinet/in.h>
#include <stdexcept>
#include <stdint.h>

#include "openflow-default.hh"
#include "assert.hh"
#include "component.hh"
#include "flow.hh"
#include "fnv_hash.hh"
#include "hash_set.hh"
#include "ofp-msg-event.hh"
#include "datapath-join.hh"
#include "datapath-leave.hh"
#include "vlog.hh"
#include "packets.h"
#include "openflow.hh"
#include "deployer.hh"

#include "netinet++/ethernetaddr.hh"
#include "netinet++/ethernet.hh"

#include "../../../oflib/ofl-actions.h"
#include "../../../oflib/ofl-messages.h"
#include "../../../oflib/ofl-packets.h"

#include "lldpPP.hh"
#include "grePP.hh"
#include "basePP.hh"

#define LLDP_PP

using namespace vigil;
using namespace vigil::container;

namespace ericsson {

	class Packetprocessor : public Component {
        enum PP_TYPES {
            LLDP_TYPE = 1,
            GRE_TYPE  = 2,
        };

        struct ppSkeleton {
            uint32_t proc_id;
            uint32_t type_id;
        };

		private:
			Vlog_module log;

            map<int, BasePP*> localPP_registry;
            map<datapathid, list<ppSkeleton> > networkPP_registry;

            JsonDispatcher *jsonDisp;

		public:
			Packetprocessor(const Context* c, const json_object*) 
                : Component(c), log("packetprocessor") { };

			void configure(const Configuration*);
			void install();

			Disposition join_handle(const Event&);
            Disposition leave_handle(const Event&);
			Disposition ctrl_handle(const Event&);
            Disposition barrier_handle(const Event&);
            Disposition packet_in_handle(const Event&);

        private:
            auto_ptr<json_object> handleJsonTest(const json_dict* json) { return auto_ptr<json_object>(NULL); };

	};
}

#endif
