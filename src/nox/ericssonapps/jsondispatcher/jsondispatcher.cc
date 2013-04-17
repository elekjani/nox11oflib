#include "jsondispatcher.hh"

#include <boost/bind.hpp>

#include "assert.hh"

//events
#include "coreapps/messenger/jsonmessenger.hh"


using namespace std;
using namespace vigil;
using namespace vigil::container;

namespace ericsson {

JsonDispatcher::JsonDispatcher(const Context *c, const json_object *json) : Component(c),
		jsonSockets(), modules(), lg("jsondp")
	{ };

void
JsonDispatcher::configure(const Configuration *conf) { };

void
JsonDispatcher::install() {
	register_handler(JSONMsg_event::static_get_name(), boost::bind(&JsonDispatcher::handleJson, this, _1));
};

void
JsonDispatcher::registerHandler(const string module, const string message, const JsonDispatcher_handler& handler) {
	lg.dbg("Registering module \"%s\" for message \"%s\".", module.c_str(), message.c_str());
	modules[module][message] = handler;
}

json_object *
JsonDispatcher::jsonInit(const string& module, const string& message) {
    json_object *msg = new json_object(json_object::JSONT_DICT);
    json_dict *msgDict = new json_dict();
    msg->object = msgDict;

    msgDict->insert(make_pair("type",    JsonDispatcher::jsonString(module)));
    msgDict->insert(make_pair("message", JsonDispatcher::jsonString(message)));

    return msg;
}

json_object *
JsonDispatcher::jsonString(const string& str) {
    json_object *json = new json_object(json_object::JSONT_STRING);
    json->object = new string(str);
    return json;
}

json_object *
JsonDispatcher::jsonString(const char *str) {
    json_object *json = new json_object(json_object::JSONT_STRING);
    json->object = new string(str);
    return json;
}

json_object *
JsonDispatcher::jsonInt(int i) {
    json_object *json = new json_object(json_object::JSONT_INTEGER);
    json->object = new int(i);
    return json;
}


Disposition
JsonDispatcher::handleJson(const Event& e) {
	const JSONMsg_event& json = assert_cast<const JSONMsg_event&>(e);

	lg.dbg("handleJson"); /* @@ZED@@ info */

    /* Filter messages that are not a dictionary */
    if (json.jsonobj->type != json_object::JSONT_DICT) {
    	lg.dbg("json message is not a dict type.");
    	return CONTINUE;
    }

    /* Filter messages that do not have a "type" field with a string value */
    json_dict* jodict = (json_dict*) json.jsonobj->object;
    json_dict::iterator typeIt = jodict->find("type");
    if (typeIt == jodict->end() || typeIt->second->type != json_object::JSONT_STRING) {
    	lg.dbg("json message has no type field.");
    	return CONTINUE;
    }

    string& type = *((string *) typeIt->second->object);

	/* {"type": "disconnect"} is sent by NOX when a json socket is closed */
    if (type == "disconnect") {
    	set<Msg_stream *>::iterator it = jsonSockets.find(json.sock);
    	if (it != jsonSockets.end()) {
            lg.dbg("json connection lost.");
    		jsonSockets.erase(it);
        } else {
        	lg.dbg("json disconnect message.");
        }
        return CONTINUE;
    }

    ModuleMap::iterator module = modules.find(type);
    if (module == modules.end()) {
		lg.dbg("json message was not destined for a registered module.");
		return CONTINUE;
	}

	/* Filter messages that do not have a "message" field with a string value */
    json_dict::iterator msgIt = jodict->find("message");
    if (msgIt == jodict->end() || msgIt->second->type != json_object::JSONT_STRING) {
    	lg.dbg("json message had no message field");
    	return CONTINUE;
    }

    string& message = *((string *) msgIt->second->object);

    /* store connection */
    if (jsonSockets.find(json.sock) == jsonSockets.end()) {
    	lg.dbg("json connection established.");
    	jsonSockets.insert(json.sock);
    }

    HandlerMap::iterator handler = module->second.find(message);

    if (handler == module->second.end()) {
    	lg.dbg("json message was not destined for a registered handler.");
    	return CONTINUE;
    }

    /* call handler */
    auto_ptr<json_object> ret = handler->second(jodict);
    /* @@ZED@@ send reply only to requester, instead of broadcasting! */
    send(ret);

    return STOP;
}

bool
JsonDispatcher::send(auto_ptr<json_object> msg) {
	if (msg.get() == NULL) {
		return false;
	}
	if (!isConnected()) {
		return false;
	}

	string msgStr = msg->get_string();
	size_t msgSize = msgStr.size();

	boost::shared_array<uint8_t> msgSizeArr(new uint8_t[2]);
	msgSizeArr.get()[0] = msgSize >> 8 & 0xff;
	msgSizeArr.get()[1] = msgSize & 0xff;

	// send message size in two bytes first
	for (set<Msg_stream *>::iterator it = jsonSockets.begin(); it != jsonSockets.end(); ++it) {
		(*it)->send(msgSizeArr, 2);
		(*it)->send(msgStr);
	}

	return true;
}

bool
JsonDispatcher::isConnected() {
	return !jsonSockets.empty();
}

REGISTER_COMPONENT(Simple_component_factory<JsonDispatcher>, JsonDispatcher);

} // namespace ericsson

