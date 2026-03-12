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
#include "lua-commands.h"

int main(int argc, char *argv[]) {
  const char *usage = 
    "Usage: wmenu-lua [-c path/to/config] <menu name>\n"
    "Be default, looks for config in $HOME/.config/wmenu-lua/"
  ;

  int ret = 0;

  char *config_dir = malloc(sizeof (char) * 200);

  char *xdg_config_home = getenv("HOME");

  if (xdg_config_home == NULL) {
    fprintf(stderr, "$HOME is not set!\n");
    return 1;
  }

  sprintf(config_dir, "%s/.config/wmenu-lua", xdg_config_home);

  int opt;
  while ((opt = getopt(argc, argv, "c:")) != -1) {
    if (opt == 'c') {
      config_dir = optarg;
    } else {
      printf("%s\n", usage);
      return 1;
    }
  }
  
  if (optind == -1 || argv[optind] == NULL) {
    printf("%s\n", usage);
    return 1;
  }

  char *file_path = malloc(sizeof (char) * 200);

  sprintf(file_path, "%s/%s.lua", config_dir, argv[optind]);

  menu_opts = calloc(1, sizeof(struct menu));

  lua_State *L = luaL_newstate();

  luaL_openlibs(L);

  addLuaFunctions(L); 

  add_config_to_require_path(L, config_dir);

  // if null, reads from stdin
  if (luaL_dofile(L, file_path)) {
    fprintf(stderr, "Lua error: %s\n", lua_tostring(L, -1));
  }

  if (!lua_isfunction(L, -1)) {
    ret = 1;
    fprintf(stderr, "wmenu-lua: expect function as return type\n");
    goto quit;
  }

  lua_pcall(L, 0, LUA_MULTRET, 0);

  if(lua_isstring(L, -1)) {
    const char *s = lua_tostring(L, -1);

    printf("%s\n", s);
  }

quit:
  lua_close(L);

  return ret;
}
