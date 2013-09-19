#pragma once

#include <boost/tuple/tuple.hpp>

#include <nscapi/nscapi_plugin_interface.hpp>

#include <check_mk/data.hpp>
#include <check_mk/server/server_handler.hpp>
#include <check_mk/lua/lua_check_mk.hpp>

class handler_impl : public check_mk::server::handler {
	bool allowArgs_;
	boost::shared_ptr<scripts::script_manager<lua::lua_traits> > scripts_;
public:
	handler_impl(boost::shared_ptr<scripts::script_manager<lua::lua_traits> > scripts) : allowArgs_(false), scripts_(scripts) {}

	virtual void set_allow_arguments(bool v)  {
		allowArgs_ = v;
	}

	check_mk::packet process();

	virtual void log_debug(std::string module, std::string file, int line, std::string msg) const {
		if (GET_CORE()->should_log(NSCAPI::log_level::debug)) {
			GET_CORE()->log(NSCAPI::log_level::debug, file, line, msg);
		}
	}
	virtual void log_error(std::string module, std::string file, int line, std::string msg) const {
		if (GET_CORE()->should_log(NSCAPI::log_level::error)) {
			GET_CORE()->log(NSCAPI::log_level::error, file, line, msg);
		}
	}
};
