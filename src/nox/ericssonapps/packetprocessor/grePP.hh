#ifndef GRE_PP_HH
#define GRE_PP_HH

#define IP_ADDR(i1,i2,i3,i4) (i1 << 24) + (i2 << 16) + (i3 << 8) + i4

#include "ofp-msg-event.hh"
#include "packets.h"
#include "openflow-default.hh"
#include "basePP.hh"
#include "vlog.hh"

using namespace vigil;

namespace ericsson {
	struct gre_pp_mod {
		uint32_t local_ip;
		uint32_t local_tunnel_ip;
		uint32_t remote_ip;
		uint32_t remote_tunnel_ip;
		uint32_t interface;
	};

	class GrePP : public BasePP {
        public:
            static const int GRE_TYPE = 2;

		private:
            Vlog_module log;

			uint32_t remote_ip;
			uint32_t local_ip;
			uint64_t remote_dpid;
			uint64_t local_dpid;
			uint32_t remote_tunnel_ip;
			uint32_t local_tunnel_ip;
			uint32_t remote_interface;
			uint32_t local_interface;

            int      type_id;

        public:
            GrePP(int type_id);

            virtual void barrier_handle(const datapathid &dpid, list<struct ofl_msg_flow_mod*> &flow_mod);
            virtual void join_handle(const datapathid &dpid, list<struct ofl_msg_processor_mod*> &proc_mod);
            virtual void ctrl_handle(const datapathid &dpid, uint32_t proc_id, uint8_t *data, size_t data_length) { };
            virtual void dp_leaved(const datapathid &dpid, uint32_t proc_id);
	};

} //namespace ericsson

#endif
