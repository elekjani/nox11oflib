#ifndef _OPENFLOW_HH_
#define _OPENFLOW_HH_ 1

#include <boost/optional.hpp>

#include "ofp-msg-event.hh"

namespace ericsson {

boost::optional<uint32_t>
getErrorXid(struct ofl_msg_error *msg) {
	if (msg->data_length < sizeof(struct ofp_header)) {
		return boost::optional<uint32_t>();
	}
	struct ofp_header *request = (struct ofp_header *)(msg->data);
	return boost::optional<uint32_t>(ntohl(request->xid));
}


} /* namespace ericsson */

#endif /* _OPENFLOW_HH_ */
