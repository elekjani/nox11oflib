#ifndef _DPLINK_HH_
#define _DPLINK_HH_ 1

#include "dpport.hh"

namespace ericsson {

class DpLink {
	/* @@ZED@@ fields should be const, but multimap doesn't like that  */
public:
	DpPort src;
	DpPort dst;

	DpLink() : src(), dst() { };
	DpLink(const DpPort& src_, const DpPort& dst_) :src(src_), dst(dst_) { };
	bool operator<(const DpLink& dplink) const { return (src < dplink.src) || (src == dplink.src && dst < dplink.dst); }
	bool operator==(const DpLink& dplink) const { return src == dplink.src && dst == dplink.dst; }
};

} /* namespace ericsson */

ostream& operator<<(ostream &os,const ericsson::DpLink &link) {
	os << "{src=" << link.src << ", dst=" << link.dst << "}";
	return os;
}

#endif /* _DPLINK_HH_ */
