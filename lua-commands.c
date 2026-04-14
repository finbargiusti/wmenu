#define _POSIX_C_SOURCE 200809L

#include <luajit-2.1/lua.h>

#include <pango.h>
#include <stdlib.h>
#include <string.h>

#include "lua-commands.h"

#define GLOBAL_OPTS "_t_globalconfig"
#define TEMP_OPTS "_t_fnconfig"

pthread_mutex_t lua_result_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t lua_result_cond = PTHREAD_COND_INITIALIZER;

bool has_result = false;
char *result;

struct menu *menu_opts;

void addLuaFunctions(lua_State *L) {
  add_lua_fn(menu);
  add_lua_fn(config);
}

void throw_option_error(lua_State *L, char *error) {
  char *errorstring = malloc(sizeof(char) * 200);

  sprintf(errorstring, "error parsing options: %s", error);
  lua_pushstring(L, errorstring);
  lua_error(L);
}

void write_menu_opts(struct menu *m) {
  if (menu_opts->font) { // only write if not NULL
    m->font = menu_opts->font;
  }
  if (menu_opts->lines) { // only write if not 0
    m->lines = menu_opts->lines;
  }
}

// boilerplate == bad

#define with_opt_type(opt_name, type, block)                                   \
  if (!strcmp(name, #opt_name)) {                                              \
    if (!lua_is##type(L, -1)) {                                                \
      sprintf(error, "bad type for " #opt_name " , expected " #type);          \
      throw_option_error(L, error);                                            \
      return 1;                                                                \
    }                                                                          \
    do {                                                                       \
      block;                                                                   \
    } while (false);                                                           \
    return 0;                                                                  \
  }

int set_option(lua_State *L, const char *name, struct menu *m) {
  char *error = malloc(sizeof(char) * 150);

  with_opt_type(font, string, {
    const char *font = lua_tostring(L, -1);

    m->font = malloc(sizeof(char) * strlen(font));
    strcpy(m->font, font);
  });

  with_opt_type(lines, number, {
    int lines = (int)lua_tonumber(L, -1); // we round down.. who cares

    m->lines = lines;
  });

  with_opt_type(normalbg, string, {
    const char *color = lua_tostring(L, -1);

    if (!parse_color(color, &m->normalbg)) {
      sprintf(error, "invalid color %s", color);
      throw_option_error(L, error);
      return 1;
    }
  });

  with_opt_type(normalfg, string, {
    const char *color = lua_tostring(L, -1);

    if (!parse_color(color, &m->normalfg)) {
      sprintf(error, "invalid color %s", color);
      throw_option_error(L, error);
      return 1;
    }
  });

  with_opt_type(selectionbg, string, {
    const char *color = lua_tostring(L, -1);

    if (!parse_color(color, &m->selectionbg)) {
      sprintf(error, "invalid color %s", color);
      throw_option_error(L, error);
      return 1;
    }
  });

  with_opt_type(selectionfg, string, {
    const char *color = lua_tostring(L, -1);

    if (!parse_color(color, &m->selectionfg)) {
      sprintf(error, "invalid color %s", color);
      throw_option_error(L, error);
      return 1;
    }
  });

  with_opt_type(promptbg, string, {
    const char *color = lua_tostring(L, -1);

    if (!parse_color(color, &m->promptbg)) {
      sprintf(error, "invalid color %s", color);
      throw_option_error(L, error);
      return 1;
    }
  });

  with_opt_type(promptfg, string, {
    const char *color = lua_tostring(L, -1);

    if (!parse_color(color, &m->promptfg)) {
      sprintf(error, "invalid color %s", color);
      throw_option_error(L, error);
      return 1;
    }
  });

  with_opt_type(prompt, string, {
    const char *prompt = lua_tostring(L, -1);

    m->prompt = malloc(sizeof(char) * strlen(prompt));
    strcpy(m->prompt, prompt);
  });

  with_opt_type(minwidth, number, {
    int minwidth = (int)lua_tonumber(L, -1); // we round down.. who cares

    m->minwidth = minwidth;
  });

  with_opt_type(position, string, {
    const char *position = lua_tostring(L, -1);

    if (!strcmp(position, "bottom")) {
      m->position = POSITION_BOTTOM;
    } else if (!strcmp(position, "top")) {
      m->position = POSITION_TOP;
    } else if (!strcmp(position, "center") ) {
      m->position = POSITION_CENTER;
    }
  });

  sprintf(error, "unrecognized option: %s", name);
  throw_option_error(L, error);

  return 1;
}

void add_config_to_require_path(lua_State *L, char *config_dir) {
  lua_getglobal(L, "package");
  lua_getfield(L, -1, "path");
  const char *path_before = lua_tostring(L, -1);
  lua_pop(L, 1);
  size_t new_path_len = strlen(config_dir) + strlen(path_before) + 8;
  char *new_path = malloc(sizeof(char) * new_path_len);
  sprintf(new_path, "%s;%s/?.lua", path_before, config_dir);
  lua_pushstring(L, new_path);
  lua_setfield(L, -2, "path");
  lua_pop(L, 1); // returning to original state
}

void table_merge_top(lua_State *L) {
  int dest = lua_gettop(L) - 1;
  int src = lua_gettop(L);
  lua_pushnil(L); /* first key for lua_next */
  while (lua_next(L, src) != 0) {
    lua_pushvalue(L, -2); /* copy key */
    lua_pushvalue(L, -2); /* copy value */
    lua_settable(L, dest);
    lua_pop(L, 1);
  }
}

void write_config(lua_State *L, struct menu *m) {
  lua_getglobal(L, GLOBAL_OPTS);
  lua_getglobal(L, TEMP_OPTS);
  bool has_global = lua_istable(L, -2);
  bool has_temp = lua_istable(L, -1);

  debug("has global: %b, has temp: %b\n", has_global, has_temp);

  if (!has_global && !has_temp) {
    lua_pop(L, 2);
    return;
  }

  if (!has_temp) {
    lua_pop(L, 1); // get rid of the temp table
  } else if (!has_global) {
    lua_remove(L, -2);
  } else {
    lua_newtable(L);
    lua_pushvalue(L, -3);
    table_merge_top(L);
    lua_pop(L, 1);
    lua_pushvalue(L, -2);
    table_merge_top(L);
    lua_pop(L, 1);
    lua_remove(L, -2);
    lua_remove(L, -2);
  }

  debug("%d\n", lua_gettop(L));

  lua_pushnil(L);
  while (lua_next(L, -2)) {
    lua_pushvalue(L, -2);
    const char *key = lua_tostring(L, -1);
    debug("do option: %s\n", key)
    lua_pop(L, 1);
    if (set_option(L, key, m)) {
      return;
    }
    lua_pop(L, 1);
  }
  lua_pop(L, 1);
  return;
}

lua_fn(config) {
  luaL_checktype(L, 1, LUA_TTABLE);

  lua_getglobal(L, GLOBAL_OPTS);
  if (lua_istable(L, -1)) {
    lua_getglobal(L, GLOBAL_OPTS);
    lua_pushvalue(L, 1);
    table_merge_top(L);
    lua_pop(L, 1);
  } else {
    lua_pushvalue(L, 1);
  }

  lua_setglobal(L, GLOBAL_OPTS);
  lua_pop(L, 1);

  return 0;
}

void return_result(struct menu *m, char *r, bool exit) {
  pthread_mutex_lock(&lua_result_lock);
  result = strdup(r);
  has_result = true;
  m->exit = true;
  pthread_cond_signal(&lua_result_cond);
  pthread_mutex_unlock(&lua_result_lock);
}

void return_failure() {
  pthread_mutex_lock(&lua_result_lock);
  result = NULL;
  has_result = true;
  pthread_cond_signal(&lua_result_cond);
  pthread_mutex_unlock(&lua_result_lock);
}

void *run_menu(void *arg) {
  struct menu *m = (struct menu *)arg;
  int status = menu_run(m);
  if (status == EXIT_FAILURE) {
    return_failure();
  }
  menu_destroy(m);
  return NULL;
}

lua_fn(menu) {
  luaL_checktype(L, 1, LUA_TTABLE);

  // allow options to be passed as 2nd parameter
  if (lua_istable(L, 2)) {
    lua_pushvalue(L, 2);
  } else {
    lua_pushnil(L);
  }
  lua_setglobal(L, TEMP_OPTS);

  lua_pushvalue(L, 1); // push table onto stack

  int len = lua_objlen(L, -1);

  struct menu *m = menu_create(return_result);
  m->position = POSITION_CENTER;

  write_config(L, m);

  int height = get_font_height(m->font);
  m->line_height = height + 2;
  m->height = m->line_height;
  if (m->lines > 0) {
    m->height += m->height * m->lines;
  }
  m->padding = height / 2;

  m->items = malloc(sizeof (struct item) * len);
  for (int i = 0; i < len; i++) {
    lua_rawgeti(L, -1 * (i + 1), (i + 1));
    luaL_checktype(L, -1, LUA_TSTRING);
    const char *item = lua_tostring(L, -1);
    unsafe_menu_add_item_no_realloc(m, strdup(item));
  }

  pthread_t tid;
  pthread_create(&tid, NULL, run_menu, m);

  pthread_mutex_lock(&lua_result_lock);
  while (!has_result) {
    pthread_cond_wait(&lua_result_cond, &lua_result_lock);
  }
  pthread_mutex_unlock(&lua_result_lock);

  has_result = false;

  lua_pushstring(L, result);

  return 1;
}
