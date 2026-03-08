#ifndef LUA_COMMANDS_H
#define LUA_COMMANDS_H

#include "lua.h"

#include <luajit-2.1/lauxlib.h>
#include <luajit-2.1/lualib.h>
#include <pthread.h>

#include "menu.h"
#include "wayland.h"

#define add_lua_fn(name) \
  lua_pushcfunction(L, &lua__##name);\
  lua_setglobal(L, #name);

#define lua_fn(name) \
  int lua__##name(lua_State *L)

// menu management from lua

extern struct menu *menu_opts;

void write_menu_opts(struct menu *m);

// Lua utilities

void add_config_to_require_path(lua_State *L, char *config_dir);

// thread mgmt

extern pthread_mutex_t lua_result_lock;
extern pthread_cond_t lua_result_cond;

extern bool has_result;
extern char *result;

// lua functions

lua_fn(config);
lua_fn(menu);

void addLuaFunctions(lua_State *L);

#endif
