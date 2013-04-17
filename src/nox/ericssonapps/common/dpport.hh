#ifndef _DPPORT_HH_
#define _DPPORT_HH_ 1

#include <ostream>

#include "netinet++/datapathid.hh"

using namespace std;
using namespace vigil;

namespace ericsson {

class DpPort {
		/* @@ZED@@ fields should be const, but multimap doesn't like that  */
	public:
		datapathid dpid;
		uint32_t port;

		DpPort() : dpid(), port(0) { };
		DpPort(const datapathid& dpid_, uint32_t port_) : dpid(dpid_), port(port_) { };
		bool operator<(const DpPort& dpport) const { return (dpid < dpport.dpid) || (dpid == dpport.dpid && port < dpport.port); }
		bool operator==(const DpPort& dpport) const { return dpid == dpport.dpid && port == dpport.port; }

};


} /* namespace ericsson */



ostream& operator<<(ostream &os,const ericsson::DpPort &port) {
    os << "{dpid=" << port.dpid << ", port=" << port.port << "}";
    return os;
}

#endif /* _DPPORT_HH_ */
