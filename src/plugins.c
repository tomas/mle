#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include "eon.h"
#include "vector.h"

void load_plugin_api(lua_State *luaMain);


int _editor_register_cmd_fn(editor_t* editor, char* name, int (*func)(cmd_context_t* ctx)) {
  return 0;
}

int _editor_init_kmap_by_str(editor_t* editor, kmap_t** ret_kmap, char* str);
int _editor_init_kmap_add_binding_by_str(editor_t* editor, kmap_t* kmap, char* str);

/*-----------------------------------------------------*/

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

lua_State *luaMain = NULL;
vector plugin_names;
vector plugin_versions;
vector events;

int max_events_for_listening = 128;
int max_listeners_per_event = 64;
int listeners[128][64];

int plugin_count = 0;
char * booting_plugin_name;

const char * plugin_path = "~/.config/eon/plugins";

int call_plugin(const char * pname, const char * func);

int unload_plugins(void) {
  if (luaMain == NULL)
    return 0;

  // printf("Unloading plugins...\n");
  lua_close(luaMain);
  luaMain = NULL;

  vector_free(&plugin_names);
  vector_free(&plugin_versions);
  vector_free(&events);

/*
  int i;
  vector * temp;
  for (i = 0; i < vector_size(&event_listeners); i++) {
    temp = vector_get(&event_listeners, i);
    if (temp->total > 0) free(temp->items);
  }

  vector_free(&event_listeners);
*/

  int i;
  for (i = 0; i < max_events_for_listening; ++i) {
    // free(listeners[i]);
  }

  // free(listeners);

  return 0;
}

int init_plugins(void) {
  // printf("Initializing plugins...\n");
  unload_plugins();

  vector_init(&plugin_names, 1);
  vector_init(&plugin_versions, 1);
  vector_init(&events, 1);
  // vector_init(&event_listeners, 1);

  // int i;
  // for (i = 0; i < max_events_for_listening; i++)
  //  listeners[i] = {0};

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


  // successfully loaded, so increase count
  plugin_count++;

  // printf("Adding %s to list of plugins, as number %d\n", name, plugin_count);

  /* Set the loaded plugin to a global using it's name. */
  lua_setglobal(luaMain, name);
  vector_add(&plugin_names, (void *)name);
  vector_add(&plugin_versions, (void *)pver);

  // run on_boot function, if present
  lua_getfield(luaMain, 0, "boot");
  if (!lua_isnil(luaMain, -1)) { // not nil, so present
    booting_plugin_name = (char *)name;
    call_plugin(name, "boot");
    booting_plugin_name = NULL;
  }

  // printf("Finished loading plugin %d: %s\n", plugin_count, name);
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
  lua_createtable(luaMain, 0, vector_size(&plugin_names));

  int i;
  for (i = 0; i < vector_size(&plugin_names); i++) {
    lua_pushstring(luaMain, vector_get(&plugin_names, i));
    lua_pushstring(luaMain, vector_get(&plugin_versions, i));
    lua_settable(luaMain, -3);
  }

  lua_setglobal(luaMain, "plugins");

  printf("%d plugins initialized. Slick.\n", plugin_count);
  return plugin_count;
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
  printf(" ---->Triggering event %s on plugin %s\n", func, pname);

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

/*
struct listener_list {
  char name[10]; // name of event
  int listeners[10];
  UT_hash_handle hh; // makes this structure hashable
};

struct listener_list * event_listeners = NULL;
*/

// events    -> [ 'foo', 'bar' ]
// listeners -> [ [1, 3, 5], [2, 4] ]
// foo event has 1, 3, 5 as listeners
// bar event has 2, 4 as listeners

int get_event_id(const char * event) {
  int i, res = -1;

  const char * name;
    for (i = 0; i < vector_size(&events); i++) {
    name = vector_get(&events, i);
    if (strcmp(event, name) == 0) {
      res = i;
      break;
    }
  }

  return res;
}

int get_last_listener_for(int event_id) {
  int i, res = -1;
  for (i = 0; i < max_listeners_per_event; i++) {
    if (listeners[event_id][i] == 0) {
      res = i;
      break;
    }
  }

  return res;
}


int trigger_plugin_event(const char * event, cmd_context_t ctx) {

  int event_id = get_event_id(event);
  printf("plugin event: %s -> %d\n", event, event_id);
  if (event_id == -1) return 0; // no listeners

  int i, res;
  const char * name;

  plugin_ctx = &ctx;
  int count = get_last_listener_for(event_id);
  if (count == -1) return -1;

  // printf(" ----> %d listeners for event: %s\n", count, event);
  int plugin;
  for (i = 0; i < count; i++) {
    int plugin = listeners[event_id][i];
    name = vector_get(&plugin_names, plugin-1); // index starts at zero

    // printf(" - %d %s\n", plugin, name);
    res = call_plugin(name, event);
    // if (res == -1) unload_plugin(name); // TODO: stop further calls to this guy.
  }

  return 0;
}

int add_listener(const char * when, const char * event, const char * func) {

  int res;
  char * plugin = booting_plugin_name;

  if (!plugin) {
    fprintf(stderr, "Something's not right. Plugin called boot function out of scope!\n");
    return -1;
  }

  printf("[%s] adding listener %s %s --> %s\n", plugin, when, event, func);

  int len = strlen(when) * strlen(event) + 1;
  char * event_name = malloc(len);
  snprintf(event_name, len, "%s.%s", (char *)when, (char *)event);

  int event_id = get_event_id(event_name);
  if (event_id == -1) {

    // new event, so add to event list and initialize new listener list
    int count = vector_add(&events, event_name);
    event_id = count - 1;

/*
    // now initialize the listener list and append to to the main array
    vector_init(&listeners, 1);
    res = vector_add(&event_listeners, &listeners);

    if (event_id != res - 1) {
      fprintf(stderr, "Plugin/event ID mismatch. Please file a bug at github.com/tomas/eon!\n");
      exit(1);
    }

  } else {

    vector * temp = vector_get(&event_listeners, event_id);
    listeners = *temp;

*/

  }

  int index = get_last_listener_for(event_id);
  if (index >= 0) listeners[event_id][index] = plugin_count;

  // printf("[%s] added new listener: %d, event id is %d, listener count is %d\n", event_name, plugin_count, event_id, index+1);
  return index;
}

int register_func_as_command(const char * func) {

  char * plugin = booting_plugin_name;
  if (!plugin) {
    fprintf(stderr, "Something's not right. Plugin called boot function out of scope!\n");
    return -1;
  }

  char * cmd; // for editor
  int len = strlen(plugin) * strlen(func) + 1;
  cmd = malloc(len);
  snprintf(cmd, len, "%s.%s", (char *)plugin, (char *)func);

  printf("[%s] registering cmd --> %s\n", plugin, cmd);

  return 0;
  // return _editor_register_cmd_fn(plugin_ctx->editor, cmd, run_plugin_function);
}

int add_plugin_keybinding(const char * keys, const char * func) {

  char * plugin = booting_plugin_name;
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
