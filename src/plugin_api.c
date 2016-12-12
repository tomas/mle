#include <stdio.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include "eon.h"
#include "termbox.h"

cmd_context_t * plugin_ctx; // shared global, from eon.h

// from plugins.c
plugin_opt * get_plugin_option(const char * key);
int add_listener(const char * when, const char * event, const char * func);
int register_func_as_command(const char * func);
int add_plugin_keybinding(const char * keys, const char * func);

/* plugin functions
-----------------------------------------------------------*/

int test_callback(lua_State * L){
  // if called with one argument and that argument is a function
  if (lua_gettop(L) == 1 && lua_isfunction(L, -1)) {
    // push arg to function
    lua_pushnumber(L, 3);
    // call function with one argument and no return values
    lua_pcall(L, 1, 0, 0);
  }
  return 0; // no return value
}

static int get_option(lua_State * L) {
  const char *key = luaL_checkstring(L, 1);
  plugin_opt * opt = get_plugin_option(key);
  if (!opt) return 0; // NULL

  switch(opt->type) {
    case 3: // JSMN_STRING
      lua_pushstring(L, opt->value);
      break;

    case 4: // JSMN_PRIMITIVE
      if (opt->value[0] == 'n') // null
        return 0;
      else if (opt->value[0] == 't' || opt->value[0] == 'f') // true or false
        lua_pushboolean(L, !!opt->value);
      else
        lua_pushnumber(L, atoi(opt->value));
      break;

    default: // undefined 0, object 1, array 2
      printf("Unknown type for plugin option %s: %d\n", key, opt->type);
      return 0;
      // break;
  }

  free(opt);
  return 1;
}

static int get_url(lua_State * L) {
  const char * url = luaL_checkstring(L, 1);
  const char * data = util_get_url(url);
  if (!data) return 0;

  lua_pushstring(L, data);
  return 1;
}

static int download_file(lua_State * L) {
  const char * url = luaL_checkstring(L, 1);
  const char * target = luaL_checkstring(L, 2);

  size_t bytes = util_download_file(url, target);
  if (bytes == -1) return 0;

  lua_pushnumber(L, bytes);
  return 1;
}

static int register_function(lua_State * L) {
  const char * func = luaL_checkstring(L, 1);
  return register_func_as_command(func); // in plugins.c
}

static int add_keybinding(lua_State * L) {
  const char *keys = luaL_checkstring(L, 1);
  const char *func = luaL_checkstring(L, 2);
  int res = add_plugin_keybinding(keys, func);
  return 0;
}

static int before(lua_State * L) {
  const char *event = luaL_checkstring(L, 1);
  const char *func  = luaL_checkstring(L, 2);
  int res = add_listener("before", event, func);
  return 0;
}

static int after(lua_State * L) {
  const char *event = luaL_checkstring(L, 1);
  const char *func  = luaL_checkstring(L, 2);
  int res = add_listener("after", event, func);
  return 0;
}

static int prompt_user(lua_State * L) {
  const char *prompt = luaL_checkstring(L, 1);
  const char *placeholder = luaL_checkstring(L, 2);

  char * reply;
  editor_prompt(plugin_ctx->editor, (char *)prompt, &(editor_prompt_params_t) {
    .data = placeholder ? (char *)placeholder : "",
    .data_len = placeholder ? strlen(placeholder) : 0
  }, &reply);

  lua_pushstring(L, reply);
  return 1;
}

// open_new_tab(title, content, close_current)
static int open_new_tab(lua_State * L) {
  const char *title = luaL_checkstring(L, 1);
  const char *buf   = luaL_checkstring(L, 2);
  int close_current = lua_tointeger(L, 3);

  bview_t * view;
  int res = editor_open_bview(plugin_ctx->editor, NULL, EON_BVIEW_TYPE_EDIT, (char *)title, strlen(title), 1, 0, &plugin_ctx->editor->rect_edit, NULL, &view);
  view->is_menu = 1;
  // view->callback = callback;

  if (res == EON_OK) {
    mark_insert_before(view->active_cursor->mark, (char *)buf, strlen(buf));
    mark_move_to(view->active_cursor->mark, 0, 0);
    if (close_current) editor_close_bview(plugin_ctx->editor, plugin_ctx->editor->active_edit, NULL);
  }

  lua_pushnumber(L, res);
  return 1;
}

static int draw(lua_State * L) {
  int x = lua_tointeger(L, 1);
  int y = lua_tointeger(L, 2);
  int bg = lua_tointeger(L, 3);
  int fg = lua_tointeger(L, 4);
  const char *str = luaL_checkstring(L, 5);

  tb_print(x, y, bg, fg, (char *)str);
  return 0;
}

// int = current_line_number()
static int current_line_number(lua_State * L) {
  int line_number = plugin_ctx->cursor->mark->bline->line_index;
  lua_pushnumber(L, line_number);
  return 1;
}

static int current_position(lua_State * L) {
  mark_t * mark = plugin_ctx->cursor->mark;

  lua_createtable(L, 2, 0);
  lua_pushinteger(L, mark->bline->line_index);
  lua_rawseti(L, -2, 0);
  lua_pushinteger(L, mark->col);
  lua_rawseti(L, -2, 1);

  return 1;
}

// bool = has_selection()
static int has_selection(lua_State * L) {
  int anchored = plugin_ctx->cursor->is_anchored;
  lua_pushboolean(L, anchored);
  return 1;
}

// selection = get_selection()
// --> [start_line, start_col, end_line, end_col]
static int get_selection(lua_State * L) {
  mark_t * mark = plugin_ctx->cursor->mark;
  mark_t * anchor = plugin_ctx->cursor->anchor;
  mark_t * first;
  mark_t * last;

  if (mark->bline->line_index < anchor->bline->line_index) {
    first = mark; last = anchor;
  } else if (mark->bline->line_index > anchor->bline->line_index) {
    first = anchor; last = mark;
  } else if (mark->col > anchor->col) {
    first = anchor; last = mark;
  } else {
    first = mark; last = anchor;
  }

  lua_createtable(L, 4, 0);
  lua_pushinteger(L, first->bline->line_index);
  lua_rawseti(L, -2, 0);
  lua_pushinteger(L, first->col);
  lua_rawseti(L, -2, 1);
  lua_pushinteger(L, last->bline->line_index);
  lua_rawseti(L, -2, 2);
  lua_pushinteger(L, last->col);
  lua_rawseti(L, -2, 3);

  return 1;
}

// int = current_line_number()
static int current_file_path(lua_State * L) {
  if (EON_BVIEW_IS_EDIT(plugin_ctx->bview) && plugin_ctx->bview->path) {
    int len = strlen(plugin_ctx->bview->path);
    lua_pushlstring(L, plugin_ctx->bview->path, len);
    return 1;
  } else {
    return 0;
  }
}

// returns total number of lines in current view
static int get_line_count(lua_State * L) {
  int line_count = plugin_ctx->bview->buffer->line_count;
  lua_pushnumber(L, line_count);
  return 1; // one argument
}

// get_buffer_at_line(number)
// returns buffer at line N
static int get_buffer_at_line(lua_State *L) {
  int line_index = lua_tointeger(L, 1);

  bline_t * line;
  buffer_get_bline(plugin_ctx->bview->buffer, line_index, &line);
  if (!line) return 0;

  lua_pushlstring(L, line->data, line->data_len);
  return 1; // one argument
};

// set_buffer_at_line(number, buffer)
static int set_buffer_at_line(lua_State *L) {
  int line_index = lua_tointeger(L, 1);
  const char *buf = luaL_checkstring(L, 2);

  bline_t * line;
  buffer_get_bline(plugin_ctx->bview->buffer, line_index, &line);
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
  buffer_get_bline(plugin_ctx->bview->buffer, line_index, &line);
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
  buffer_get_bline(plugin_ctx->bview->buffer, line_index, &line);
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
  buffer_get_bline(plugin_ctx->bview->buffer, line_index, &line);
  if (!line) return 0;

  bint_t ret_chars;
  bline_insert(line, 0, (char *)buf, strlen(buf), &ret_chars);

  lua_pushnumber(L, ret_chars);
  return 1;
};

// append_buffer_at_line(line_number, buffer)
static int append_buffer_at_line(lua_State *L) {
  int line_index = lua_tointeger(L, 1);
  const char *buf = luaL_checkstring(L, 2);

  bline_t * line;
  buffer_get_bline(plugin_ctx->bview->buffer, line_index, &line);
  if (!line) return 0;

  bint_t ret_chars;
  bline_insert(line, line->data_len, (char *)buf, strlen(buf), &ret_chars);

  lua_pushnumber(L, ret_chars);
  return 1;
};

static int set_line_bg_color(lua_State * L) {
  int line_index = lua_tointeger(L, 1);
  int color = lua_tointeger(L, 2);

  int res = bview_set_line_bg(plugin_ctx->bview, line_index, color);

  lua_pushnumber(L, res);
  return 1;
}

void load_plugin_api(lua_State *luaMain) {
  lua_pushcfunction(luaMain, get_option);
  lua_setglobal(luaMain, "get_option");

  lua_pushcfunction(luaMain, current_line_number);
  lua_setglobal(luaMain, "current_line_number");
  lua_pushcfunction(luaMain, current_position);
  lua_setglobal(luaMain, "current_position");

  lua_pushcfunction(luaMain, current_file_path);
  lua_setglobal(luaMain, "current_file_path");

  lua_pushcfunction(luaMain, has_selection);
  lua_setglobal(luaMain, "has_selection");
  lua_pushcfunction(luaMain, get_selection);
  lua_setglobal(luaMain, "get_selection");

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

  lua_pushcfunction(luaMain, set_line_bg_color);
  lua_setglobal(luaMain, "set_line_bg_color");

  lua_pushcfunction(luaMain, prompt_user);
  lua_setglobal(luaMain, "prompt_user");
  lua_pushcfunction(luaMain, open_new_tab);
  lua_setglobal(luaMain, "open_new_tab");
  lua_pushcfunction(luaMain, draw);
  lua_setglobal(luaMain, "draw");

  lua_pushcfunction(luaMain, add_keybinding);
  lua_setglobal(luaMain, "add_keybinding");
  lua_pushcfunction(luaMain, register_function);
  lua_setglobal(luaMain, "register_function");

  lua_pushcfunction(luaMain, before);
  lua_setglobal(luaMain, "before");
  lua_pushcfunction(luaMain, after);
  lua_setglobal(luaMain, "after");

  lua_pushcfunction(luaMain, get_url);
  lua_setglobal(luaMain, "get_url");
  lua_pushcfunction(luaMain, download_file);
  lua_setglobal(luaMain, "download_file");


}