#ifndef _TOPOLOGY_HH_
#define _TOPOLOGY_HH_ 1

#include <map>
#include <set>

#include <lemon/list_graph.h>
#include <lemon/maps.h>

#include "component.hh"
#include "vlog.hh"

#include "../jsondispatcher/jsondispatcher.hh"
#include "../common/dpport.hh"
#include "../common/dplink.hh"

using namespace std;
using namespace vigil;
using namespace vigil::container;
using namespace lemon;

namespace ericsson {

#define DEFAULT_PORT_SPEED 1024 /* kbps */

class Topology : public Component {
public:
	class DpPortProps {
	public:
		uint32_t speed; // in kbps
		DpPortProps() : speed(-1) { };
		DpPortProps(uint32_t speed_) : speed(speed_) { };
	};

	class DpLinkProps {
	public:
		uint32_t bandwidth; // in kbps
		DpLinkProps() : bandwidth(0) { };
		DpLinkProps(uint32_t bandwidth_) : bandwidth(bandwidth_) { };
	};

	typedef ListDigraph::NodeMap<datapathid>   NodeToDpidMap;
	typedef ListDigraph::ArcMap<DpLink>        ArcToLinkMap;
	typedef map<datapathid, ListDigraph::Node> DpidToNodeMap;
	typedef map<DpLink, ListDigraph::Arc>      LinkToArcMap;
	typedef multimap<datapathid, DpPort>       DpidToPortsMap;
	typedef multimap<DpPort, DpLink>           PortToLinksMap;
	typedef map<DpPort, DpPortProps>           PortToPropsMap;
	typedef map<DpLink, DpLinkProps>           LinkToPropsMap;

private:

/*
 *  Node                     Arc
 *    ^                       ^
 *    |                       |
 *    v                       v
 *  Dpid  ===>  DpPort ===> DpLink
 *                |           |
 *                v           v
 *           DpPortProps DpLinkProps
 */

	/* topology graph, and maps between elements and ids */
	ListDigraph topo;
	ListDigraph::NodeMap<datapathid> nodeToDpid;
	ListDigraph::ArcMap<DpLink> arcToLink;

	DpidToNodeMap dpidToNode;
	LinkToArcMap linkToArc;

	/* maps for finding an element from a subelement */
	DpidToPortsMap dpidToPorts;
	PortToLinksMap portToLinks;

	/* port and link property maps */
	PortToPropsMap portToProps;
	LinkToPropsMap linkToProps;

	JsonDispatcher* jsonDisp;

	Vlog_module lg;

	void handleLinkAdd(const DpLink& link);
	void handleLinkRemove(const DpLink& link);

	void sendJsonDpEvent(const datapathid& dpid, bool join);
	void sendJsonLinkEvent(const DpLink& link, bool join);

	json_object *dpToJson(const datapathid &dpid);
	json_object *portToJson(const DpPort &port);
	json_object *linkToJson(const DpLink &link);

public:
	Topology(const Context *c, const json_object *json);
	void configure(const Configuration *conf);
	void install();
	static const Component_name component_name();

	Disposition handleDatapathJoin(const Event& e);
	Disposition handleDatapathLeave(const Event& e);
	Disposition handleLinkChange(const Event& e);

	auto_ptr<json_object> handleJsonTopo(const json_dict* json);
	auto_ptr<json_object> handleJsonDump(const json_dict* json);

	const ListDigraph&     getTopo();
	const NodeToDpidMap&   getNodeToDpid();
	const ArcToLinkMap&    getArcToLink();
	const DpidToNodeMap&   getDpidToNode();
	const LinkToArcMap&    getLinkToArc();
	const DpidToPortsMap&  getDpidToPorts();
	const PortToLinksMap&  getPortToLinks();
	const PortToPropsMap&  getPortToProps();
	const LinkToPropsMap&  getLinkToProps();

};

} // namespace ericsson

#endif // _TOPOLOGY_HH_
