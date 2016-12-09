#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include "eon.h"

void load_plugin_api(lua_State *luaMain);


int _editor_register_cmd_fn(editor_t* editor, char* name, int (*func)(cmd_context_t* ctx)) {
  return 0;
}

int _editor_init_kmap_by_str(editor_t* editor, kmap_t** ret_kmap, char* str);
int _editor_init_kmap_add_binding_by_str(editor_t* editor, kmap_t* kmap, char* str);


/*-----------------------------------------------------*/

// our very own vector implementation. from:
// http://eddmann.com/posts/implementing-a-dynamic-vector-array-in-c/

typedef struct vector {
  void **items;
  int capacity;
  int total;
} vector;

void vector_init(vector *v, int capacity) {
  v->capacity = capacity;
  v->total = 0;
  v->items = malloc(sizeof(void *) * v->capacity);
}

static void vector_resize(vector *v, int capacity) {
  void **items = realloc(v->items, sizeof(void *) * capacity);
  if (items) {
    v->items = items;
    v->capacity = capacity;
  }
}

void vector_add(vector *v, void *item) {
  if (v->capacity == v->total)
    vector_resize(v, v->capacity * 2);
  v->items[v->total++] = item;
}

void *vector_get(vector *v, int index) {
  if (index >= 0 && index < v->total)
    return v->items[index];
  return NULL;
}

int vector_size(vector *v) {
  return v->total;
}

void vector_free(vector *v) {
  free(v->items);
}

/*-----------------------------------------------------*/

lua_State *luaMain = NULL;
vector pluginNames;
vector pluginVersions;
char * booting_plugin;
const char * plugin_path = "~/.config/eon/plugins";

int call_plugin(const char * pname, const char * func);

int unload_plugins(void) {
  if (luaMain == NULL)
    return 0;

  // printf("Unloading plugins...\n");
  lua_close(luaMain);
  luaMain = NULL;

  vector_free(&pluginNames);
  vector_free(&pluginVersions);
  return 0;
}

int init_plugins(void) {
  // printf("Initializing plugins...\n");
  unload_plugins();

  vector_init(&pluginNames, 1);
  vector_init(&pluginVersions, 1);

  luaMain = luaL_newstate();
  if (!luaMain) {
    fprintf(stderr, "Unable to initialize Lua context for plugins.\n");
    return -1;
  }

  luaL_openlibs(luaMain);
  load_plugin_api(luaMain);
  return 0;
}

void load_plugin(const char * dir, const char * name) {
  const char * pname;
  const char * pver;

  char path[128];
  sprintf(path, "%s/%s/plugin.lua", dir, name);
  // printf("Loading plugin in path '%s': %s\n", path, name);

  /* Load the plugin. */
  if (luaL_dofile(luaMain, path)) {
    fprintf(stderr, "Could not load plugin: %s\n", lua_tostring(luaMain, -1));
    return;
  }

  /* Get and check the plugin has a name*/
  lua_getfield(luaMain, -1, "name");
  if (lua_isnil(luaMain, -1)) {
    fprintf(stderr, "Could not load file %s: name missing\n", path);
    lua_pop(luaMain, 2);
    return;
  }

  pname = lua_tostring(luaMain, -1);
  lua_pop(luaMain, 1);

  // get version
  lua_getfield(luaMain, -1, "version");
  pver = lua_tostring(luaMain, -1);
  lua_pop(luaMain, 1);

  /* Set the loaded plugin to a global using it's name. */
  lua_setglobal(luaMain, name);

  vector_add(&pluginNames, (void *)name);
  vector_add(&pluginVersions, (void *)pver);

  printf("Loaded plugin: %s\n", name);

  // run on_boot function, if present
  lua_getfield(luaMain, 0, "boot");
  if (!lua_isnil(luaMain, -1)) { // not nil, so present
    booting_plugin = (char *)name;
    call_plugin(name, "boot");
    booting_plugin = NULL;
  }

}

int load_plugins(editor_t * editor) {
  if (luaMain == NULL && init_plugins() == -1)
    return -1;

  char* expanded_path;
  util_expand_tilde((char *)plugin_path, strlen(plugin_path), &expanded_path);

  DIR *dir;
  struct dirent *ent;
  if ((dir = opendir(expanded_path)) != NULL) {
    while ((ent = readdir(dir)) != NULL) {
      if (strcmp(ent->d_name, ".") != 0 && strcmp(ent->d_name, "..") != 0) {
        load_plugin(expanded_path, strdup(ent->d_name));
      }
    }
    free(ent);
    closedir(dir);
  } else {
    fprintf(stderr, "Unable to open plugin directory: %s\n", plugin_path);
    return -1;
  }

  /* Create a global table with name = version of loaded plugins. */
  lua_createtable(luaMain, 0, vector_size(&pluginNames));

  int i;
  for (i = 0; i < vector_size(&pluginNames); i++) {
    lua_pushstring(luaMain, vector_get(&pluginNames, i));
    lua_pushstring(luaMain, vector_get(&pluginVersions, i));
    lua_settable(luaMain, -3);
  }

  lua_setglobal(luaMain, "plugins");
  return 0;
}

void show_plugins() {
  lua_State    *L;
  const char * pname;
  const char * pver;

  if (luaMain != NULL) {
    L = lua_newthread(luaMain);

    lua_getglobal(L, "plugins");
    lua_pushnil(L);
    while (lua_next(L, -2)) {
      if (!lua_isstring(L, -2)) {
        lua_pop(L, 1);
        continue;
      }

      pname = lua_tostring(L, -2);
      if (lua_isstring(L, -1)) {
        pver = lua_tostring(L, -1);
      } else {
        pver = "?";
      }

      // printf(" --> name: %s, ver: %s\n", pname, pver);
      lua_pop(L, 1);
    }

    lua_settop(L, 0);
    lua_pop(luaMain, 1);
  }
}

///////////////////////////////////////////////////////

int run_plugin_function(cmd_context_t * ctx) {
  // cmd name should be "plugin_name:function_name"
  char * cmd = ctx->cmd->name;
  char * delim;
  int pos, len;

  // so get the position of the :
  delim = strchr(cmd, ':');
  pos = (int)(delim - cmd);

  // get the name of the plugin
  len = pos;
  char plugin[len];
  strncpy(plugin, cmd, len);

  // and the name of the function
  len = strlen(cmd) - pos;
  char func[len];
  strncpy(func, cmd + pos, len);

  // and then call it
  plugin_ctx = ctx;
  return call_plugin(plugin, func);
}

int call_plugin(const char * pname, const char * func) {
  lua_State  *L;
  char text[7] = "foobar";
  // printf(" ---->Triggering event %s on plugin %s\n", func, pname);

  L = lua_newthread(luaMain);
  lua_getglobal(L, pname);
  if (lua_isnil(L, -1)) {
    fprintf(stderr, "Fatal: Could not get plugin: %s\n", pname);
    lua_pop(luaMain, 1);
    return -1;
  }

  /* Run the plugin's run function providing it with the text. */
  lua_getfield(L, -1, func);
  lua_pushstring(L, text);

  if (lua_pcall(L, 1, LUA_MULTRET, 0) != 0) {
    // printf("Fatal: Could not run %s function on plugin: %s\n", func, pname);
    lua_pop(luaMain, 1);
    return -1;
  }

  if (lua_gettop(L) == 3) {
    fprintf(stderr, "Fatal: plugin failed: %s\n", pname);
    lua_pop(luaMain, 1);
    return -1;
  } else {
    // printf("Result from %s: %s\n", func, lua_tostring(L, -1));
  }

  lua_pop(luaMain, 1);
  return 0;
}

int trigger_plugin_event(const char * event, cmd_context_t ctx) {
  plugin_ctx = &ctx;

  const char * pname;
  int i, res;

  // printf(" ---->Triggering event: %s\n", event);
  for (i = 0; i < vector_size(&pluginNames); i++) {
    pname = vector_get(&pluginNames, i);
    res = call_plugin(pname, event);
    // if (res == -1) unload_plugin(pname); // TODO: stop further calls to this guy.
  }

  return 0;
}


int add_listener(const char * when, const char * event, const char * func) {

  char * plugin = booting_plugin;
  if (!plugin) {
    fprintf(stderr, "Something's not right. Plugin called boot function out of scope!\n");
    return -1;
  }

  printf("[%s] adding listener %s %s --> %s\n", plugin, when, event, func);
  // TODO
  return 0;
}

int register_func_as_command(const char * func) {
  char * cmd; // for editor
  char * plugin = booting_plugin;
  if (!plugin) {
    fprintf(stderr, "Something's not right. Plugin called boot function out of scope!\n");
    return -1;
  }

  int len = strlen(plugin) * strlen(func) + 1;
  cmd = malloc(len);
  snprintf(cmd, len, "%s.%s", (char *)plugin, (char *)func);

  printf("[%s] registering cmd --> %s\n", plugin, cmd);

  return 0;
  // return _editor_register_cmd_fn(plugin_ctx->editor, cmd, run_plugin_function);
}

int add_plugin_keybinding(const char * keys, const char * func) {

  char * plugin = booting_plugin;
  if (!plugin) {
    fprintf(stderr, "Something's not right. Plugin called boot function out of scope!\n");
    return -1;
  }

  printf("[%s] mapping %s to --> %s\n", plugin, keys, func);

  return 0;

/*
  kmap_t* kmap;
  if (_editor_init_kmap_by_str(plugin_ctx->editor, &kmap, (char *)keymap_name) != EON_OK)
    return 0;

  int res = _editor_init_kmap_add_binding_by_str(plugin_ctx->editor, kmap, (char *)command_name);
  lua_pushnumber(L, res);
  return 1;
*/
}
