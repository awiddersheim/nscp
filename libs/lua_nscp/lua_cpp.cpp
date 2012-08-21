#include <lua/lua_cpp.hpp>

extern "C" {
#include "lauxlib.h"
#include "lualib.h"
}
#include "luna.h"

#include <strEx.h>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_plugin_wrapper.hpp>

lua::Lua_State::Lua_State() : L(lua_open()) { }
lua::Lua_State::~Lua_State() {
	lua_close(L);
}
lua_State* lua::Lua_State::get_state() const {
	lua_gettop(L);
	return L;
}


int lua::lua_wrapper::append_path(const std::string &path) {
	lua_getglobal(L, "package");
	lua_getfield(L, -1, "path");
	std::string cur_path = lua_tostring(L, -1);
	cur_path.append(";");
	cur_path.append(path);
	lua_pop(L, 1);
	lua_pushstring(L, cur_path.c_str());
	lua_setfield(L, -2, "path");
	lua_pop(L, 1);
	return 0;
}


std::string lua::lua_wrapper::get_string(int pos) {
	std::string ret;
	if (get_string(ret, pos))
		return ret;
	return "<NOT_A_STRING:" + strEx::s::xtos(type(pos)) + ">";
}
bool lua::lua_wrapper::get_string(std::string &str, int pos) {
	if (pos == -1)
		pos = lua_gettop(L);
	if (pos == 0)
		return false;
	if (is_string(pos))
		str = utf8::cvt<std::string>(lua_tostring(L, pos));
	else if (is_number(pos))
		str = strEx::s::xtos(lua_tonumber(L, pos));
	else
		return false;
	return true;
}
int lua::lua_wrapper::get_int(int pos) {
	if (pos == -1)
		pos = lua_gettop(L);
	if (pos == 0)
		return 0;
	if (is_string(pos))
		return strEx::s::stox<int>(lua_tostring(L, pos));
	if (is_number(pos))
		return lua_tonumber(L, pos);
	return 0;
}
bool lua::lua_wrapper::get_boolean(int pos) {
	if (pos == -1)
		pos = lua_gettop(L);
	if (pos == 0)
		return false;
	if (is_boolean(pos))
		return lua_toboolean(L, pos);
	if (is_number(pos))
		return lua_tonumber(L, pos)==1;
	return false;
}


NSCAPI::nagiosReturn lua::lua_wrapper::get_code(int pos) {
	std::string str;
	if (pos == -1)
		pos = lua_gettop(L);
	if (pos == 0)
		return NSCAPI::returnUNKNOWN;
	switch (lua_type(L, pos)) 
	{
	case LUA_TNUMBER: 
		return static_cast<int>(lua_tonumber(L, pos));
	case LUA_TSTRING:
		return string_to_code(lua_tostring(L, pos));
	case LUA_TBOOLEAN:
		return lua_toboolean(L, pos)?NSCAPI::returnOK:NSCAPI::returnCRIT;
	}
	NSC_LOG_ERROR_STD(_T("Incorrect type: should be error, ok, warning or unknown: ") + strEx::itos(lua_type(L, pos)));
	return NSCAPI::returnUNKNOWN;
}

std::list<std::string> lua::lua_wrapper::get_array(const int pos) {
	std::list<std::string> ret;
	const int len = lua_objlen(L, pos);
	for ( int i = 1; i <= len; ++i ) {
		lua_pushinteger(L, i);
		lua_gettable(L, -2);
		ret.push_back(get_string(-1));
		pop();
	}
	return ret;
}






bool lua::lua_wrapper::pop_boolean() {
	int pos = lua_gettop(L);
	if (pos == 0)
		return false;
	NSCAPI::nagiosReturn ret = get_boolean(pos);
	lua_pop(L, 1);
	return ret;
}
std::string lua::lua_wrapper::pop_string() {
	std::string ret;
	int top = lua_gettop(L);
	if (top == 0)
		return "<EMPTY>";
	ret = get_string(top);
	pop();
	return ret;
}
bool lua::lua_wrapper::pop_string(std::string &str) {
	int top = lua_gettop(L);
	if (top == 0)
		return false;
	if (!get_string(str, top))
		return false;
	pop();
	return true;
}
bool lua::lua_wrapper::pop_function_ref(int &funref) {
	int top = lua_gettop(L);
	if (top == 0)
		return false;
	if (!is_function(top))
		return false;
	funref = luaL_ref(L, LUA_REGISTRYINDEX);
	if (funref == 0)
		return false;
	return true;
}
bool lua::lua_wrapper::pop_instance_ref(int &funref) {
	int top = lua_gettop(L);
	if (top == 0)
		return false;
	funref = luaL_ref(L, LUA_REGISTRYINDEX);
	if (funref == 0)
		return false;
	return true;
}
int lua::lua_wrapper::pop_int() {
	int ret;
	int top = lua_gettop(L);
	if (top == 0)
		return 0;
	ret = get_int(top);
	pop();
	return ret;
}
NSCAPI::nagiosReturn lua::lua_wrapper::pop_code() {
	int pos = lua_gettop(L);
	if (pos == 0)
		return NSCAPI::returnUNKNOWN;
	NSCAPI::nagiosReturn ret = get_code(pos);
	pop();
	return ret;
}
std::list<std::string> lua::lua_wrapper::pop_array() {
	std::list<std::string> ret;
	int pos = lua_gettop(L);
	if (pos == 0)
		return ret;
	ret = get_array(pos);
	pop();
	return ret;
}



NSCAPI::nagiosReturn lua::lua_wrapper::string_to_code(std::string str) {
	if ((str == "critical")||(str == "crit")||(str == "error")) {
		return NSCAPI::returnCRIT;
	} else if ((str == "warning")||(str == "warn")) {
		return NSCAPI::returnWARN;
	} else if (str == "ok") {
		return NSCAPI::returnOK;
	} else if (str == "unknown") {
		return NSCAPI::returnUNKNOWN;
	}
	NSC_LOG_ERROR_STD(_T("Invalid code: ") + utf8::to_unicode(str));
	return NSCAPI::returnUNKNOWN;
}


int lua::lua_wrapper::type(int pos) {
	if (pos == -1)
		pos = lua_gettop(L);
	if (pos == 0)
		return LUA_TNIL;
	int type = lua_type(L, pos);
	return type;
}
std::string lua::lua_wrapper::get_type_as_string(int pos) {
	if (pos == -1)
		pos = lua_gettop(L);
	if (pos == 0)
		return "<EMPTY>";
	switch (lua_type(L, pos)) 
	{
	case LUA_TNUMBER: 
		return "<NUMBER>";
	case LUA_TSTRING:
		return "<STRING>";
	case LUA_TBOOLEAN:
		return "<TABLE>";
	case LUA_TLIGHTUSERDATA:
		return "<LIGHTUSERDATA>";
	case LUA_TTABLE:
		return "<TABLE>";
	}
	return "<UNKNOWN>";
}


void lua::lua_wrapper::push_code(NSCAPI::nagiosReturn code) {
	if (code == NSCAPI::returnOK)
		lua_pushstring(L, "ok");
	else if (code == NSCAPI::returnWARN)
		lua_pushstring(L, "warning");
	else if (code == NSCAPI::returnCRIT)
		lua_pushstring(L, "critical");
	else
		lua_pushstring(L, "unknown");
}
void lua::lua_wrapper::push_string(std::string s) {
	lua_pushstring(L, s.c_str());
}
void lua::lua_wrapper::push_boolean(bool b) {
	lua_pushboolean(L, b?TRUE:FALSE);
}
void lua::lua_wrapper::push_int(int b) {
	lua_pushinteger(L, b);
}
void lua::lua_wrapper::push_raw_string(std::string s) {
	lua_pushlstring(L, s.c_str(), s.size());
}
void lua::lua_wrapper::push_array(const std::list<std::string> &arr) {
	lua_createtable(L, 0, arr.size());
	int i=0;
	BOOST_FOREACH(const std::string &s, arr) {
		lua_pushnumber(L,i++);
		lua_pushstring(L,s.c_str());
		lua_settable(L,-3);
	}
}
int lua::lua_wrapper::size() {
	return lua_gettop(L);
}
bool lua::lua_wrapper::empty() {
	return size() == 0;
}

void lua::lua_wrapper::assert_lua_return(int expected_size) {
	int real_size = size();
	if (real_size != expected_size) {
		NSC_LOG_ERROR_STD(_T("Invalid return size: ") + strEx::itos(expected_size) + _T(" != ") + strEx::itos(real_size))
	}
}

void lua::lua_wrapper::log_stack() {
	int args = size();
	NSC_DEBUG_MSG_STD(_T("Invalid lua stack state, dumping stack"));
	for (int i=1;i<args+1;i++) {
		NSC_DEBUG_MSG_STD(lua::s2w(get_type_as_string(i) +": " + get_string(i)));
	}
}

int lua::lua_wrapper::error(std::string s) {
	NSC_LOG_ERROR_STD(_T("Lua raised an error: ") + utf8::cvt<std::wstring>(s));
	return luaL_error(L, s.c_str());
}

lua::lua_wrapper::stack_trace lua::lua_wrapper::get_stack_trace(int level) {
	lua_Debug ar;
	if (lua_getstack(L, level, &ar)) {  /* check function at level */
		lua_getinfo(L, "Sl", &ar);  /* get info about it */
		if (ar.currentline > 0) {  /* is there info? */
			return stack_trace(ar.short_src, ar.currentline);
		}
	}
	return stack_trace("unknown",0);
}

std::string lua::lua_wrapper::dump_stack() {
	std::string ret;
	int len = size();
	while (!empty()) {
		if (!ret.empty())
			ret += ", ";
		int t = type(-1);
		if (t == LUA_TSTRING || t == LUA_TNUMBER) {
			ret += pop_string();
		} else if (t == LUA_TTABLE) {
			std::list<std::string> list = pop_array();
			ret += "<" + strEx::s::xtos(list.size()) + ">[";
			BOOST_FOREACH(std::string s, list) {
				ret += s + ", ";
			}
			ret += "]";
		} else {
			ret += "UNKNOWN:" + strEx::s::xtos(t);
			pop();
		}
	}
	return "stack(" + strEx::s::xtos(len) + "): " + ret;
}

void lua::lua_wrapper::openlibs() {
	luaL_openlibs(L);
}

int lua::lua_wrapper::loadfile(std::string script) {
	return luaL_loadfile(L, script.c_str());
}

int lua::lua_wrapper::pcall(int nargs, int nresults, int errfunc) {
	return lua_pcall(L, nargs, nresults, errfunc);
}


std::string lua::lua_wrapper::op_string(int pos, std::string def) {
	return luaL_optstring(L, pos, def.c_str());
}
std::string lua::lua_wrapper::check_string(int pos) {
	return luaL_checkstring(L, pos);
}

std::list<std::string> lua::lua_wrapper::check_array(int pos) {
	luaL_checktype(L, pos, LUA_TTABLE);
	return get_array(pos);
}

bool lua::lua_wrapper::check_bool(int pos) {
	return lua_toboolean(L, pos);
}
int lua::lua_wrapper::op_int(int pos, int def) {
	return luaL_optinteger(L, pos, def);
}
int lua::lua_wrapper::checkint(int pos) {
	return luaL_checkint(L, pos);
}
int lua::lua_wrapper::gc(int what, int data)
{
	return lua_gc(L, what, data);
}





std::string lua::w2s(std::wstring s) {
	return utf8::cvt<std::string>(s);
}
std::wstring lua::s2w(std::string s) {
	return utf8::cvt<std::wstring>(s);
}


void lua::lua_wrapper::remove_userdata(std::string id) {
	lua_pushstring(L, id.c_str());
	lua_pushnil(L);
	lua_settable(L, LUA_REGISTRYINDEX);
}

void lua::lua_wrapper::set_raw_userdata(std::string id, void* data) {
	lua_pushstring(L, id.c_str());
	lua_pushlightuserdata(L, data);
	lua_settable(L, LUA_REGISTRYINDEX);
}

void* lua::lua_wrapper::get_raw_userdata(std::string id) {
	lua_pushstring(L, id.c_str());
	lua_gettable(L, LUA_REGISTRYINDEX);
	void* ret = lua_touserdata(L, -1);
	pop();
	return ret;
}