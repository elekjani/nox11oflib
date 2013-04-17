#ifndef _JSONDISPATCHER_HH_
#define _JSONDISPATCHER_HH_ 1

#include <map>
#include <set>

#include <boost/smart_ptr.hpp>

#include "component.hh"
#include "coreapps/messenger/messenger_core.hh"
#include "vlog.hh"

using namespace std;
using namespace boost;
using namespace vigil;
using namespace vigil::container;

namespace ericsson {

typedef auto_ptr<json_object> JsonDispatcher_signature(const json_dict* json);
typedef function<JsonDispatcher_signature> JsonDispatcher_handler;

class JsonDispatcher : public Component {
private:
	typedef map<string, JsonDispatcher_handler> HandlerMap;
	typedef map<string, HandlerMap> ModuleMap;
    set<Msg_stream *> jsonSockets;
	ModuleMap modules;
	Vlog_module lg;
public:
	JsonDispatcher(const Context *c, const json_object *json);
	void configure(const Configuration *conf);
	void install();

	void registerHandler(const string module, const string message, const JsonDispatcher_handler& handler);
	bool send(auto_ptr<json_object> message);
	bool isConnected();

	static json_object *jsonInit(const string& module, const string& message);
	static json_object *jsonString(const string& str);
	static json_object *jsonString(const char *str);
	static json_object *jsonInt(int i);

	Disposition handleJson(const Event& e);
};

} // namespace ericsson


#endif // _JSONDISPATCHER_HH_
