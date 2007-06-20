#ifndef _LUA_GAMECONFIG_H
#define _LUA_GAMECONFIG_H

#include <string>
#include <stack>
#include <boost/filesystem/path.hpp>
#include "luascript.h"

class GameConfigScript
{
public:
    GameConfigScript();

    int vfs_add(lua_State* L);
    int vfs_add_path(lua_State* L);
    int add_config(lua_State* L);

    void parse(const std::string& path);

    static LuaScript::Reg<GameConfigScript> lua_methods[];
private:
    LuaScript script;
    std::stack<boost::filesystem::path> current_directory;
};

#endif