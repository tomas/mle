#include <stdio.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include "eon.h"

cmd_context_t * plugin_ctx; // shared global, from eon.h

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

// int = current_line_number()
static int current_line_number(lua_State * L) {
  int line_number = plugin_ctx->cursor->mark->bline->line_index;
  lua_pushnumber(L, line_number);
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
  return 0;
};

// append_buffer_at_line(line_number, buffer)
static int append_buffer_at_line(lua_State *L) {
  int line_index = lua_tointeger(L, 1);
  const char *buf = luaL_checkstring(L, 2);

  bline_t * line;
  buffer_get_bline(plugin_ctx->bview->buffer, line_index, &line);

  bint_t ret_chars;
  bline_insert(line, line->data_len, (char *)buf, strlen(buf), &ret_chars);

  lua_pushnumber(L, ret_chars);
  return 0;
};

void load_plugin_api(lua_State *luaMain) {
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

  lua_pushcfunction(luaMain, prompt_user);
  lua_setglobal(luaMain, "prompt_user");

}