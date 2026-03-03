#include "lua.h"
#define _POSIX_C_SOURCE 200809L

#include <luajit-2.1/lauxlib.h>
#include <luajit-2.1/lualib.h>
#include <luajit.h>
#include <stdio.h>
#include <string.h>
#include <pango.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#include "menu.h"
#include "wayland.h"

#define add_lua_fn(name) \
  lua_pushcfunction(L, &c_##name);\
  lua_setglobal(L, #name);

#define lua_fn(name) \
  int c_##name(lua_State *L)
  

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
bool has_result = false;
char *result;

void set_result(struct menu *m, char *r, bool exit) {
  pthread_mutex_lock(&lock);
  result = strdup(r);
  has_result = true;
  m->exit = true;
  pthread_cond_signal(&cond);
  pthread_mutex_unlock(&lock);
}

void set_failure() {
  pthread_mutex_lock(&lock);
  result = NULL;
  has_result = true;
  pthread_cond_signal(&cond);
  pthread_mutex_unlock(&lock);
}

struct menu *menu_opts;

void *run_menu(void *arg) {
  struct menu *m = (struct menu *)arg;
  int status = menu_run(m);
  if (status == EXIT_FAILURE) {
    set_failure();
  }
  return NULL;
}

void throw_option_error(lua_State *L, char *error)  {
  char *errorstring = malloc(sizeof( char) * 200);

  sprintf(errorstring, "error parsing options: %s", error);
  lua_pushstring(L, errorstring); 
  lua_error(L);
}

int set_option(lua_State *L, const char *name) {
  char *error = malloc(sizeof (char) * 150);

  if (!strcmp(name, "font")) {
    if (!lua_isstring(L, -1)) {
      sprintf(error, "bad type for font, expeced string");
      throw_option_error(L, error);
    }

    const char *font = lua_tostring(L, -1);

    int font_len = strlen(font);

    char *option = malloc(sizeof (char) * font_len);

    menu_opts->font = strcpy(option, font);

    return 0;
  }

  if (!strcmp(name, "lines")) {
    if (!lua_isnumber(L, -1)) {
      sprintf(error, "bad type for lines, expeced number");
      throw_option_error(L, error);
    }

    int lines = (int) lua_tonumber(L, -1); // we round down.. who cares
    
    menu_opts->lines = lines;

    return 0;
  }

  sprintf(error, "unrecognized option: %s", name);
  throw_option_error(L, error);

  return 1;
}

lua_fn(config) {
  luaL_checktype(L, 1, LUA_TTABLE);

  lua_pushnil(L); 

  while (lua_next(L, -2)) {
    lua_pushvalue(L, -2);

    const char *key = lua_tostring(L, -1);

    lua_pop(L, 1);

    if (set_option(L, key)) {
      return 1;
    }

    lua_pop(L, 1);
  }

  return 0;
}

void write_menu_opts(struct menu *m) {
  if (menu_opts->font) { // only write if not NULL
    m->font = menu_opts->font;
  }
  if (menu_opts->lines) { // only write if not 0
    m->lines = menu_opts->lines;
  }
}

lua_fn(menu) {
  luaL_checktype(L, 1, LUA_TTABLE);

  int len = lua_objlen(L, 1);

  struct menu *m = menu_create(set_result);
  m->position = POSITION_CENTER;
  m->minwidth = 400;
  write_menu_opts(m);

  int height = get_font_height(m->font);
  m->line_height = height + 2;
  m->height = m->line_height;
  if (m->lines > 0) {
	m->height += m->height * m->lines;
  }
  m->padding = height / 2;

  for (int i = 0; i < len; i++) {
    lua_rawgeti(L, -1 * (i + 1), (i + 1));
    luaL_checktype(L, -1, LUA_TSTRING);
    const char *item = lua_tostring(L, -1);
    menu_add_item(m, strdup(item));
  }

  pthread_t tid;
  pthread_create(&tid, NULL, run_menu, m);

  pthread_mutex_lock(&lock);
  while (!has_result) {
    pthread_cond_wait(&cond, &lock);
  }
  pthread_mutex_unlock(&lock);

  has_result = false;

  menu_destroy(m);

  lua_pushstring(L, result);

  return 1;
}

void addLuaFunctions(lua_State *L) {
  add_lua_fn(menu);
  add_lua_fn(config);
}

int main(int argc, char *argv[]) {
  const char *usage = "Usage: wmenu-lua file.lua";
  int ret = 0;

  if (argc != 2) {
    printf("%s\n", usage);
    return 1;
  }

  char *file = argv[1];

  menu_opts = calloc(1, sizeof(struct menu));

  lua_State *L = luaL_newstate();
  luaL_openlibs(L);

  addLuaFunctions(L); 

  // if null, reads from stdin
  if (luaL_dofile(L, file)) {
    fprintf(stderr, "Lua error: %s\n", lua_tostring(L, -1));
  }

  if (!lua_istable(L, -1)) {
    ret = 1;
    fprintf(stderr, "wmenu-lua: expected table as return value from input\n");
    goto quit;
  }

  lua_getfield(L, -1, "run");

  if (!lua_isfunction(L, -1)) {
    ret = 1;
    fprintf(stderr, "wmenu-lua: table should contain instance method run\n");
    goto quit;
  }

  lua_pushvalue(L, -2);

  lua_pcall(L, 1, LUA_MULTRET, 0);

  if(lua_isstring(L, -1)) {
    const char *s = lua_tostring(L, -1);

    printf("%s\n", s);
  }

quit:
  lua_close(L);

  return ret;
}
