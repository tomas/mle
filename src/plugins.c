#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include "eon.h"
#include "vector.h"
#include "jsmn.h"

void load_plugin_api(lua_State *luaMain);

/*-----------------------------------------------------*/

// #define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

lua_State *luaMain = NULL;
vector plugin_names;
vector plugin_versions;
vector events;
vector listeners;

typedef struct listener {
  char plugin[32];
  char func[32];
  struct listener *next;
} listener;

/////////////////////////////////////////////

int plugin_count = 0;

editor_t * editor_ref; // needed for function calls from lua when booting
char * booting_plugin_name;

const char * plugin_path = "~/.config/eon/plugins";

// for option parsing
#define MAX_TOKENS 32
static char * json_string;
jsmntok_t json_root[MAX_TOKENS];

int read_plugin_options(const char * plugin) {
  jsmn_parser parser;
  jsmn_init(&parser);

  char path[128];
  sprintf(path, "%s/%s/plugin.conf", plugin_path, plugin);
  
  char* expanded_path;
  util_expand_tilde((char *)path, strlen(path), &expanded_path);

  char * str = util_read_file(expanded_path);
  if (!str) {
    return -1;
  }
  
  int res = jsmn_parse(&parser, str, strlen(str), json_root, MAX_TOKENS);

  if (res >= 0) { // success, so store a reference to string
    json_string = str;
  }

  return res;
}

int unload_plugins(void) {
  if (luaMain == NULL)
    return 0;

  // printf("Unloading plugins...\n");
  lua_close(luaMain);
  luaMain = NULL;

  vector_free(&plugin_names);
  vector_free(&plugin_versions);
  vector_free(&events);

  int i;
  listener * temp, * obj;

  // listeners contains the first one for each event
  // so for each event process the row, and continue
  for (i = 0; i < vector_size(&listeners); i++) {
    obj = vector_get(&listeners, i);
    while(obj) {
      temp = obj->next;
      free(obj);
      obj = temp;
    };
  }

  vector_free(&listeners);
  return 0;
}

int init_plugins(void) {
  // printf("Initializing plugins...\n");
  unload_plugins();

  vector_init(&plugin_names, 1);
  vector_init(&plugin_versions, 1);
  vector_init(&events, 1);
  vector_init(&listeners, 1);

  luaMain = luaL_newstate();
  if (!luaMain) {
    fprintf(stderr, "Unable to initialize Lua context for plugins.\n");
    return -1;
  }

  luaL_openlibs(luaMain);
  load_plugin_api(luaMain);
  return 0;
}


int call_plugin(const char * pname, const char * func) {
  lua_State  *L;
  // printf(" ---->Triggering function %s on plugin %s\n", func, pname);

  L = lua_newthread(luaMain);
  lua_getglobal(L, pname);
  if (lua_isnil(L, -1)) {
    fprintf(stderr, "Fatal: Could not get plugin: %s\n", pname);
    lua_pop(luaMain, 1);
    return -1;
  }

  lua_getfield(L, -1, func); // get the function

  // call it
  if (lua_pcall(L, 0, LUA_MULTRET, 0) != 0) {
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

void load_plugin(const char * dir, const char * name) {
  const char * pname;
  const char * pver;

  char path[128];
  sprintf(path, "%s/%s/plugin.lua", dir, name);

  // printf("Loading plugin in path '%s': %s\n", path, name);
  if (luaL_dofile(luaMain, path)) {
    fprintf(stderr, "Could not load plugin: %s\n", lua_tostring(luaMain, -1));
    return;
  }

  // Get and check the plugin's name
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
  
    read_plugin_options(name);
    booting_plugin_name = (char *)name;

    call_plugin(name, "boot");
  
    if (json_string) {
      free(json_string);
      json_string = NULL;
    }

    // json_root = NULL;
    booting_plugin_name = NULL;
  }

  printf("Finished loading plugin %d: %s\n", plugin_count, name);
}

int load_plugins(editor_t * editor) {
  if (luaMain == NULL && init_plugins() == -1)
    return -1;

  editor_ref = editor;
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
    editor_ref = NULL;
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

  printf("%d plugins initialized.\n", plugin_count);
  editor_ref = NULL;
  return plugin_count;
}

/*
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
*/

///////////////////////////////////////////////////////

int run_plugin_function(cmd_context_t * ctx) {
  // cmd name should be "plugin_name.function_name"
  char * cmd = ctx->cmd->name;
  char * delim;
  int res, pos, len;

  // so get the position of the dot
  delim = strchr(cmd, '.');
  pos = (int)(delim - cmd);

  // get the name of the plugin
  len = pos-4;
  char plugin[len];
  strncpy(plugin, cmd+4, len);
  plugin[len] = '\0';

  // and the name of the function
  len = strlen(cmd) - pos;
  char func[len];
  strncpy(func, cmd + pos + 1, len);
  func[len] = '\0';

  // and finally call it
  // printf("calling %s function from %s plugin\n", func, plugin);
  plugin_ctx = ctx;
  res = call_plugin(plugin, func);
  plugin_ctx = NULL;
  return res;
}

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

int trigger_plugin_event(const char * event, cmd_context_t ctx) {

  int event_id = get_event_id(event);
  if (event_id == -1) return 0; // no listeners

  int res = -1;
  plugin_ctx = &ctx;
  listener * el;
  el = vector_get(&listeners, event_id);

  while (el) {
    res = call_plugin(el->plugin, el->func);
    // if (res == -1) unload_plugin(name); TODO: stop further calls to this guy.
    el = el->next;
  };

  plugin_ctx = NULL;
  return res;
}

plugin_opt * get_plugin_option(const char * key) {
  char * plugin = booting_plugin_name;

  if (!plugin) {
    fprintf(stderr, "Something's not right. Plugin called boot function out of scope!\n");
    return NULL;
  }

  if (!json_string) return NULL;

  jsmntok_t * token = get_hash_token(json_string, json_root, key);
  if (!token) return NULL; // not found

  plugin_opt * opt;
  opt = (plugin_opt *)malloc(sizeof(plugin_opt));
  opt->type = token->type;
  opt->value = strdup(get_string_from_token(json_string, token));
  return opt;
}

int add_listener(const char * when, const char * event, const char * func) {

  char * plugin = booting_plugin_name;

  if (!plugin) {
    fprintf(stderr, "Something's not right. Plugin called boot function out of scope!\n");
    return -1;
  }

  // printf("[%s] adding listener %s.%s --> %s\n", plugin, when, event, func);

  int len = strlen(when) * strlen(event) + 1;
  char * event_name = malloc(len);
  snprintf(event_name, len, "%s.%s", (char *)when, (char *)event);

  listener * obj;
  int event_id = get_event_id(event_name);
  if (event_id == -1) {

    // new event, so add to event list and initialize new listener list
    int count = vector_add(&events, event_name);
    event_id = count - 1;

    // we should set this to null, but in that case we'll get a null object afterwards
    obj = (listener *)malloc(sizeof(listener) + 1);
    strcpy(obj->plugin, plugin);
    strcpy(obj->func, func);
    obj->next = NULL;
    vector_add(&listeners, obj);

  } else {

    // start with the first, and get the last element in array
    obj = vector_get(&listeners, event_id);
    while (obj->next) { obj = obj->next; }

    listener * el;
    el = (listener *)malloc(sizeof(listener) + 1);
    strcpy(el->plugin, plugin);
    strcpy(el->func, func);
    el->next = NULL; // very important
    obj->next = el;

  }

  // printf("[%s] added new listener: %d, event id is %d, listener count is %d\n", event_name, plugin_count, event_id, index+1);
  return 0;
}

int register_func_as_command(const char * func) {

  char * plugin = booting_plugin_name;
  if (!plugin) {
    fprintf(stderr, "Something's not right. Plugin called boot function out of scope!\n");
    return -1;
  }

  char * cmd_name;
  int len = strlen(plugin) * strlen(func) + 1;
  cmd_name = malloc(len);
  snprintf(cmd_name, len, "cmd_%s.%s", (char *)plugin, (char *)func);

  printf("[%s] registering cmd --> %s\n", plugin, cmd_name);

  cmd_t cmd = {0};
  cmd.name = cmd_name;
  cmd.func = run_plugin_function;
  return editor_register_cmd(editor_ref, &cmd);
}

int add_plugin_keybinding(const char * keys, const char * func) {

  char * plugin = booting_plugin_name;
  if (!plugin) {
    fprintf(stderr, "Something's not right. Plugin called boot function out of scope!\n");
    return -1;
  }
  
  // TODO: check if plugin func exists

  char * cmd_name;
  int len = strlen(plugin) * strlen(func) + 1;
  cmd_name = malloc(len);
  snprintf(cmd_name, len, "cmd_%s.%s", (char *)plugin, (char *)func);

  printf("[%s] mapping %s to --> %s (%s)\n", plugin, keys, func, cmd_name);
  return editor_add_binding_to_keymap(editor_ref, editor_ref->kmap_normal, &((kbinding_def_t) {cmd_name, (char *)keys, NULL}));

/*
  kmap_t* kmap;
  if (_editor_init_kmap_by_str(plugin_ctx->editor, &kmap, (char *)keymap_name) != EON_OK)
    return 0;

  int res = _editor_init_kmap_add_binding_by_str(plugin_ctx->editor, kmap, (char *)command_name);
  lua_pushnumber(L, res);
  return 1;
*/
}
