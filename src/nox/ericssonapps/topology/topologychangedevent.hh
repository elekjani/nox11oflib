#ifndef _TOPOLOGYCHANGED_EVENT_HH_
#define _TOPOLOGYCHANGED_EVENT_HH_

#include "event.hh"

namespace ericsson {

class TopologyChangedEvent : public Event {
public:
	TopologyChangedEvent() : Event(static_get_name()) { };

    static const Event_name static_get_name() {
        return "Topology_changed_event";
    }
};

} /* namespace ericsson */

#endif /* _TOPOLOGYCHANGED_EVENT_HH_ */
