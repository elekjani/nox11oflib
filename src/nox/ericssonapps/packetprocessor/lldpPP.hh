#ifndef LLDP_PP_HH
#define LLDP_PP_HH

#include <string>
#include <map>

#include <boost/unordered_map.hpp>

#include <lemon/list_graph.h>
#include <lemon/maps.h>

#include "ofp-msg-event.hh"
#include "packets.h"
#include "openflow-default.hh"
#include "vlog.hh"
#include "basePP.hh"

#include "netinet++/ethernet.hh"
#include "netinet++/datapathid.hh"


using namespace vigil;
using namespace lemon;
using namespace std;

namespace ericsson {

enum LldpChassisIdSubtype {
  CST_CHASSIS_COMPONENT = 1,
  CST_INTERFACEALIAS   = 2,
  CST_PORT_COMPONENT    = 3,
  CST_MAC_ADDRESS       = 4,
  CST_NETWORK_ADDRESS   = 5,
  CST_INTERFACE_NAME    = 6,
  CST_LOCAL            = 7,
};

enum LldpPortIdSubtype {
  PST_INTERFACE_ALIAS = 1,
  PST_PORT_COMPONENT  = 2,
  PST_MAC_ADDRESS     = 3,
  PST_NETWORK_ADDRESS = 4,
  PST_INTERFACE_NAME  = 5,
  PST_AGENT_CIRCUIT_ID = 6,
  PST_LOCAL          = 7,
};

enum notifType {
  CHASSIS_INFO = 1,
  PORT_INFO    = 2,
  LINK_INFO    = 3,
};


struct notifData {
  uint8_t status: 4;
  uint8_t notifType: 4;

  size_t length;
};

struct chassisInfo {
  struct notifData notifData;
  enum LldpChassisIdSubtype chassisIdSubtype;
  string chassisId;
  string sysName;
  string sysDesc;
};

struct portInfo {
  struct notifData notifData;
  struct chassisInfo chassis;
  uint16_t    portNumber;
  enum LldpPortIdSubtype portIdSubtype;
  string portId;
  string portDesc;
};

struct linkInfo {
  struct notifData notifData;
  struct portInfo    srcPort;
  struct portInfo    dstPort;
};

enum LldpStatus {
  LLDP_NONE    = 0,
  LLDP_ADDED   = 1,
  LLDP_DELETED = 2,
  LLDP_CHANGED = 3,
};



class LldpPP : public BasePP {
    struct lldp_pp_mod {
      uint16_t lldpMessageTxInterval;
      uint16_t lldpMessageTxHoldMultiplier;
      uint16_t lldpNotificationInterval;
      uint8_t  pad[2];

      uint32_t enabledPorts;
      uint32_t disabledPorts;
    };

  public:
    typedef map<string, ListDigraph::Node>    ChIdToNodeMap;
    typedef ListDigraph::NodeMap<string>      NodeToChIdMap;
    typedef pair<string, string>              PortIdPair;
    typedef ListDigraph::ArcMap<PortIdPair>   ArcToPortIdMap;
    typedef map<PortIdPair, ListDigraph::Arc> PortIdToArcMap;

    typedef map<string, chassisInfo>  ChIdToInfoMap;
    typedef map<PortIdPair, portInfo> PortIdToInfoMap;

    typedef map<string, datapathid>  ChIdToDpIdMap;
    typedef map<datapathid, string>  DpIdToChIdMap;

  private:
    Vlog_module log;

    uint16_t messageTxInterval;
    uint16_t messageTxHoldMultiplier;
    uint16_t notificationInterval;
    uint32_t enabledPorts;
    uint32_t disabledPorts;

    ListDigraph    topo;
    ChIdToNodeMap  chIdToNode;
    NodeToChIdMap  nodeToChId;
    PortIdToArcMap portIdToArc;
    ArcToPortIdMap arcToPortId;
    ArcToPortIdMap arcToDstPortId;

    ChIdToInfoMap   chIdToInfo;
    PortIdToInfoMap portIdToInfo;

    ChIdToDpIdMap chIdToDpId;
    DpIdToChIdMap dpIdToChId;

    const ListDigraph::Node  unknownNode;

    int type_id;

  public:
    LldpPP(int type_id);

    virtual void barrier_handle(const datapathid &dpid, list<struct ofl_msg_flow_mod *> &flow_mod);
    virtual void join_handle(const datapathid &dpid, list<struct ofl_msg_processor_mod *> &proc_mod);
    virtual void ctrl_handle(const datapathid &dpid, uint32_t proc_id, uint8_t *data, size_t data_length);
    virtual void dp_leaved(const datapathid &dpid, uint32_t proc_id);

  private:
    auto_ptr<json_object> handleJsonTopo(const json_dict *json);
    auto_ptr<json_object> handleJsonDump(const json_dict *json);

    void sendJsonDpEvent(ListDigraph::Node, bool);
    void sendJsonLinkEvent(ListDigraph::Arc, bool);

    json_object *chToJson(ListDigraph::Node node);
    json_object *linkToJson(ListDigraph::Arc arc);
    json_object *portToJson(PortIdPair &port);

    void get_chassis_info(struct chassisInfo *chassis, uint8_t *data);
    void get_port_info(struct portInfo *port, uint8_t *data);
    void get_link_info(struct linkInfo *link, uint8_t *data);

    ListDigraph::Node manageNodes(struct chassisInfo *chassis);
    ListDigraph::Arc  managePorts(struct portInfo *port);
    void manageLinks(struct linkInfo *link);
};

} //namespace ericsson

#endif
