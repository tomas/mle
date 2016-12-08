#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include "eon.h"

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

cmd_context_t * current_ctx;

const char * plugin_path = "plugins";

// int = current_line_number()
static int current_line_number(lua_State * L) {
  int line_number = current_ctx->cursor->mark->bline->line_index;
  lua_pushnumber(L, line_number);
  return 1;
}

// bool = has_selection()
static int has_selection(lua_State * L) {
  int anchored = current_ctx->cursor->is_anchored;
  lua_pushboolean(L, anchored);
  return 1;
}

// selection = get_selection()
// selection.from.col, selection.from.line
// selection.to.col, selection.to.line
static int get_selection(lua_State * L) {
  mark_t * mark = current_ctx->cursor->mark;
  mark_t * anchor = current_ctx->cursor->anchor;
  mark_t * first;
  mark_t * last;

  if (mark->bline->line_index < anchor->bline->line_index) {
    first = anchor; last = mark;
  } else if (mark->bline->line_index > anchor->bline->line_index) {
    first = mark; last = anchor;
  } else if (mark->col > anchor->col) {
    first = mark; last = anchor;
  } else {
    first = anchor; last = mark;
  }

  lua_createtable(L, 4, 0);
  lua_pushinteger(L, first->bline->line_index);
  lua_rawseti(L,-2, 2);
  lua_pushinteger(L, first->col);
  lua_rawseti(L,-2, 3);
  lua_pushinteger(L, last->bline->line_index);
  lua_rawseti(L,-2, 0);
  lua_pushinteger(L, last->col);
  lua_rawseti(L,-2, 1);

  return 1;
}

static int get_line_count(lua_State * L) {
  int line_count = current_ctx->bview->buffer->line_count;
  lua_pushnumber(L, line_count);
  return 1; // one argument
}

static int get_buffer_at_line(lua_State *L) {
  int line_index = lua_tointeger(L, 1);

  bline_t * line;
  buffer_get_bline(current_ctx->bview->buffer, line_index, &line);
  if (!line) return 0;

  lua_pushlstring(L, line->data, line->data_len);
  return 1; // one argument
};

// set_buffer_at_line(number, buffer)
static int set_buffer_at_line(lua_State *L) {
  int line_index = lua_tointeger(L, 1);
  const char *buf = luaL_checkstring(L, 2);

  bline_t * line;
  buffer_get_bline(current_ctx->bview->buffer, line_index, &line);
  if (!line) return 0;

  int col = 0;
  int res = bline_replace(line, col, line->data_len, (char *)buf, strlen(buf));

  lua_pushnumber(L, res);
  return 1; // one argument
};

// insert_buffer_at_line(line_number, buffer, column)
static int insert_buffer_at_line(lua_State *L) {
  int line_index = lua_tointeger(L, 1);
  const char *buf = luaL_checkstring(L, 2);
  int column = lua_tointeger(L, 3);
  if (!column || column < 0) return 0;

  bline_t * line;
  buffer_get_bline(current_ctx->bview->buffer, line_index, &line);
  if (!line) return 0;

  bint_t ret_chars;
  bline_insert(line, column, (char *)buf, strlen(buf), &ret_chars);

  lua_pushnumber(L, ret_chars);
  return 1;
};

// delete_chars_at_line(line_number, column, count)
static int delete_chars_at_line(lua_State *L) {
  int line_index = lua_tointeger(L, 1);
  int column = lua_tointeger(L, 2);
  int count = lua_tointeger(L, 3);
  if (line_index < 0 || column < 0) return 0;

  bline_t * line;
  buffer_get_bline(current_ctx->bview->buffer, line_index, &line);
  if (!line) return 0;

  if (!count) count = line->data_len - column;

  int res = bline_delete(line, column, count);
  lua_pushnumber(L, res);
  return 1;
};

// prepend_buffer_at_line(line_number, buffer)
static int prepend_buffer_at_line(lua_State *L) {
  int line_index = lua_tointeger(L, 1);
  const char *buf = luaL_checkstring(L, 2);
  if (line_index < 0) return 0;

  bline_t * line;
  buffer_get_bline(current_ctx->bview->buffer, line_index, &line);
  if (!line) return 0;

  bint_t ret_chars;
  bline_insert(line, 0, (char *)buf, strlen(buf), &ret_chars);

  lua_pushnumber(L, ret_chars);
  return 0;
};

// append_buffer_at_line(line_number, buffer)
static int append_buffer_at_line(lua_State *L) {
  int line_index = lua_tointeger(L, 1);
  const char *buf = luaL_checkstring(L, 2);

  bline_t * line;
  buffer_get_bline(current_ctx->bview->buffer, line_index, &line);

  bint_t ret_chars;
  bline_insert(line, line->data_len, (char *)buf, strlen(buf), &ret_chars);

  lua_pushnumber(L, ret_chars);
  return 0;
};


int unload_plugins(void) {
  if (luaMain == NULL)
    return 0;

  printf("Unloading plugins...\n");
  lua_close(luaMain);
  luaMain = NULL;

  vector_free(&pluginNames);
  vector_free(&pluginVersions);
  return 0;
}

void init_plugins(void) {
  // printf("Initializing plugins...\n");
  unload_plugins();

  vector_init(&pluginNames, 1);
  vector_init(&pluginVersions, 1);

  luaMain = luaL_newstate();
  luaL_openlibs(luaMain);

  lua_pushcfunction(luaMain, has_selection);
  lua_setglobal(luaMain, "has_selection");
  lua_pushcfunction(luaMain, get_selection);
  lua_setglobal(luaMain, "get_selection");
  lua_pushcfunction(luaMain, current_line_number);
  lua_setglobal(luaMain, "current_line_number");

  lua_pushcfunction(luaMain, get_line_count);
  lua_setglobal(luaMain, "get_line_count");
  lua_pushcfunction(luaMain, get_buffer_at_line);
  lua_setglobal(luaMain, "get_buffer_at_line");
  lua_pushcfunction(luaMain, set_buffer_at_line);
  lua_setglobal(luaMain, "set_buffer_at_line");

  lua_pushcfunction(luaMain, insert_buffer_at_line);
  lua_setglobal(luaMain, "insert_buffer_at_line");
  lua_pushcfunction(luaMain, delete_chars_at_line);
  lua_setglobal(luaMain, "delete_chars_at_line");
  lua_pushcfunction(luaMain, prepend_buffer_at_line);
  lua_setglobal(luaMain, "prepend_buffer_at_line");
  lua_pushcfunction(luaMain, append_buffer_at_line);
  lua_setglobal(luaMain, "append_buffer_at_line");
}

void load_plugin(const char * name) {
  const char * pname;
  const char * pver;

  char path[128];
  sprintf(path, "%s/%s", plugin_path, name);

  printf("Loading plugin in path '%s': %s\n", plugin_path, name);

  /* Load the plugin. */
  if (luaL_dofile(luaMain, path)) {
    printf("Could not load plugin: %s\n", lua_tostring(luaMain, -1));
    return;
  }

  /* Get and check the plugin has a name*/
  lua_getfield(luaMain, -1, "name");
  if (lua_isnil(luaMain, -1)) {
    printf("Could not load file %s: name missing\n", path);
    lua_pop(luaMain, 2);
    return;
  }

  pname = lua_tostring(luaMain, -1);
  lua_pop(luaMain, 1);

  /* Get the plugin version (optional attribute). */
  lua_getfield(luaMain, -1, "version");
  pver = lua_tostring(luaMain, -1);
  lua_pop(luaMain, 1);

  /* Set the loaded plugin to a global using it's name. */
  lua_setglobal(luaMain, name);

  vector_add(&pluginNames, (void *)name);
  vector_add(&pluginVersions, (void *)pver);
}

int load_plugins(editor_t * editor) {
  if (luaMain == NULL)
    init_plugins();

  DIR *dir;
  struct dirent *ent;
  if ((dir = opendir(plugin_path)) != NULL) {
    while ((ent = readdir(dir)) != NULL) {
      if ((strcmp(ent->d_name, ".") != 0) && (strcmp(ent->d_name, "..") != 0)) {
        load_plugin(strdup(ent->d_name));
      }
    }
    free(ent);
    closedir(dir);
  } else {
    printf("Unable to open plugin directory: %s\n", plugin_path);
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

int call_plugin(const char * pname, const char * func, cmd_context_t ctx) {
  lua_State  *L;
  char text[7] = "foobar";

  // printf(" ---->Triggering event %s on plugin %s\n", func, pname);

  L = lua_newthread(luaMain);
  lua_getglobal(L, pname);
  if (lua_isnil(L, -1)) {
    printf("Fatal: Could not get plugin: %s\n", pname);
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
    printf("Fatal: plugin failed: %s\n", pname);
    lua_pop(luaMain, 1);
    return -1;
  } else {
    // printf("Result from %s: %s\n", func, lua_tostring(L, -1));
  }

  lua_pop(luaMain, 1);
  return 0;
}

int trigger_plugin_event(const char * event, cmd_context_t ctx) {
  current_ctx = &ctx;

  const char * pname;
  int i, res;

  // printf(" ---->Triggering event: %s\n", event);
  for (i = 0; i < vector_size(&pluginNames); i++) {
    pname = vector_get(&pluginNames, i);
    res = call_plugin(pname, event, ctx);
    // if (res == -1) unload_plugin(pname); // TODO: stop further calls to this guy.
  }

  return 0;
}


/*
int main() {
  printf("Loading plugins...\n");
  load_plugins();
  printf("Getting plugin info...\n");
  show_plugins();
  printf("Running plugins...\n");
  call_plugin("upper", "run");
  printf("Unloading plugins...\n");
  unload_plugins();
  printf("Plugins unloaded!\n");
}
*/