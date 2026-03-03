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

struct state {
  char *program;
};

void *run_menu(void *arg) {
  struct menu *m = (struct menu *)arg;
  int status = menu_run(m);
  if (status == EXIT_FAILURE) {
    set_failure();
  }
  return NULL;
}


lua_fn(menu) {
  luaL_checktype(L, 1, LUA_TTABLE);

  int len = lua_objlen(L, 1);

  struct menu *m = menu_create(set_result);
  m->position = POSITION_CENTER;
  m->font = "FiraCodeNerdFont Mono Regular 18 @wght=400";
  m->lines = len;
  m->minwidth = 400;

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

  menu_destroy(m);

  lua_pushstring(L, result);

  return 1;
}

void addLuaFunctions(lua_State *L) {
  add_lua_fn(menu);
}

int main(int argc, char *argv[]) {
  const char *usage = "Usage: wmenu-lua [-l file.lua]";

  int opt;
  char *file = 0;

  while ((opt = getopt(argc, argv, "f:")) != -1) {
    switch (opt) {
    case 'f':
      file = strdup(optarg);
      break;
    default:
      fprintf(stderr, "%s\n", usage);
      return 1;
    }
  }

  lua_State *L = luaL_newstate();
  luaL_openlibs(L);

  addLuaFunctions(L); 

  // if null, reads from stdin
  if (luaL_dofile(L, file)) {
    fprintf(stderr, "Lua error: %s\n", lua_tostring(L, -1));
  }

  lua_close(L);

  return 0;
}
