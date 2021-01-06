#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <termbox.h>
#include <libgen.h>
#include "uthash.h"
#include "utlist.h"
#include "eon.h"
#include "mlbuf.h"
#include "colors.h"

#ifdef WITH_PLUGINS
int load_plugins(editor_t * editor);
int unload_plugins(void);
int trigger_plugin_event(char * event, cmd_context_t ctx);
#endif

static int _editor_set_macro_toggle_key(editor_t* editor, char* key);
static int _editor_bview_exists(editor_t* editor, bview_t* bview);
static int _editor_register_cmd_fn(editor_t* editor, char* name, int (*func)(cmd_context_t* ctx));
static int _editor_should_skip_rc(char** argv);
static int _editor_close_bview_inner(editor_t* editor, bview_t* bview, int *optret_num_closed);
static int _editor_destroy_cmd(editor_t* editor, cmd_t* cmd);
static int _editor_prompt_input_submit(cmd_context_t* ctx);
static int _editor_prompt_input_complete(cmd_context_t* ctx);
static prompt_history_t* _editor_prompt_find_or_add_history(cmd_context_t* ctx, prompt_hnode_t** optret_prompt_hnode);
static int _editor_prompt_history_up(cmd_context_t* ctx);
static int _editor_prompt_history_down(cmd_context_t* ctx);
static int _editor_prompt_history_append(cmd_context_t* ctx, char* data);
static int _editor_prompt_yna_all(cmd_context_t* ctx);
static int _editor_prompt_yn_yes(cmd_context_t* ctx);
static int _editor_prompt_yn_no(cmd_context_t* ctx);
static int _editor_prompt_cancel(cmd_context_t* ctx);
static int _editor_menu_submit(cmd_context_t* ctx);
static int _editor_menu_cancel(cmd_context_t* ctx);
static int _editor_prompt_menu_up(cmd_context_t* ctx);
static int _editor_prompt_menu_down(cmd_context_t* ctx);
static int _editor_prompt_menu_page_up(cmd_context_t* ctx);
static int _editor_prompt_menu_page_down(cmd_context_t* ctx);
static int _editor_prompt_isearch_next(cmd_context_t* ctx);
static int _editor_prompt_isearch_prev(cmd_context_t* ctx);
static int _editor_prompt_isearch_drop_cursors(cmd_context_t* ctx);
static int _editor_prompt_toggle_replace(cmd_context_t* ctx);
static void _editor_loop(editor_t* editor, loop_context_t* loop_ctx);
static int _editor_maybe_toggle_macro(editor_t* editor, kinput_t* input);
static void _editor_resize(editor_t* editor, int w, int h);
static void _editor_draw_cursors(editor_t* editor, bview_t* bview);
static void _editor_get_user_input(editor_t* editor, cmd_context_t* ctx);
static void _editor_ingest_paste(editor_t* editor, cmd_context_t* ctx);
static void _editor_record_macro_input(kmacro_t* macro, kinput_t* input);
static cmd_t* _editor_get_command(editor_t* editor, cmd_context_t* ctx, kinput_t* opt_peek_input);
static kbinding_t* _editor_get_kbinding_node(kbinding_t* node, kinput_t* input, loop_context_t* loop_ctx, int is_peek, int* ret_again);
static cmd_t* _editor_resolve_cmd(editor_t* editor, cmd_t** rcmd, char* cmd_name);
static int _editor_key_to_input(char* key, kinput_t* ret_input);
static void _editor_init_signal_handlers(editor_t* editor);
static void _editor_graceful_exit(int signum);
static void _editor_register_cmds(editor_t* editor);
static void _editor_init_kmaps(editor_t* editor);
static void _editor_init_kmap(editor_t* editor, kmap_t** ret_kmap, char* name, char* default_cmd_name, int allow_fallthru, kbinding_def_t* defs);
static void _editor_init_kmap_add_binding(editor_t* editor, kmap_t* kmap, kbinding_def_t* binding_def);
static int _editor_init_kmap_add_binding_to_trie(kbinding_t** trie, char* cmd_name, char* cur_key_patt, char* full_key_patt, char* static_param);
static int _editor_init_kmap_by_str(editor_t* editor, kmap_t** ret_kmap, char* str);
static int _editor_init_kmap_add_binding_by_str(editor_t* editor, kmap_t* kmap, char* str);
static void _editor_destroy_kmap(kmap_t* kmap, kbinding_t* trie);
static int _editor_add_macro_by_str(editor_t* editor, char* str);
static void _editor_init_syntaxes(editor_t* editor);
static void _editor_init_syntax(editor_t* editor, syntax_t** optret_syntax, char* name, char* path_pattern, int tab_width, int tab_to_space, srule_def_t* defs);
static int _editor_init_syntax_by_str(editor_t* editor, syntax_t** ret_syntax, char* str);
static void _editor_init_syntax_add_rule(syntax_t* syntax, srule_def_t* def);
static int _editor_init_syntax_add_rule_by_str(syntax_t* syntax, char* str);
static void _editor_destroy_syntax_map(syntax_t* map);
static int _editor_init_from_rc_read(editor_t* editor, FILE* rc, char** ret_rc_data, size_t* ret_rc_data_len);
static int _editor_init_from_rc_exec(editor_t* editor, char* rc_path, char** ret_rc_data, size_t* ret_rc_data_len);
static int _editor_init_from_rc(editor_t* editor, FILE* rc, char* rc_path);
static int _editor_init_from_args(editor_t* editor, int argc, char** argv);
static void _editor_init_status(editor_t* editor);
static void _editor_init_bviews(editor_t* editor, int argc, char** argv);
static int _editor_init_headless_mode(editor_t* editor);
static int _editor_init_startup_macro(editor_t* editor);
static int _editor_init_or_deinit_commands(editor_t* editor, int is_deinit);

// Init editor from args
int editor_init(editor_t* editor, int argc, char** argv) {
  int rv;
  FILE* rc;
  char *home_rc;
  rv = EON_OK;

  do {

    // Set editor defaults
    editor->is_in_init = 1;
    editor->tab_width = EON_DEFAULT_TAB_WIDTH;
    editor->tab_to_space = EON_DEFAULT_TAB_TO_SPACE;
    editor->trim_paste = EON_DEFAULT_TRIM_PASTE;
    editor->smart_indent = EON_DEFAULT_SMART_INDENT;
    editor->highlight_bracket_pairs = EON_DEFAULT_HILI_BRACKET_PAIRS;
    editor->read_rc_file = EON_DEFAULT_READ_RC_FILE;
    editor->soft_wrap = EON_DEFAULT_SOFT_WRAP;
    editor->viewport_scope_x = -4;
    editor->viewport_scope_y = -1;
    editor->color_col = -1;
    editor->exit_code = EXIT_SUCCESS;
    editor->headless_mode = isatty(STDIN_FILENO) == 0 ? 1 : 0;

    _editor_set_macro_toggle_key(editor, EON_DEFAULT_MACRO_TOGGLE_KEY);
    _editor_init_signal_handlers(editor);
    _editor_register_cmds(editor);
    _editor_init_kmaps(editor);
    _editor_init_syntaxes(editor);

    // Parse rc files
    if (!_editor_should_skip_rc(argv)) {
      home_rc = NULL;

      if (getenv("HOME")) {
        int res = asprintf(&home_rc, "%s/%s", getenv("HOME"), ".eonrc");

        if (util_is_file(home_rc, "rb", &rc)) {
          rv = _editor_init_from_rc(editor, rc, home_rc);
          fclose(rc);
        }

        free(home_rc);
      }

      if (rv != EON_OK) break;

      if (util_is_file("/etc/eonrc", "rb", &rc)) {
        rv = _editor_init_from_rc(editor, rc, "/etc/eonrc");
        fclose(rc);
      }

      if (rv != EON_OK) break;
    }

    // Parse cli args
    rv = _editor_init_from_args(editor, argc, argv);
    if (rv != EON_OK) break;

    _editor_init_status(editor);
    _editor_init_bviews(editor, argc, argv);
    _editor_init_or_deinit_commands(editor, 0);
    _editor_init_headless_mode(editor);
    _editor_init_startup_macro(editor);

#ifdef WITH_PLUGINS
    load_plugins(editor);
#endif

  } while (0);

  editor->is_in_init = 0;
  return rv;
}

// Run editor
int editor_run(editor_t* editor) {
  loop_context_t loop_ctx;
  memset(&loop_ctx, 0, sizeof(loop_context_t));
  _editor_resize(editor, -1, -1);
  _editor_loop(editor, &loop_ctx);
  return EON_OK;
}

int editor_deinit(editor_t* editor) {
  bview_t* bview;
  bview_t* bview_tmp1;
  bview_t* bview_tmp2;
  kmap_t* kmap;
  kmap_t* kmap_tmp;
  kmacro_t* macro;
  kmacro_t* macro_tmp;
  cmd_t* cmd;
  cmd_t* cmd_tmp;
  prompt_history_t* prompt_history;
  prompt_history_t* prompt_history_tmp;
  prompt_hnode_t* prompt_hnode;
  prompt_hnode_t* prompt_hnode_tmp1;
  prompt_hnode_t* prompt_hnode_tmp2;

#ifdef WITH_PLUGINS
  unload_plugins();
#endif

  _editor_init_or_deinit_commands(editor, 1);
  if (editor->status) bview_destroy(editor->status);

  CDL_FOREACH_SAFE2(editor->all_bviews, bview, bview_tmp1, bview_tmp2, all_prev, all_next) {
    CDL_DELETE2(editor->all_bviews, bview, all_prev, all_next);
    bview_destroy(bview);
  }

  HASH_ITER(hh, editor->kmap_map, kmap, kmap_tmp) {
    HASH_DEL(editor->kmap_map, kmap);
    _editor_destroy_kmap(kmap, kmap->bindings->children);
    if (kmap->default_cmd_name) free(kmap->default_cmd_name);

    free(kmap->bindings);
    free(kmap->name);
    free(kmap);
  }

  HASH_ITER(hh, editor->macro_map, macro, macro_tmp) {
    HASH_DEL(editor->macro_map, macro);
    if (macro->inputs) free(macro->inputs);
    if (macro->name) free(macro->name);
    free(macro);
  }

  HASH_ITER(hh, editor->cmd_map, cmd, cmd_tmp) {
    HASH_DEL(editor->cmd_map, cmd);
    _editor_destroy_cmd(editor, cmd);
  }

  HASH_ITER(hh, editor->prompt_history, prompt_history, prompt_history_tmp) {
    HASH_DEL(editor->prompt_history, prompt_history);
    free(prompt_history->prompt_str);
    CDL_FOREACH_SAFE(prompt_history->prompt_hlist, prompt_hnode, prompt_hnode_tmp1, prompt_hnode_tmp2) {
      CDL_DELETE(prompt_history->prompt_hlist, prompt_hnode);
      free(prompt_hnode->data);
      free(prompt_hnode);
    }
    free(prompt_history);
  }

  if (editor->macro_record) {
    if (editor->macro_record->inputs)
      free(editor->macro_record->inputs);

    free(editor->macro_record);
  }

  _editor_destroy_syntax_map(editor->syntax_map);
  if (editor->kmap_init_name) free(editor->kmap_init_name);
  if (editor->insertbuf) free(editor->insertbuf);
  if (editor->ttyfd) close(editor->ttyfd);
  if (editor->startup_macro_name) free(editor->startup_macro_name);

  return EON_OK;
}

int editor_close_prompt(editor_t * editor, bview_t * invoker) {
  bview_t * bview_tmp;
  editor->prompt->menu_callback = NULL;
  // free(editor->prompt->prompt_str);
  bview_tmp = editor->prompt;
  editor->prompt = NULL;
  editor_close_bview(editor, bview_tmp, NULL);
  editor_set_active(editor, invoker);
  return EON_OK;
}

int editor_set_prompt_str(editor_t * editor, char * str) {
  // editor->rect_prompt.x = strlen(str) + 1;
  editor->prompt->prompt_str = str;
  return EON_OK;
}

// Prompt user for input
int editor_prompt(editor_t* editor, char* prompt, editor_prompt_params_t* params, char** optret_answer) {
  bview_t* bview_tmp;
  loop_context_t loop_ctx;
  memset(&loop_ctx, 0, sizeof(loop_context_t));

  // Disallow nested prompts
  if (editor->prompt) {
    if (optret_answer) *optret_answer = NULL;

    return EON_ERR;
  }

  // Init loop_ctx
  loop_ctx.invoker = editor->active;
  loop_ctx.should_exit = 0;
  loop_ctx.prompt_answer = NULL;

  // Init prompt
  editor->rect_prompt.x = strlen(prompt) + 1;
  editor_open_bview(editor, NULL, EON_BVIEW_TYPE_PROMPT, NULL, 0, 1, 0, &editor->rect_prompt, NULL, &editor->prompt);
  editor_set_prompt_str(editor, prompt);

  if (params && params->prompt_cb) bview_add_listener(editor->prompt, params->prompt_cb, params->prompt_cb_udata);
  bview_push_kmap(editor->prompt, params && params->kmap ? params->kmap : editor->kmap_prompt_input);

  // Insert data if present
  if (params && params->data && params->data_len > 0) {
    buffer_insert(editor->prompt->buffer, 0, params->data, params->data_len, NULL);
    mark_move_eol(editor->prompt->active_cursor->mark);
  }

  // Loop inside prompt
  _editor_loop(editor, &loop_ctx);

  // Set answer
  if (optret_answer) {
    *optret_answer = loop_ctx.prompt_answer;

  } else if (loop_ctx.prompt_answer) {
    free(loop_ctx.prompt_answer);
    loop_ctx.prompt_answer = NULL;
  }

  // Restore previous focus
  editor_close_prompt(editor, loop_ctx.invoker);

  return EON_OK;
}

// Show menu in editor (file browser, grep results, etc)
int editor_page_menu(editor_t* editor, cb_func_t callback, char* opt_buf_data, int opt_buf_data_len, async_proc_t* opt_aproc, bview_t** optret_menu) {
  bview_t* menu;
  editor_open_bview(editor, NULL, EON_BVIEW_TYPE_EDIT, NULL, 0, 1, 0, &editor->rect_edit, NULL, &menu);
  menu->is_menu = 1;
  menu->menu_callback = callback;
  bview_push_kmap(menu, editor->kmap_menu);

  if (opt_aproc) {
    async_proc_set_owner(opt_aproc, menu, &(menu->async_proc));
  }

  if (opt_buf_data) {
    mark_insert_before(menu->active_cursor->mark, opt_buf_data, opt_buf_data_len);
  }

  if (optret_menu) *optret_menu = menu;

  return EON_OK;
}


int editor_prompt_menu(editor_t* editor, cb_func_t callback, char* prompt, int prompt_len) {

  editor_open_bview(editor, NULL, EON_BVIEW_TYPE_PROMPT, NULL, 0, 1, 0, &editor->rect_prompt, NULL, &editor->prompt);
  editor_set_prompt_str(editor, prompt);

  bview_push_kmap(editor->prompt, editor->kmap_prompt_menu);

  editor->prompt->menu_callback = callback;
  // if (params && params->prompt_cb) bview_add_listener(editor->prompt, params->prompt_cb, params->prompt_cb_udata);

  // Insert data if present
//  if (opt_buf_data && opt_buf_data_len > 0) {
//    buffer_insert(editor->prompt->buffer, 0, opt_buf_data, opt_buf_data_len, NULL);
//    mark_move_eol(editor->prompt->active_cursor->mark);
//  }

  return EON_OK;
}

int _editor_open_dir(editor_t* editor, bview_t * bview, char* opt_path, int opt_path_len) {
  cmd_context_t ctx;

  memset(&ctx, 0, sizeof(cmd_context_t));
  ctx.editor = editor;
  ctx.static_param = strndup(opt_path, opt_path_len);
  ctx.bview = bview;
  bview->is_menu = 1;

  cmd_browse(&ctx);

  editor_close_bview(editor, bview, NULL);
  free(ctx.static_param);
  ctx.static_param = NULL;

  return EON_OK;
}

// Open a bview
int editor_open_bview(editor_t* editor, bview_t* parent, int type, char* opt_path, int opt_path_len, int make_active, bint_t linenum, bview_rect_t* opt_rect, buffer_t* opt_buffer, bview_t** optret_bview) {

  bview_t* bview;
  int found = 0;
  // debug("Opening bview [%d], path %s, buffer len %ld\n", type, opt_path, opt_buffer == NULL ? -1 : opt_buffer->byte_count);

  if (opt_path) { // Check if already open and not dirty
    CDL_FOREACH2(editor->all_bviews, bview, all_next) {
      // debug("Checking if bview (%s) matches path %s\n", bview->buffer && bview->buffer->path, opt_path);
      if (bview->buffer && bview->buffer->path && strcmp(opt_path, bview->buffer->path) == 0) {
        found = 1;
        break;
      }
    }
  }

  // Make new bview if not already open
  if (!found) {
    // debug("Initializing bview with path: %s\n", opt_path);
    char* opt_cwd = NULL, *path_cpy = NULL;
    if (opt_path != NULL) {
      path_cpy = strdup(opt_path);
      opt_cwd = dirname(path_cpy);
    }

    bview = bview_new_cwd(editor, opt_path, opt_path_len, opt_cwd, opt_buffer);
    bview->type = type;
    CDL_APPEND2(editor->all_bviews, bview, all_prev, all_next);
    if (!parent) {
      DL_APPEND2(editor->top_bviews, bview, top_prev, top_next);
    } else {
      parent->split_child = bview;
    }

    free(path_cpy);
  }

  if (make_active) {
    editor_set_active(editor, bview);
  }

  if (!found && opt_rect) {
    bview_resize(bview, opt_rect->x, opt_rect->y, opt_rect->w, opt_rect->h);
  }

  if (linenum > 0) {
    mark_move_to(bview->active_cursor->mark, linenum - 1, 0);
    bview_center_viewport_y(bview);
  }

  if (optret_bview) {
    *optret_bview = bview;
  }

  if (!found && opt_path && util_is_dir(opt_path)) {
    _editor_open_dir(editor, bview, opt_path, opt_path_len);
  }

  return EON_OK;
}

// Close a bview
int editor_close_bview(editor_t* editor, bview_t* bview, int* optret_num_closed) {
  int rc;

  if (optret_num_closed) *optret_num_closed = 0;

  // debug("Closing bview with path: %s\n", bview->buffer->path);
  if ((rc = _editor_close_bview_inner(editor, bview, optret_num_closed)) == EON_OK) {
    _editor_resize(editor, editor->w, editor->h);
  }

  return rc;
}

// Set the active bview
int editor_set_active(editor_t* editor, bview_t* bview) {
  if (!_editor_bview_exists(editor, bview)) {
    EON_RETURN_ERR(editor, "No bview %p in editor->all_bviews", bview);

  } else if (editor->prompt) {
    EON_RETURN_ERR(editor, "Cannot abandon prompt for bview %p", bview);
  }

  editor->active = bview;

  if (EON_BVIEW_IS_EDIT(bview)) {
    editor->active_edit = bview;
    editor->active_edit_root = bview_get_split_root(bview);
  }

  bview_rectify_viewport(bview);
  return EON_OK;
}

// Set macro toggle key
static int _editor_set_macro_toggle_key(editor_t* editor, char* key) {
  return _editor_key_to_input(key, &editor->macro_toggle_key);
}

// Return 1 if bview exists in editor, else return 0
static int _editor_bview_exists(editor_t* editor, bview_t* bview) {
  bview_t* tmp;
  CDL_FOREACH2(editor->all_bviews, tmp, all_next) {
    if (tmp == bview) return 1;
  }
  return 0;
}

// Return number of EDIT bviews open
int editor_bview_edit_count(editor_t* editor) {
  int count;
  bview_t* bview;
  count = 0;
  CDL_FOREACH2(editor->all_bviews, bview, all_next) {
    if (EON_BVIEW_IS_EDIT(bview)) count += 1;
  }
  return count;
}

// Return number of bviews displaying buffer
int editor_count_bviews_by_buffer(editor_t* editor, buffer_t* buffer) {
  int count;
  bview_t* bview;
  count = 0;
  CDL_FOREACH2(editor->all_bviews, bview, all_next) {
    if (bview->buffer == buffer) count += 1;
  }
  return count;
}

// Register a command
static int _editor_register_cmd_fn(editor_t* editor, char* name, int (*func)(cmd_context_t* ctx)) {
  cmd_t cmd = {0};
  cmd.name = name;
  cmd.func = func;
  return editor_register_cmd(editor, &cmd);
}

// Register a command (extended)
int editor_register_cmd(editor_t* editor, cmd_t* cmd) {
  cmd_t* existing_cmd;
  cmd_t* new_cmd;
  HASH_FIND_STR(editor->cmd_map, cmd->name, existing_cmd);

  if (existing_cmd) return EON_ERR;

  new_cmd = calloc(1, sizeof(cmd_t));
  *new_cmd = *cmd;
  new_cmd->name = strdup(new_cmd->name);
  HASH_ADD_KEYPTR(hh, editor->cmd_map, new_cmd->name, strlen(new_cmd->name), new_cmd);
  return EON_OK;
}

// Get input from either macro or user
int editor_get_input(editor_t* editor, loop_context_t* loop_ctx, cmd_context_t* ctx) {
  ctx->is_user_input = 0;

  if (editor->macro_apply
      && editor->macro_apply_input_index < editor->macro_apply->inputs_len) {
    // Get input from macro
    ctx->input = editor->macro_apply->inputs[editor->macro_apply_input_index];
    editor->macro_apply_input_index += 1;

  } else {
    // Get input from user
    if (editor->macro_apply) {
      // Clear macro if present
      editor->macro_apply = NULL;
      editor->macro_apply_input_index = 0;
    }

    if (editor->headless_mode) {
      // Bail if in headless mode
      loop_ctx->should_exit = 1;
      return EON_ERR;

    } else {
      // Get input from user
      _editor_get_user_input(editor, ctx);
      ctx->is_user_input = 1;
    }
  }

  if (editor->is_recording_macro && editor->macro_record) {
    // Record macro input
    _editor_record_macro_input(editor->macro_record, &ctx->input);
  }

  return EON_OK;
}

// Display the editor
int editor_display(editor_t* editor) {
  bview_t* bview;

  if (editor->headless_mode) return EON_OK;

  tb_clear_buffer();
  bview_draw(editor->active_edit_root);
  bview_draw(editor->status);

  if (editor->prompt) bview_draw(editor->prompt);

  DL_FOREACH2(editor->top_bviews, bview, top_next) {
    _editor_draw_cursors(editor, bview);
  }

  tb_render();
  return EON_OK;
}

// Return 1 if we should skip reading rc files
static int _editor_should_skip_rc(char** argv) {
  int skip = 0;

  while (*argv) {
    if (strcmp("-h", *argv) == 0 || strcmp("-N", *argv) == 0) {
      skip = 1;
      break;
    }

    argv++;
  }

  return skip;
}

// Close a bview
static int _editor_close_bview_inner(editor_t* editor, bview_t* bview, int *optret_num_closed) {
  if (!_editor_bview_exists(editor, bview)) {
    EON_RETURN_ERR(editor, "No bview %p in editor->all_bviews", bview);
  }

  if (bview->split_child) {
    _editor_close_bview_inner(editor, bview->split_child, optret_num_closed);
  }

  if (bview->split_parent) {
    bview->split_parent->split_child = NULL;
    editor_set_active(editor, bview->split_parent);

  } else {
    if (bview->all_next && bview->all_next != bview && EON_BVIEW_IS_EDIT(bview->all_next)) {
      editor_set_active(editor, bview->all_next);

    } else if (bview->all_prev && bview->all_prev != bview && EON_BVIEW_IS_EDIT(bview->all_prev)) {
      editor_set_active(editor, bview->all_prev);

    } else {
      editor_open_bview(editor, NULL, EON_BVIEW_TYPE_EDIT, NULL, 0, 1, 0, &editor->rect_edit, NULL, NULL);
    }
  }

  if (!bview->split_parent) {
    DL_DELETE2(editor->top_bviews, bview, top_prev, top_next);
  }

  CDL_DELETE2(editor->all_bviews, bview, all_prev, all_next);
  bview_destroy(bview);

  if (optret_num_closed) *optret_num_closed += 1;

  return EON_OK;
}

// Destroy a command
static int _editor_destroy_cmd(editor_t* editor, cmd_t* cmd) {
  free(cmd->name);
  free(cmd);
  return EON_OK;
}

// Invoked when user hits enter in a prompt_input
static int _editor_prompt_input_submit(cmd_context_t* ctx) {
  bint_t answer_len;
  char* answer;

  buffer_get(ctx->bview->buffer, &answer, &answer_len);
  ctx->loop_ctx->prompt_answer = strndup(answer, answer_len);
  _editor_prompt_history_append(ctx, ctx->loop_ctx->prompt_answer);
  ctx->loop_ctx->should_exit = 1;

  return EON_OK;
}

// Invoke when user hits tab in a prompt_input
static int _editor_prompt_input_complete(cmd_context_t* ctx) {

/*
  // TODO: rewrite this

  loop_context_t* loop_ctx;
  loop_ctx = ctx->loop_ctx;

  char* cmd;
  char* cmd_arg;
  char* terms;

  size_t terms_len;
  int num_terms;

  char* term;
  int term_index;

  // Update tab_complete_term and tab_complete_index
  if (loop_ctx->last_cmd && loop_ctx->last_cmd->func == _editor_prompt_input_complete) {
    loop_ctx->tab_complete_index += 1;

  } else if (ctx->bview->buffer->first_line->data_len < EON_LOOP_CTX_MAX_COMPLETE_TERM_SIZE) {
    snprintf(
      loop_ctx->tab_complete_term,
      EON_LOOP_CTX_MAX_COMPLETE_TERM_SIZE,
      "%.*s",
      (int)ctx->bview->buffer->first_line->data_len,
      ctx->bview->buffer->first_line->data
    );
    loop_ctx->tab_complete_index = 0;

  } else {
    return EON_OK;
  }

  // Assemble compgen command
  cmd_arg = util_escape_shell_arg(loop_ctx->tab_complete_term, strlen(loop_ctx->tab_complete_term));
  int res = asprintf(&cmd, "compgen -f %s 2>/dev/null | sort", cmd_arg);

  // Run compgen command
  terms = NULL;
  terms_len = 0;
  util_shell_exec(ctx->editor, cmd, 1, NULL, 0, "bash", &terms, &terms_len);
  free(cmd);
  free(cmd_arg);

  // Get number of terms
  // TODO valgrind thinks there's an error here
  num_terms = 0;
  term = strchr(terms, '\n');

  while (term) {
    num_terms += 1;
    term = strchr(term + 1, '\n');
  }

  // Bail if no terms
  if (num_terms < 1) {
    free(terms);
    return EON_OK;
  }

  // Determine term index
  term_index = loop_ctx->tab_complete_index % num_terms;

  // Set prompt input to term
  term = strtok(terms, "\n");

  while (term != NULL) {
    if (term_index == 0) {
      buffer_set(ctx->bview->buffer, term, strlen(term));
      mark_move_eol(ctx->cursor->mark);
      break;

    } else {
      term_index -= 1;
    }

    term = strtok(NULL, "\n");
  }

  free(terms);
*/

  return EON_OK;
}

// Find or add a prompt history entry for the current prompt
static prompt_history_t* _editor_prompt_find_or_add_history(cmd_context_t* ctx, prompt_hnode_t** optret_prompt_hnode) {
  prompt_history_t* prompt_history;
  HASH_FIND_STR(ctx->editor->prompt_history, ctx->bview->prompt_str, prompt_history);

  if (!prompt_history) {
    prompt_history = calloc(1, sizeof(prompt_history_t));
    prompt_history->prompt_str = strdup(ctx->bview->prompt_str);
    HASH_ADD_KEYPTR(hh, ctx->editor->prompt_history, prompt_history->prompt_str, strlen(prompt_history->prompt_str), prompt_history);
  }

  if (!ctx->loop_ctx->prompt_hnode) {
    ctx->loop_ctx->prompt_hnode = prompt_history->prompt_hlist
                                  ? prompt_history->prompt_hlist->prev
                                  : NULL;
  }

  if (optret_prompt_hnode) {
    *optret_prompt_hnode = ctx->loop_ctx->prompt_hnode;
  }

  return prompt_history;
}

// Prompt history up
static int _editor_prompt_history_up(cmd_context_t* ctx) {
  prompt_hnode_t* prompt_hnode;
  _editor_prompt_find_or_add_history(ctx, &prompt_hnode);

  if (prompt_hnode) {
    ctx->loop_ctx->prompt_hnode = prompt_hnode->prev;
    buffer_set(ctx->buffer, prompt_hnode->data, prompt_hnode->data_len);
  }

  return EON_OK;
}

// Prompt history down
static int _editor_prompt_history_down(cmd_context_t* ctx) {
  prompt_hnode_t* prompt_hnode;
  _editor_prompt_find_or_add_history(ctx, &prompt_hnode);

  if (prompt_hnode) {
    ctx->loop_ctx->prompt_hnode = prompt_hnode->next;
    buffer_set(ctx->buffer, prompt_hnode->data, prompt_hnode->data_len);
  }

  return EON_OK;
}

// Prompt history append
static int _editor_prompt_history_append(cmd_context_t* ctx, char* data) {
  prompt_history_t* prompt_history;
  prompt_hnode_t* prompt_hnode;
  prompt_history = _editor_prompt_find_or_add_history(ctx, NULL);
  prompt_hnode = calloc(1, sizeof(prompt_hnode_t));
  prompt_hnode->data = strdup(data);
  prompt_hnode->data_len = (bint_t)strlen(data);
  CDL_APPEND(prompt_history->prompt_hlist, prompt_hnode);
  return EON_OK;
}

// Invoked when user hits a in a prompt_yna
static int _editor_prompt_yna_all(cmd_context_t* ctx) {
  ctx->loop_ctx->prompt_answer = EON_PROMPT_ALL;
  ctx->loop_ctx->should_exit = 1;
  return EON_OK;
}

// Invoked when user hits y in a prompt_yn(a)
static int _editor_prompt_yn_yes(cmd_context_t* ctx) {
  ctx->loop_ctx->prompt_answer = EON_PROMPT_YES;
  ctx->loop_ctx->should_exit = 1;
  return EON_OK;
}

// Invoked when user hits n in a prompt_yn(a)
static int _editor_prompt_yn_no(cmd_context_t* ctx) {
  ctx->loop_ctx->prompt_answer = EON_PROMPT_NO;
  ctx->loop_ctx->should_exit = 1;
  return EON_OK;
}

// Invoked when user cancels (Ctrl-C) a prompt_(input|yn), or hits any key in a prompt_ok
static int _editor_prompt_cancel(cmd_context_t* ctx) {
  ctx->loop_ctx->prompt_answer = NULL;
  ctx->loop_ctx->should_exit = 1;
  return EON_OK;
}

// Invoked when user hits enter in a menu
static int _editor_menu_submit(cmd_context_t* ctx) {
  if (ctx->bview->menu_callback) return ctx->bview->menu_callback(ctx, "submit");

  return EON_OK;
}

// Invoked when user hits C-c in a menu
static int _editor_menu_cancel(cmd_context_t* ctx) {
  if (ctx->bview->async_proc) async_proc_destroy(ctx->bview->async_proc, 1);
  // if (ctx->bview->menu_callback) return ctx->bview->menu_callback(ctx, NULL);

  return EON_OK;
}

static int _editor_prompt_menu_cancel(cmd_context_t* ctx) {
  if (ctx->editor->prompt->menu_callback)
    return ctx->editor->prompt->menu_callback(ctx, NULL);

  return EON_OK;
}

// Invoked when user hits up in a prompt_menu
static int _editor_prompt_menu_up(cmd_context_t* ctx) {
  if (ctx->editor->prompt->menu_callback)
    return ctx->editor->prompt->menu_callback(ctx, "up");

  // mark_move_vert(ctx->editor->active_edit->active_cursor->mark, -1);
  // bview_rectify_viewport(ctx->editor->active_edit);
  return EON_OK;
}

// Invoked when user hits down in a prompt_menu
static int _editor_prompt_menu_down(cmd_context_t* ctx) {
  if (ctx->editor->prompt->menu_callback)
    return ctx->editor->prompt->menu_callback(ctx, "down");

  // mark_move_vert(ctx->editor->active_edit->active_cursor->mark, 1);
  // bview_rectify_viewport(ctx->editor->active_edit);
  return EON_OK;
}

// Invoked when user hits page-up in a prompt_menu
static int _editor_prompt_menu_page_up(cmd_context_t* ctx) {
  if (ctx->editor->prompt->menu_callback)
    return ctx->editor->prompt->menu_callback(ctx, "pageup");

  // mark_move_vert(ctx->editor->active_edit->active_cursor->mark, -1 * ctx->editor->active_edit->rect_buffer.h);
  // bview_zero_viewport_y(ctx->editor->active_edit);
  return EON_OK;
}

// Invoked when user hits page-down in a prompt_menu
static int _editor_prompt_menu_page_down(cmd_context_t* ctx) {
  if (ctx->editor->prompt->menu_callback)
    return ctx->editor->prompt->menu_callback(ctx, "pagedown");

  // mark_move_vert(ctx->editor->active_edit->active_cursor->mark, ctx->editor->active_edit->rect_buffer.h);
  // bview_zero_viewport_y(ctx->editor->active_edit);
  return EON_OK;
}

// Invoked when user hits down in a prompt_isearch
static int _editor_prompt_isearch_next(cmd_context_t* ctx) {
  if (ctx->editor->active_edit->isearch_rule) {
    mark_move_next_cre_nudge(ctx->editor->active_edit->active_cursor->mark, ctx->editor->active_edit->isearch_rule->cre);
    bview_center_viewport_y(ctx->editor->active_edit);
  }

  return EON_OK;
}

// Invoked when user hits up in a prompt_isearch
static int _editor_prompt_isearch_prev(cmd_context_t* ctx) {
  if (ctx->editor->active_edit->isearch_rule) {
    mark_move_prev_cre(ctx->editor->active_edit->active_cursor->mark, ctx->editor->active_edit->isearch_rule->cre);
    bview_center_viewport_y(ctx->editor->active_edit);
  }

  return EON_OK;
}

// Invoked when user hits Ctrl-F while on the search prompt
static int _editor_prompt_toggle_replace(cmd_context_t* ctx) {
  // ctx->loop_ctx->prompt_answer = NULL;
  _editor_prompt_cancel(ctx);

  ctx->editor->macro_record = calloc(1, sizeof(kmacro_t));
  ctx->editor->macro_record->name = "foo";

  kinput_t * input = calloc(1, sizeof(kinput_t));
  input->ch   = 0;
  input->key  = TB_KEY_CTRL_H;
  input->meta = TB_META_CTRL;
  _editor_record_macro_input(ctx->editor->macro_record, input);

  ctx->editor->macro_apply = ctx->editor->macro_record;
  ctx->editor->macro_apply_input_index = 0;

  return EON_OK;
}

// Drops a cursor on each isearch match
static int _editor_prompt_isearch_drop_cursors(cmd_context_t* ctx) {
  bview_t* bview;
  mark_t* mark;
  pcre* cre;
  cursor_t* orig_cursor;
  cursor_t* last_cursor;
  bview = ctx->editor->active_edit;

  if (!bview->isearch_rule) return EON_OK;

  orig_cursor = bview->active_cursor;
  mark = bview->active_cursor->mark;
  cre = bview->isearch_rule->cre;
  mark_move_beginning(mark);
  last_cursor = NULL;

  while (mark_move_next_cre_nudge(mark, cre) == MLBUF_OK) {
    if (mark->col == 0 && mark->bline->line_index == 0) {
      break; // otherwise hell breaks loose. FIXME: we should skip to the next one.
    }

    bview_add_cursor(bview, mark->bline, mark->col, &last_cursor);
  }

  if (last_cursor) bview_remove_cursor(bview, last_cursor);

  bview->active_cursor = orig_cursor;
  bview_center_viewport_y(bview);
  ctx->loop_ctx->prompt_answer = NULL;
  ctx->loop_ctx->should_exit = 1;
  return EON_OK;
}

// Run editor loop
static void _editor_loop(editor_t* editor, loop_context_t* loop_ctx) {
  cmd_t* cmd;
  cmd_context_t cmd_ctx;
  char event_name[64];

  // Increment loop_depth
  editor->loop_depth += 1;

  // Init cmd_context
  memset(&cmd_ctx, 0, sizeof(cmd_context_t));
  cmd_ctx.editor   = editor;
  cmd_ctx.loop_ctx = loop_ctx;
  cmd_ctx.cursor   = editor->active ? editor->active->active_cursor : NULL;
  cmd_ctx.bview    = cmd_ctx.cursor ? cmd_ctx.cursor->bview : NULL;
  cmd_ctx.buffer   = cmd_ctx.bview->buffer;

  // Loop until editor should exit
  while (!loop_ctx->should_exit) {
    // Set loop_ctx
    editor->loop_ctx = loop_ctx;

    // Display editor
    if (!editor->is_display_disabled) {
      editor_display(editor);
    }

    // Check for async io
    // async_proc_drain_all will bail and return 0 if there's any tty data
    if (editor->async_procs && async_proc_drain_all(editor->async_procs, &editor->ttyfd)) {
      continue;
    }

    // Get input
    if (editor_get_input(editor, loop_ctx, &cmd_ctx) == EON_ERR) {
      break;
    }

// #define SHOW_KEYS 1
#ifdef SHOW_KEYS
    if (cmd_ctx.is_user_input) {
      //  rect_printf(editor->rect_status, editor->rect_status.w - 20, 0, TB_DEFAULT, TB_DEFAULT,
      //    "k:%d/ch:%d/m:%d", cmd_ctx.input.key, cmd_ctx.input.ch, cmd_ctx.input.meta);
      printf("k:%d/ch:%d/m:%d\n", cmd_ctx.input.key, cmd_ctx.input.ch, cmd_ctx.input.meta);
    }
#endif

    // Toggle macro?
    if (_editor_maybe_toggle_macro(editor, &cmd_ctx.input)) {
      continue;
    }

    if ((cmd = _editor_get_command(editor, &cmd_ctx, NULL)) != NULL) {
      // Found cmd in kmap trie, now execute
      // printf("cmd: %s\n", cmd->name);

      // ensure these are set before performing any checks.
      cmd_ctx.cmd    = cmd;
      cmd_ctx.cursor = editor->active ? editor->active->active_cursor : NULL;
      cmd_ctx.bview  = cmd_ctx.cursor ? cmd_ctx.cursor->bview : NULL;
      cmd_ctx.buffer = cmd_ctx.bview->buffer;

      if (cmd_ctx.is_user_input && cmd->func == cmd_insert_data) {
        if (EON_BVIEW_IS_EDIT(cmd_ctx.bview) && cmd_ctx.cursor->is_anchored) {
          cmd_delete_before(&cmd_ctx);
        }

        _editor_ingest_paste(editor, &cmd_ctx);
      }

#ifdef WITH_PLUGINS
      if (cmd->name[0] != '_') {
        snprintf(event_name, strlen(cmd->name) + 6, "before.%s", cmd->name + 4);
        trigger_plugin_event(event_name, cmd_ctx);
      }
#endif

      cmd->func(&cmd_ctx); // call the function itself

#ifdef WITH_PLUGINS
      if (cmd->name[0] != '_') {
        snprintf(event_name, strlen(cmd->name) + 7, "after.%s", cmd->name + 4);
        trigger_plugin_event(event_name, cmd_ctx);
      }
#endif

      loop_ctx->binding_node = NULL;
      loop_ctx->wildcard_params_len = 0;
      loop_ctx->numeric_params_len = 0;
      loop_ctx->last_cmd = cmd;
      // printf("cmd finished %s\n", cmd->name);

    } else if (loop_ctx->need_more_input) {
      // Need more input to find
    } else {
      // Not found, bad command
      loop_ctx->binding_node = NULL;
    }
  }

  // Free pastebuf if present
  if (cmd_ctx.pastebuf) free(cmd_ctx.pastebuf);

  // Free last_insert
  str_free(&loop_ctx->last_insert);

  // Decrement loop_depth
  editor->loop_depth -= 1;
}

// If input == editor->macro_toggle_key, toggle macro mode and return 1. Else
// return 0.
static int _editor_maybe_toggle_macro(editor_t* editor, kinput_t* input) {
  char* name;

  if (memcmp(input, &editor->macro_toggle_key, sizeof(kinput_t)) != 0) {
    return 0;
  }

  if (editor->is_recording_macro) {
    // Stop recording macro and add to map
    if (editor->macro_record->inputs_len > 0) {
      // Remove toggle key from macro inputs
      editor->macro_record->inputs_len -= 1; // TODO This is hacky
    }

    HASH_ADD_STR(editor->macro_map, name, editor->macro_record);
    editor->macro_record = NULL;
    editor->is_recording_macro = 0;

  } else {
    // Get macro name and start recording
    editor_prompt(editor, "record_macro: Name?", NULL, &name);

    if (!name) return 1;

    editor->macro_record = calloc(1, sizeof(kmacro_t));
    editor->macro_record->name = name;
    editor->is_recording_macro = 1;
  }

  return 1;
}

// Resize the editor
static void _editor_resize(editor_t* editor, int w, int h) {
  bview_t* bview;
  bview_rect_t* bounds;

  editor->w = w >= 0 ? w : tb_width();
  editor->h = h >= 0 ? h : tb_height();
  editor->bview_tab_width = 20; // TODO: shrink dynamically

  editor->rect_edit.x = 0;
  editor->rect_edit.y = 0;
  editor->rect_edit.w = editor->w;
  editor->rect_edit.h = editor->h - 1;

  editor->rect_status.x = 0;
  editor->rect_status.y = editor->h - 1;
  editor->rect_status.w = editor->w;
  editor->rect_status.h = 1;

  editor->rect_prompt.x = 0;
  editor->rect_prompt.y = editor->h - 1;
  editor->rect_prompt.w = editor->w;
  editor->rect_prompt.h = 1;

  DL_FOREACH2(editor->top_bviews, bview, top_next) {
    if (EON_BVIEW_IS_PROMPT(bview)) {
      bounds = &editor->rect_prompt;

    } else if (EON_BVIEW_IS_STATUS(bview)) {
      bounds = &editor->rect_status;

    } else {
      if (bview->split_parent) continue;
      bounds = &editor->rect_edit;
    }

    bview_resize(bview, bounds->x, bounds->y, bounds->w, bounds->h);
  }
}

// Draw bviews cursors recursively
static void _editor_draw_cursors(editor_t* editor, bview_t* bview) {
  if (EON_BVIEW_IS_EDIT(bview) && bview_get_split_root(bview) != editor->active_edit_root) {
    return;
  }

  bview_draw_cursor(bview, bview == editor->active ? 1 : 0);

  if (bview->split_child) {
    _editor_draw_cursors(editor, bview->split_child);
  }
}

static int _get_bview_count(cmd_context_t * ctx) {
  bview_t* bview_tmp;
  int bview_count = 0;

  CDL_FOREACH2(ctx->editor->all_bviews, bview_tmp, all_next) {
    if (EON_BVIEW_IS_EDIT(bview_tmp)) {
      bview_count += 1;
    }
  }

  return bview_count;
}

static int _find_bview_at(cmd_context_t * ctx, int offset) {
  int bview_count = _get_bview_count(ctx);

  int from = -1, index = bview_count, to = -1;

  while (index--) {
    to   = to == -1 ? ctx->editor->w : from;
    from = (index) * ctx->editor->bview_tab_width;

    if (from < offset && offset < to) {
      break;
    }
  }

  return index;
}

static void _open_bview_at(cmd_context_t * ctx, int offset) {
  bview_t* bview_tmp;
  int index = _find_bview_at(ctx, offset);

  if (index == -1) return;

  int a = 0;
  CDL_FOREACH2(ctx->editor->all_bviews, bview_tmp, all_next) {
    if (EON_BVIEW_IS_EDIT(bview_tmp)) {
      if (a++ == index) {
        editor_set_active(ctx->editor, bview_tmp);
      }
    }
  }
}

static void _close_bview_at(cmd_context_t * ctx, int offset) {
  int bview_count = _get_bview_count(ctx);

  if (bview_count <= 1) return;

  bview_t* bview_tmp;
  int index = _find_bview_at(ctx, offset);

  if (index == -1) return;

  int a = 0;
  CDL_FOREACH2(ctx->editor->all_bviews, bview_tmp, all_next) {
    if (EON_BVIEW_IS_EDIT(bview_tmp)) {
      if (a++ == index && (!bview_tmp->buffer->is_unsaved || EON_BVIEW_IS_MENU(bview_tmp))) {
        editor_close_bview(ctx->editor, bview_tmp, NULL);
      }
    }
  }
}

static int mouse_down = 0;
struct click {
  int type;
  int x;
  int y;
};

static struct click last_click = { -1, -1, -1 };

static void _handle_mouse_event(cmd_context_t* ctx, tb_event_t ev) {
  switch (ev.key) {
  case TB_KEY_MOUSE_LEFT:
    if (ev.y == 0) {
      _open_bview_at(ctx, ev.x);

    } else if (ev.y == ctx->editor->h - 1) {
      // clicked status bar
    } else {
      int is_double_click = ev.h == 2;

      if (mouse_down == 0 && is_double_click) {
        if (EON_BVIEW_IS_MENU(ctx->editor->active))
          _editor_menu_submit(ctx);
        else
          cmd_select_current_word(ctx);
      } else {
        cmd_mouse_move(ctx, mouse_down, ev.x, ev.y);
        mouse_down = 1;
      }
    }

    break;

  case TB_KEY_MOUSE_MIDDLE:
    if (ev.y == 0) {
      _close_bview_at(ctx, ev.x);

    } else if (ev.y == ctx->editor->h - 1) {

    } else {
      if (ctx->cursor->is_anchored) {
        cmd_copy(ctx);

      } else {
        // should we move the cursor before pasting?
        // cmd_mouse_move(ctx, 0, ev.x, ev.y);
        cmd_uncut(ctx);
      }
    }

    break;

  case TB_KEY_MOUSE_RIGHT:
    cmd_toggle_mouse_mode(ctx);
    break;

  case TB_KEY_MOUSE_WHEEL_UP:
    cmd_scroll_up(ctx);
    break;

  case TB_KEY_MOUSE_WHEEL_DOWN:
    cmd_scroll_down(ctx);
    break;

  case TB_KEY_MOUSE_RELEASE:
    if (ev.y == 0) {
      // _open_bview_at(ctx, ev.x);
    } else if (ev.y == ctx->editor->h - 1) {
      // clicked status bar
    } else {
      if (ev.x != last_click.x && ev.y != last_click.y)
        cmd_mouse_move(ctx, mouse_down, ev.x, ev.y);

      mouse_down = 0;
    }

    break;

  }

  last_click.type = ev.key;
  last_click.x = ev.x;
  last_click.y = ev.y;
}

// Get user input
static void _editor_get_user_input(editor_t* editor, cmd_context_t* ctx) {
  int rc;
  tb_event_t ev;
  ev.key  = 0;
  ev.meta = 0;
  ev.key  = 0;

  // Reset pastebuf
  ctx->pastebuf_len = 0;

  // Use pastebuf_leftover is present
  if (ctx->has_pastebuf_leftover) {
    ctx->input = ctx->pastebuf_leftover;
    ctx->has_pastebuf_leftover = 0;
    return;
  }

  // Poll for event
  while (1) {
    rc = tb_poll_event(&ev);

    if (rc == -1) { // error
      continue;

    } else if (rc == TB_EVENT_MOUSE) {
      if (ctx->bview && editor->active == ctx->bview) {
        _handle_mouse_event(ctx, ev);
        editor_display(editor);
        continue;
      } else {
        EON_SET_ERR(editor, "No editor active, mouse event: %d/%d", ev.x, ev.y);
        editor_display(editor);
      }

    } else if (rc == TB_EVENT_RESIZE) {
      _editor_resize(editor, ev.w, ev.h);
      editor_display(editor);
      continue;
    }

    ctx->input = (kinput_t) { ev.ch, ev.key, ev.meta };
    // printf("ch %d, key %d, meta %d\n", ev.ch, ev.key, ev.meta);
    break;
  }
}

// Ingest available input until non-cmd_insert_data
static void _editor_ingest_paste(editor_t* editor, cmd_context_t* ctx) {
  int rc;
  tb_event_t ev;
  kinput_t input;
  cmd_t* cmd;
  memset(&input, 0, sizeof(kinput_t));

  // Reset pastebuf
  ctx->pastebuf_len = 0;

  // Peek events
  while (1) {
    // Expand pastebuf if needed
    if (ctx->pastebuf_len + 1 > ctx->pastebuf_size) {
      ctx->pastebuf_size += EON_PASTEBUF_INCR;
      ctx->pastebuf = realloc(ctx->pastebuf, sizeof(kinput_t) * ctx->pastebuf_size);
    }

    // Peek event
    rc = tb_peek_event(&ev, 0);

    if (rc == -1) {
      break; // Error

    } else if (rc == 0) {
      break; // Timeout

    } else if (rc == TB_EVENT_RESIZE) {
      // Resize
      _editor_resize(editor, ev.w, ev.h);
      editor_display(editor);
      break;
    }

    input = (kinput_t) { ev.ch, ev.key, ev.meta };
    // TODO check for macro key
    cmd = _editor_get_command(editor, ctx, &input);

    if (cmd && cmd->func == cmd_insert_data) {
      // Insert data; keep ingesting
      ctx->pastebuf[ctx->pastebuf_len++] = input;

    } else {
      // Not insert data; set leftover and stop ingesting
      ctx->has_pastebuf_leftover = 1;
      ctx->pastebuf_leftover = input;
      break;
    }
  }
}

// Copy input into macro buffer
static void _editor_record_macro_input(kmacro_t* macro, kinput_t* input) {
  if (!macro->inputs) {
    macro->inputs = calloc(8, sizeof(kinput_t));
    macro->inputs_len = 0;
    macro->inputs_cap = 8;

  } else if (macro->inputs_len + 1 > macro->inputs_cap) {
    macro->inputs_cap = macro->inputs_len + 8;
    macro->inputs = realloc(macro->inputs, macro->inputs_cap * sizeof(kinput_t));
  }

  memcpy(macro->inputs + macro->inputs_len, input, sizeof(kinput_t));
  macro->inputs_len += 1;
}

// Return command for input
static cmd_t* _editor_get_command(editor_t* editor, cmd_context_t* ctx, kinput_t* opt_peek_input) {
  loop_context_t* loop_ctx;
  kinput_t* input;
  kbinding_t* node;
  kbinding_t* binding;
  kmap_node_t* kmap_node;
  int is_top;
  int is_peek;
  int again;

  // Init some vars
  loop_ctx = ctx->loop_ctx;
  is_peek = opt_peek_input ? 1 : 0;
  input = opt_peek_input ? opt_peek_input : &ctx->input;
  kmap_node = editor->active->kmap_tail;
  node = loop_ctx->binding_node;
  is_top = (node == NULL ? 1 : 0);
  loop_ctx->need_more_input = 0;
  loop_ctx->binding_node = NULL;

  // Look for key binding
  while (kmap_node) {
    if (is_top) node = kmap_node->kmap->bindings;

    again = 0;
    binding = _editor_get_kbinding_node(node, input, loop_ctx, is_peek, &again);

    if (binding) {
      if (again) {
        // Need more input on current node
        if (!is_peek) {
          loop_ctx->need_more_input = 1;
          loop_ctx->binding_node = binding;
        }

        return NULL;

      } else if (binding->is_leaf) {
        // Found leaf!
        if (!is_peek) {
          ctx->static_param = binding->static_param;
        }

        return _editor_resolve_cmd(editor, &(binding->cmd), binding->cmd_name);

      } else if (binding->children) {
        // Need more input on next node
        if (!is_peek) {
          loop_ctx->need_more_input = 1;
          loop_ctx->binding_node = binding;
        }

        return NULL;

      } else {
        // This shouldn't happen... TODO err
        return NULL;
      }

    } else if (node == kmap_node->kmap->bindings) {
      // Binding not found at top level
      if (kmap_node->kmap->default_cmd_name) {
        // Fallback to default
        return _editor_resolve_cmd(editor, &(kmap_node->kmap->default_cmd), kmap_node->kmap->default_cmd_name);
      }

      if (kmap_node->kmap->allow_fallthru && kmap_node != kmap_node->prev) {
        // Fallback to previous kmap on stack
        kmap_node = kmap_node->prev;
        is_top = 1;

      } else {
        // Fallback not allowed or reached bottom
        return NULL;
      }

    } else {
      // Binding not found
      return NULL;
    }
  }

  // No more kmaps
  return NULL;
}

// Find binding by input in trie, taking into account numeric and wildcards patterns
static kbinding_t* _editor_get_kbinding_node(kbinding_t* node, kinput_t* input, loop_context_t* loop_ctx, int is_peek, int* ret_again) {
  kbinding_t* binding;
  kinput_t input_tmp;
  memset(&input_tmp, 0, sizeof(kinput_t));

  if (!is_peek) {
    // Look for numeric .. TODO can be more efficient about this
    if (input->ch >= '0' && input->ch <= '9') {
      if (!loop_ctx->numeric_node) {
        input_tmp = EON_KINPUT_NUMERIC;
        HASH_FIND(hh, node->children, &input_tmp, sizeof(kinput_t), binding);
        loop_ctx->numeric_node = binding;
      }

      if (loop_ctx->numeric_node) {
        if (loop_ctx->numeric_len < EON_LOOP_CTX_MAX_NUMERIC_LEN) {
          loop_ctx->numeric[loop_ctx->numeric_len] = (char)input->ch;
          loop_ctx->numeric_len += 1;
          *ret_again = 1;
          return node; // Need more input on this node
        }

        return NULL; // Ran out of `numeric` buffer .. TODO err
      }
    }

    // Parse/reset numeric buffer
    if (loop_ctx->numeric_len > 0) {
      if (loop_ctx->numeric_params_len < EON_LOOP_CTX_MAX_NUMERIC_PARAMS) {
        loop_ctx->numeric[loop_ctx->numeric_len] = '\0';
        loop_ctx->numeric_params[loop_ctx->numeric_params_len] = strtoul(loop_ctx->numeric, NULL, 10);
        loop_ctx->numeric_params_len += 1;
        loop_ctx->numeric_len = 0;
        node = loop_ctx->numeric_node; // Resume on numeric's children
        loop_ctx->numeric_node = NULL;

      } else {
        loop_ctx->numeric_len = 0;
        loop_ctx->numeric_node = NULL;
        return NULL; // Ran out of `numeric_params` space .. TODO err
      }
    }
  }

  // Look for input
  HASH_FIND(hh, node->children, input, sizeof(kinput_t), binding);

  if (binding) {
    return binding;
  }

  if (!is_peek) {
    // Look for wildcard
    input_tmp = EON_KINPUT_WILDCARD;
    HASH_FIND(hh, node->children, &input_tmp, sizeof(kinput_t), binding);

    if (binding) {
      if (loop_ctx->wildcard_params_len < EON_LOOP_CTX_MAX_WILDCARD_PARAMS) {
        loop_ctx->wildcard_params[loop_ctx->wildcard_params_len] = input->ch;
        loop_ctx->wildcard_params_len += 1;

      } else {
        return NULL; // Ran out of `wildcard_params` space .. TODO err
      }

      return binding;
    }
  }

  return NULL;
}

// Resolve a potentially unresolved cmd by name
static cmd_t* _editor_resolve_cmd(editor_t* editor, cmd_t** rcmd, char* cmd_name) {
  cmd_t* tcmd;
  cmd_t* cmd;
  cmd = NULL;

  if ((*rcmd) && !(*rcmd)->is_dead) {
    cmd = *rcmd;

  } else if (cmd_name) {
    HASH_FIND_STR(editor->cmd_map, cmd_name, tcmd);

    if (tcmd && !tcmd->is_dead) {
      *rcmd = tcmd;
      cmd = tcmd;
    }
  }

  return cmd;
}

// Return a kinput_t given a key name
static int _editor_key_to_input(char* key, kinput_t* ret_input) {
  int keylen;
  int mod;
  int meta = 0;
  uint32_t ch;
  keylen = strlen(key);
  memset(ret_input, 0, sizeof(kinput_t));

  // Check for special key
#define EON_KEY_DEF(pckey, pmeta, pch, pkey) \
        } else if (keylen == strlen((pckey)) && !strncmp((pckey), key, keylen)) { \
            ret_input->meta = (pmeta); \
            ret_input->ch = (pch); \
            ret_input->key = (pkey); \
            return EON_OK;

  if (keylen < 1) {
    return EON_ERR;
#include "keys.h"
  }

#undef EON_KEY_DEF

  // Check for character, with potential ALT modifier
  mod = 0;
  ch = 0;

  if (keylen > 2 && !strncmp("M-", key, 2)) {
    // mod = TB_MOD_ALT;
    meta = TB_META_ALT;
    key += 2;
  }

  utf8_char_to_unicode(&ch, key, NULL);

  if (ch < 1) {
    return EON_ERR;
  }

  ret_input->ch   = ch;
  ret_input->meta = meta;
  return EON_OK;
}

// Init signal handlers
static void _editor_init_signal_handlers(editor_t* editor) {
  struct sigaction action;
  memset(&action, 0, sizeof(struct sigaction));
  action.sa_handler = _editor_graceful_exit;
  sigaction(SIGTERM, &action, NULL);
  sigaction(SIGINT, &action, NULL);
  sigaction(SIGQUIT, &action, NULL);
  sigaction(SIGHUP, &action, NULL);
  signal(SIGPIPE, SIG_IGN);
}

// Gracefully exit
static void _editor_graceful_exit(int signum) {
  bview_t* bview;
  char path[64];
  int bview_num;
  bview_num = 0;

  if (tb_width() >= 0) tb_shutdown();

  CDL_FOREACH2(_editor.all_bviews, bview, all_next) {
    if (bview->buffer->is_unsaved) {
      snprintf((char*)&path, 64, ".eon.bak.%d.%d", getpid(), bview_num);
      buffer_save_as(bview->buffer, path, NULL);
      bview_num += 1;
    }
  }
  editor_deinit(&_editor);
  exit(1);
}

// Register built-in commands
static void _editor_register_cmds(editor_t* editor) {
  _editor_register_cmd_fn(editor, "cmd_apply_macro", cmd_apply_macro);
  _editor_register_cmd_fn(editor, "cmd_apply_macro_by", cmd_apply_macro_by);
  _editor_register_cmd_fn(editor, "cmd_browse", cmd_browse);
  _editor_register_cmd_fn(editor, "cmd_close", cmd_close);
  _editor_register_cmd_fn(editor, "cmd_copy", cmd_copy);
  _editor_register_cmd_fn(editor, "cmd_copy_by", cmd_copy_by);
  _editor_register_cmd_fn(editor, "cmd_ctag", cmd_ctag);
  _editor_register_cmd_fn(editor, "cmd_cut", cmd_cut);
  _editor_register_cmd_fn(editor, "cmd_cut_by", cmd_cut_by);
  _editor_register_cmd_fn(editor, "cmd_cut_or_close", cmd_cut_or_close);
  _editor_register_cmd_fn(editor, "cmd_delete_after", cmd_delete_after);
  _editor_register_cmd_fn(editor, "cmd_delete_before", cmd_delete_before);
  _editor_register_cmd_fn(editor, "cmd_delete_word_after", cmd_delete_word_after);
  _editor_register_cmd_fn(editor, "cmd_delete_word_before", cmd_delete_word_before);
  _editor_register_cmd_fn(editor, "cmd_drop_cursor_column", cmd_drop_cursor_column);
  _editor_register_cmd_fn(editor, "cmd_drop_sleeping_cursor", cmd_drop_sleeping_cursor);
  _editor_register_cmd_fn(editor, "cmd_find_word", cmd_find_word);
  _editor_register_cmd_fn(editor, "cmd_fsearch", cmd_fsearch);
  _editor_register_cmd_fn(editor, "cmd_grep", cmd_grep);
  _editor_register_cmd_fn(editor, "cmd_indent", cmd_indent);
  _editor_register_cmd_fn(editor, "cmd_insert_data", cmd_insert_data);
  _editor_register_cmd_fn(editor, "cmd_insert_newline_above", cmd_insert_newline_above);
  _editor_register_cmd_fn(editor, "cmd_isearch", cmd_isearch);
  _editor_register_cmd_fn(editor, "cmd_less", cmd_less);
  _editor_register_cmd_fn(editor, "cmd_move_beginning", cmd_move_beginning);
  _editor_register_cmd_fn(editor, "cmd_move_bol", cmd_move_bol);
  _editor_register_cmd_fn(editor, "cmd_move_bracket_back", cmd_move_bracket_back);
  _editor_register_cmd_fn(editor, "cmd_move_bracket_forward", cmd_move_bracket_forward);
  _editor_register_cmd_fn(editor, "cmd_move_down", cmd_move_down);
  _editor_register_cmd_fn(editor, "cmd_move_end", cmd_move_end);
  _editor_register_cmd_fn(editor, "cmd_move_eol", cmd_move_eol);
  _editor_register_cmd_fn(editor, "cmd_move_left", cmd_move_left);
  _editor_register_cmd_fn(editor, "cmd_move_page_down", cmd_move_page_down);
  _editor_register_cmd_fn(editor, "cmd_move_page_up", cmd_move_page_up);
  _editor_register_cmd_fn(editor, "cmd_move_relative", cmd_move_relative);
  _editor_register_cmd_fn(editor, "cmd_move_right", cmd_move_right);
  _editor_register_cmd_fn(editor, "cmd_move_to_line", cmd_move_to_line);
  _editor_register_cmd_fn(editor, "cmd_move_until_back", cmd_move_until_back);
  _editor_register_cmd_fn(editor, "cmd_move_until_forward", cmd_move_until_forward);
  _editor_register_cmd_fn(editor, "cmd_move_up", cmd_move_up);
  _editor_register_cmd_fn(editor, "cmd_move_word_back", cmd_move_word_back);
  _editor_register_cmd_fn(editor, "cmd_move_word_forward", cmd_move_word_forward);
  _editor_register_cmd_fn(editor, "cmd_next", cmd_next);
  _editor_register_cmd_fn(editor, "cmd_open_file", cmd_open_file);
  _editor_register_cmd_fn(editor, "cmd_open_new", cmd_open_new);
  _editor_register_cmd_fn(editor, "cmd_open_replace_file", cmd_open_replace_file);
  _editor_register_cmd_fn(editor, "cmd_open_replace_new", cmd_open_replace_new);
  _editor_register_cmd_fn(editor, "cmd_outdent", cmd_outdent);
  _editor_register_cmd_fn(editor, "cmd_pop_kmap", cmd_pop_kmap);
  _editor_register_cmd_fn(editor, "cmd_prev", cmd_prev);
  _editor_register_cmd_fn(editor, "cmd_push_kmap", cmd_push_kmap);
  _editor_register_cmd_fn(editor, "cmd_quit", cmd_quit);
  _editor_register_cmd_fn(editor, "cmd_redo", cmd_redo);
  _editor_register_cmd_fn(editor, "cmd_redraw", cmd_redraw);
  _editor_register_cmd_fn(editor, "cmd_remove_extra_cursors", cmd_remove_extra_cursors);
  _editor_register_cmd_fn(editor, "cmd_replace", cmd_replace);
  _editor_register_cmd_fn(editor, "cmd_save", cmd_save);
  _editor_register_cmd_fn(editor, "cmd_save_as", cmd_save_as);
  _editor_register_cmd_fn(editor, "cmd_search", cmd_search);
  _editor_register_cmd_fn(editor, "cmd_search_next", cmd_search_next);
  _editor_register_cmd_fn(editor, "cmd_set_opt", cmd_set_opt);
  _editor_register_cmd_fn(editor, "cmd_shell", cmd_shell);
  _editor_register_cmd_fn(editor, "cmd_show_help", cmd_show_help);
  _editor_register_cmd_fn(editor, "cmd_split_horizontal", cmd_split_horizontal);
  _editor_register_cmd_fn(editor, "cmd_split_vertical", cmd_split_vertical);
  _editor_register_cmd_fn(editor, "cmd_toggle_mouse_mode", cmd_toggle_mouse_mode);
  _editor_register_cmd_fn(editor, "cmd_toggle_anchor", cmd_toggle_anchor);
  _editor_register_cmd_fn(editor, "cmd_select_bol", cmd_select_bol);
  _editor_register_cmd_fn(editor, "cmd_select_eol", cmd_select_eol);
  _editor_register_cmd_fn(editor, "cmd_select_beginning", cmd_select_beginning);
  _editor_register_cmd_fn(editor, "cmd_select_end", cmd_select_end);
  _editor_register_cmd_fn(editor, "cmd_select_up", cmd_select_up);
  _editor_register_cmd_fn(editor, "cmd_select_down", cmd_select_down);
  _editor_register_cmd_fn(editor, "cmd_select_left", cmd_select_left);
  _editor_register_cmd_fn(editor, "cmd_select_right", cmd_select_right);
  _editor_register_cmd_fn(editor, "cmd_select_word_back", cmd_select_word_back);
  _editor_register_cmd_fn(editor, "cmd_select_word_forward", cmd_select_word_forward);
  _editor_register_cmd_fn(editor, "cmd_select_current_word", cmd_select_current_word);
  _editor_register_cmd_fn(editor, "cmd_select_current_line", cmd_select_current_line);
  _editor_register_cmd_fn(editor, "cmd_new_cursor_up", cmd_new_cursor_up);
  _editor_register_cmd_fn(editor, "cmd_new_cursor_down", cmd_new_cursor_down);
  _editor_register_cmd_fn(editor, "cmd_uncut", cmd_uncut);
  _editor_register_cmd_fn(editor, "cmd_undo", cmd_undo);
  _editor_register_cmd_fn(editor, "cmd_viewport_top", cmd_viewport_top);
  _editor_register_cmd_fn(editor, "cmd_viewport_mid", cmd_viewport_mid);
  _editor_register_cmd_fn(editor, "cmd_viewport_bot", cmd_viewport_bot);
  _editor_register_cmd_fn(editor, "cmd_wake_sleeping_cursors", cmd_wake_sleeping_cursors);
  _editor_register_cmd_fn(editor, "_editor_menu_cancel", _editor_menu_cancel);
  _editor_register_cmd_fn(editor, "_editor_menu_submit", _editor_menu_submit);
  _editor_register_cmd_fn(editor, "_editor_prompt_cancel", _editor_prompt_cancel);
  _editor_register_cmd_fn(editor, "_editor_prompt_history_down", _editor_prompt_history_down);
  _editor_register_cmd_fn(editor, "_editor_prompt_history_up", _editor_prompt_history_up);
  _editor_register_cmd_fn(editor, "_editor_prompt_input_complete", _editor_prompt_input_complete);
  _editor_register_cmd_fn(editor, "_editor_prompt_input_submit", _editor_prompt_input_submit);
  _editor_register_cmd_fn(editor, "_editor_prompt_isearch_drop_cursors", _editor_prompt_isearch_drop_cursors);
  _editor_register_cmd_fn(editor, "_editor_prompt_isearch_next", _editor_prompt_isearch_next);
  _editor_register_cmd_fn(editor, "_editor_prompt_isearch_prev", _editor_prompt_isearch_prev);
  _editor_register_cmd_fn(editor, "_editor_prompt_menu_cancel", _editor_prompt_menu_cancel);
  _editor_register_cmd_fn(editor, "_editor_prompt_menu_down", _editor_prompt_menu_down);
  _editor_register_cmd_fn(editor, "_editor_prompt_menu_page_down", _editor_prompt_menu_page_down);
  _editor_register_cmd_fn(editor, "_editor_prompt_menu_page_up", _editor_prompt_menu_page_up);
  _editor_register_cmd_fn(editor, "_editor_prompt_menu_up", _editor_prompt_menu_up);
  _editor_register_cmd_fn(editor, "_editor_prompt_toggle_replace", _editor_prompt_toggle_replace);
  _editor_register_cmd_fn(editor, "_editor_prompt_yna_all", _editor_prompt_yna_all);
  _editor_register_cmd_fn(editor, "_editor_prompt_yn_no", _editor_prompt_yn_no);
  _editor_register_cmd_fn(editor, "_editor_prompt_yn_yes", _editor_prompt_yn_yes);
}

// Init built-in kmaps
static void _editor_init_kmaps(editor_t* editor) {
  _editor_init_kmap(editor, &editor->kmap_normal, "eon_normal", "cmd_insert_data", 0, (kbinding_def_t[]) {
    EON_KBINDING_DEF("cmd_show_help", "F2"),
    EON_KBINDING_DEF("cmd_delete_before", "backspace"),
    EON_KBINDING_DEF("cmd_delete_before", "backspace2"),
    EON_KBINDING_DEF("cmd_delete_after", "delete"),
    EON_KBINDING_DEF("cmd_insert_newline_above", "C-\\"),
    EON_KBINDING_DEF("cmd_move_bol", "C-a"),
    EON_KBINDING_DEF("cmd_move_bol", "home"),
    EON_KBINDING_DEF("cmd_select_bol", "S-home"),
    EON_KBINDING_DEF("cmd_select_beginning", "CS-home"),
    EON_KBINDING_DEF("cmd_move_eol", "C-e"),
    EON_KBINDING_DEF("cmd_move_eol", "end"),
    EON_KBINDING_DEF("cmd_select_eol", "S-end"),
    EON_KBINDING_DEF("cmd_select_end", "CS-end"),
    EON_KBINDING_DEF("cmd_move_beginning", "M-\\"),
    EON_KBINDING_DEF("cmd_move_beginning", "C-home"),
    EON_KBINDING_DEF("cmd_move_end", "M-/"),
    EON_KBINDING_DEF("cmd_move_end", "C-end"),
    EON_KBINDING_DEF("cmd_move_left", "left"),
    EON_KBINDING_DEF("cmd_move_right", "right"),
    EON_KBINDING_DEF("cmd_move_up", "up"),
    EON_KBINDING_DEF("cmd_move_down", "down"),
    EON_KBINDING_DEF("cmd_move_page_up", "page-up"),
    EON_KBINDING_DEF("cmd_move_page_down", "page-down"),
    EON_KBINDING_DEF("cmd_move_to_line", "M-g"),
    EON_KBINDING_DEF_EX("cmd_move_relative", "M-y ## u", "up"),
    EON_KBINDING_DEF_EX("cmd_move_relative", "M-y ## d", "down"),
    EON_KBINDING_DEF("cmd_move_until_forward", "M-' **"),
    EON_KBINDING_DEF("cmd_move_until_back", "M-; **"),
    EON_KBINDING_DEF("cmd_move_word_forward", "M-f"),
    EON_KBINDING_DEF("cmd_move_word_back", "M-b"),
    EON_KBINDING_DEF("cmd_move_word_forward", "C-right"),
    EON_KBINDING_DEF("cmd_move_word_back", "C-left"),
    EON_KBINDING_DEF("cmd_move_bracket_forward", "M-]"),
    EON_KBINDING_DEF("cmd_move_bracket_back", "M-["),
    EON_KBINDING_DEF("cmd_move_bracket_back", "C-up"),
    EON_KBINDING_DEF("cmd_move_bracket_forward", "C-down"),

    // EON_KBINDING_DEF("cmd_search", "C-f"),
    EON_KBINDING_DEF("cmd_search", "C-w"),
    EON_KBINDING_DEF("cmd_search_next", "C-g"),
    EON_KBINDING_DEF("cmd_search_next", "F3"),
    EON_KBINDING_DEF("cmd_find_word", "C-v"),
    EON_KBINDING_DEF("cmd_isearch", "C-f"),
    EON_KBINDING_DEF("cmd_replace", "C-r"),
    EON_KBINDING_DEF("cmd_cut", "C-k"),
    // EON_KBINDING_DEF("cmd_cut", "M-c"),
    EON_KBINDING_DEF("cmd_cut_or_close", "C-x"),
    EON_KBINDING_DEF("cmd_copy", "M-k"),
    EON_KBINDING_DEF("cmd_copy", "C-c"),
    EON_KBINDING_DEF("cmd_uncut", "C-u"),
    EON_KBINDING_DEF("cmd_uncut", "C-v"),
    EON_KBINDING_DEF("cmd_redraw", "M-x l"),
    EON_KBINDING_DEF("cmd_less", "M-l"),
    EON_KBINDING_DEF("cmd_viewport_top", "M--"),
    EON_KBINDING_DEF("cmd_viewport_mid", "C-l"),
    EON_KBINDING_DEF("cmd_viewport_bot", "M-="),
    EON_KBINDING_DEF("cmd_push_kmap", "M-x p"),
    EON_KBINDING_DEF("cmd_pop_kmap", "M-x P"),
    // EON_KBINDING_DEF_EX("cmd_copy_by", "C-c d", "bracket"),
    // EON_KBINDING_DEF_EX("cmd_copy_by", "C-c w", "word"),
    // EON_KBINDING_DEF_EX("cmd_copy_by", "C-c s", "word_back"),
    // EON_KBINDING_DEF_EX("cmd_copy_by", "C-c f", "word_forward"),
    // EON_KBINDING_DEF_EX("cmd_copy_by", "C-c a", "bol"),
    // EON_KBINDING_DEF_EX("cmd_copy_by", "C-c e", "eol"),
    // EON_KBINDING_DEF_EX("cmd_copy_by", "C-c c", "string"),
    // EON_KBINDING_DEF_EX("cmd_cut_by", "C-d d", "bracket"),
    // EON_KBINDING_DEF_EX("cmd_cut_by", "C-d w", "word"),
    // EON_KBINDING_DEF_EX("cmd_cut_by", "C-d s", "word_back"),
    // EON_KBINDING_DEF_EX("cmd_cut_by", "C-d f", "word_forward"),
    // EON_KBINDING_DEF_EX("cmd_cut_by", "C-d a", "bol"),
    // EON_KBINDING_DEF_EX("cmd_cut_by", "C-d e", "eol"),
    // EON_KBINDING_DEF_EX("cmd_cut_by", "C-d c", "string"),
    EON_KBINDING_DEF("cmd_delete_word_before", "C-h"),
    EON_KBINDING_DEF("cmd_delete_word_after", "M-d"),
    // EON_KBINDING_DEF("cmd_toggle_anchor", "M-a"),
    EON_KBINDING_DEF("cmd_select_up", "S-up"),
    EON_KBINDING_DEF("cmd_select_down", "S-down"),
    EON_KBINDING_DEF("cmd_select_left", "S-left"),
    EON_KBINDING_DEF("cmd_select_right", "S-right"),

    // for linux terminal, that doesn't have shift+arrows
    EON_KBINDING_DEF("cmd_select_up", "M-w"),
    EON_KBINDING_DEF("cmd_select_down", "M-s"),
    EON_KBINDING_DEF("cmd_select_left", "M-a"),
    EON_KBINDING_DEF("cmd_select_right", "M-d"),

    EON_KBINDING_DEF("cmd_select_word_back", "CS-left"),
    EON_KBINDING_DEF("cmd_select_word_forward", "CS-right"),

    EON_KBINDING_DEF("cmd_select_word_back", "MS-a"), // linux
    EON_KBINDING_DEF("cmd_select_word_forward", "MS-d"), // linux

    // EON_KBINDING_DEF("cmd_new_cursor_up", "CS-up"),
    EON_KBINDING_DEF("cmd_new_cursor_up", "MS-up"),
    EON_KBINDING_DEF("cmd_new_cursor_up", "MS-w"), // alt+shift+w, for linux
    // EON_KBINDING_DEF("cmd_new_cursor_down", "CS-down"),
    EON_KBINDING_DEF("cmd_new_cursor_down", "MS-down"),
    EON_KBINDING_DEF("cmd_new_cursor_down", "MS-s"), // alt+shift+s

    EON_KBINDING_DEF("cmd_drop_sleeping_cursor", "C-/ ."),
    EON_KBINDING_DEF("cmd_wake_sleeping_cursors", "C-/ a"),
    EON_KBINDING_DEF("cmd_remove_extra_cursors", "C-/ /"),
    EON_KBINDING_DEF("cmd_remove_extra_cursors", "C-2"), // Ctrl+2 or Ctrl+Space
    // EON_KBINDING_DEF("cmd_remove_extra_cursors", "C-up"),
    // EON_KBINDING_DEF("cmd_remove_extra_cursors", "C-down"),
    EON_KBINDING_DEF("cmd_drop_cursor_column", "C-/ '"),
    EON_KBINDING_DEF("cmd_apply_macro", "M-j"),
    EON_KBINDING_DEF("cmd_apply_macro_by", "M-m **"),
    EON_KBINDING_DEF("cmd_prev", "M-,"),
    EON_KBINDING_DEF("cmd_next", "M-."),
    EON_KBINDING_DEF("cmd_prev", "C-page-down"),
    EON_KBINDING_DEF("cmd_next", "C-page-up"),
    EON_KBINDING_DEF("cmd_prev", "CS-page-down"),
    EON_KBINDING_DEF("cmd_next", "CS-page-up"),
    EON_KBINDING_DEF("cmd_split_vertical", "M-v"),
    EON_KBINDING_DEF("cmd_split_horizontal", "M-h"),
    EON_KBINDING_DEF("cmd_grep", "M-q"),
    EON_KBINDING_DEF("cmd_grep", "CS-f"),
    EON_KBINDING_DEF("cmd_fsearch", "C-p"),
    EON_KBINDING_DEF("cmd_browse", "C-b"),
    EON_KBINDING_DEF("cmd_browse", "C-t"),
    EON_KBINDING_DEF("cmd_undo", "C-z"),
    EON_KBINDING_DEF("cmd_redo", "C-y"),
    EON_KBINDING_DEF("cmd_redo", "CS-z"),
    EON_KBINDING_DEF("cmd_save", "C-s"),
    // EON_KBINDING_DEF("cmd_save_as", "M-s"),
    EON_KBINDING_DEF("cmd_save_as", "C-o"),
    EON_KBINDING_DEF("cmd_save_as", "CS-s"),
    EON_KBINDING_DEF_EX("cmd_set_opt", "M-o a", "tab_to_space"),
    EON_KBINDING_DEF_EX("cmd_set_opt", "M-o t", "tab_width"),
    EON_KBINDING_DEF_EX("cmd_set_opt", "M-o s", "syntax"),
    EON_KBINDING_DEF_EX("cmd_set_opt", "M-o w", "soft_wrap"),
    EON_KBINDING_DEF("cmd_open_new", "C-n"),
    // EON_KBINDING_DEF("cmd_open_file", "C-o"),
    EON_KBINDING_DEF("cmd_open_replace_new", "C-q n"),
    EON_KBINDING_DEF("cmd_open_replace_file", "C-q o"),
    EON_KBINDING_DEF_EX("cmd_fsearch", "C-q p", "replace"),
    EON_KBINDING_DEF("cmd_indent", "tab"),
    // EON_KBINDING_DEF("cmd_outdent", "M-,"),
    EON_KBINDING_DEF("cmd_outdent", "S-tab"),
    EON_KBINDING_DEF("cmd_ctag", "F6"),
    EON_KBINDING_DEF("cmd_shell", "M-e"),
    // EON_KBINDING_DEF("cmd_close", "M-c"),
    EON_KBINDING_DEF("cmd_toggle_mouse_mode", "M-backspace"),
    EON_KBINDING_DEF("cmd_toggle_mouse_mode", "S-delete"),

    EON_KBINDING_DEF("cmd_close", "C-q"),
    EON_KBINDING_DEF("cmd_close", "C-d"),
    EON_KBINDING_DEF("cmd_close", "escape"),
    EON_KBINDING_DEF("cmd_quit", "CS-q"),
    EON_KBINDING_DEF(NULL, NULL)
  });

  // prompt input, used when requesting input from user (search string, save as)
  // no default command, but falls-through to normal keymap if no matches.
  _editor_init_kmap(editor, &editor->kmap_prompt_input, "eon_prompt_input", NULL, 1, (kbinding_def_t[]) {
    EON_KBINDING_DEF("_editor_prompt_input_submit", "enter"),
    EON_KBINDING_DEF("_editor_prompt_input_complete", "tab"),
    EON_KBINDING_DEF("_editor_prompt_cancel", "escape"),
    EON_KBINDING_DEF("_editor_prompt_cancel", "C-c"),
    EON_KBINDING_DEF("_editor_prompt_cancel", "C-x"),
    EON_KBINDING_DEF("_editor_prompt_cancel", "M-c"),
    EON_KBINDING_DEF("_editor_prompt_history_up", "up"),
    EON_KBINDING_DEF("_editor_prompt_history_down", "down"),
    EON_KBINDING_DEF(NULL, NULL)
  });

  // yes/no keymap. used in prompts like "discard charges on file?" no fallthrough, meaning
  // input is silenced.
  _editor_init_kmap(editor, &editor->kmap_prompt_yn, "eon_prompt_yn", NULL, 0, (kbinding_def_t[]) {
    EON_KBINDING_DEF("_editor_prompt_yn_yes", "y"),
    EON_KBINDING_DEF("_editor_prompt_yn_no", "n"),
    EON_KBINDING_DEF("_editor_prompt_cancel", "escape"),
    EON_KBINDING_DEF("_editor_prompt_cancel", "C-c"),
    EON_KBINDING_DEF("_editor_prompt_cancel", "C-x"),
    EON_KBINDING_DEF("_editor_prompt_cancel", "M-c"),
    EON_KBINDING_DEF(NULL, NULL)
  });

  // yes/no/all keymap. used in the find and replace prompt, after matches have been found.
  _editor_init_kmap(editor, &editor->kmap_prompt_yna, "eon_prompt_yna", NULL, 0, (kbinding_def_t[]) {
    EON_KBINDING_DEF("_editor_prompt_yn_yes", "enter"),
    EON_KBINDING_DEF("_editor_prompt_yn_yes", "y"),
    EON_KBINDING_DEF("_editor_prompt_yn_no", "n"),
    EON_KBINDING_DEF("_editor_prompt_yn_no", "down"),
    EON_KBINDING_DEF("_editor_prompt_yna_all", "a"),
    EON_KBINDING_DEF("_editor_prompt_cancel", "escape"),
    EON_KBINDING_DEF("_editor_prompt_cancel", "C-c"),
    EON_KBINDING_DEF("_editor_prompt_cancel", "C-x"),
    EON_KBINDING_DEF("_editor_prompt_cancel", "M-c"),
    EON_KBINDING_DEF(NULL, NULL)
  });


//  _editor_init_kmap(editor, &editor->kmap_prompt_ok, "eon_prompt_ok", "_editor_prompt_cancel", 0, (kbinding_def_t[]) {
//    EON_KBINDING_DEF(NULL, NULL)
//  });

  // menu keymap. used when showing menus in the editor, like the directory tree
  // or the search results page (grep). fallsthrough to normal keymap, meaning regular input is allowed.
  _editor_init_kmap(editor, &editor->kmap_menu, "eon_menu", NULL, 1, (kbinding_def_t[]) {
    EON_KBINDING_DEF("_editor_menu_submit", "enter"),
    EON_KBINDING_DEF("_editor_menu_cancel", "C-c"),
    EON_KBINDING_DEF(NULL, NULL)
    });



  // prompt menu input. used in prompts that allow going up and down. not used.
  _editor_init_kmap(editor, &editor->kmap_prompt_menu, "eon_prompt_menu", NULL, 0, (kbinding_def_t[]) {
    EON_KBINDING_DEF("_editor_prompt_input_submit", "enter"),
    EON_KBINDING_DEF("_editor_prompt_menu_up", "up"),
    EON_KBINDING_DEF("_editor_prompt_menu_down", "down"),
    EON_KBINDING_DEF("_editor_prompt_menu_up", "left"),
    EON_KBINDING_DEF("_editor_prompt_menu_down", "right"),
    EON_KBINDING_DEF("_editor_prompt_menu_page_up", "page-up"),
    EON_KBINDING_DEF("_editor_prompt_menu_page_down", "page-down"),
    EON_KBINDING_DEF("_editor_prompt_menu_cancel", "escape"),
    EON_KBINDING_DEF("_editor_prompt_menu_cancel", "C-c"),
    EON_KBINDING_DEF("_editor_prompt_menu_cancel", "C-x"),
    EON_KBINDING_DEF("_editor_prompt_menu_cancel", "M-c"),
    EON_KBINDING_DEF(NULL, NULL)
  });

  // incremental search keymap. allows jumping to prev/next result, dropping cursors on them, etc
  _editor_init_kmap(editor, &editor->kmap_prompt_isearch, "eon_prompt_isearch", NULL, 1, (kbinding_def_t[]) {
    EON_KBINDING_DEF("_editor_prompt_toggle_replace", "C-f"),
    EON_KBINDING_DEF("_editor_prompt_isearch_prev", "up"),
    EON_KBINDING_DEF("_editor_prompt_isearch_next", "down"),
    EON_KBINDING_DEF("_editor_prompt_isearch_drop_cursors", "C-/"),
    EON_KBINDING_DEF("_editor_prompt_isearch_drop_cursors", "C-2"),
    EON_KBINDING_DEF("_editor_prompt_isearch_drop_cursors", "M-enter"), // alt enter
    EON_KBINDING_DEF("_editor_prompt_cancel", "enter"),
    EON_KBINDING_DEF("_editor_prompt_cancel", "escape"),
    EON_KBINDING_DEF("_editor_prompt_cancel", "C-c"),
    EON_KBINDING_DEF("_editor_prompt_cancel", "C-x"),
    EON_KBINDING_DEF("_editor_prompt_cancel", "M-c"),
    EON_KBINDING_DEF(NULL, NULL)
  });
}

// Init a single kmap
static void _editor_init_kmap(editor_t* editor, kmap_t** ret_kmap, char* name, char* default_cmd_name, int allow_fallthru, kbinding_def_t* defs) {
  kmap_t* kmap;

  kmap = calloc(1, sizeof(kmap_t));
  kmap->name = strdup(name);
  kmap->allow_fallthru = allow_fallthru;
  kmap->bindings = calloc(1, sizeof(kbinding_t));

  if (default_cmd_name) {
    kmap->default_cmd_name = strdup(default_cmd_name);
  }

  while (defs && defs->cmd_name) {
    _editor_init_kmap_add_binding(editor, kmap, defs);
    defs++;
  }

  HASH_ADD_KEYPTR(hh, editor->kmap_map, kmap->name, strlen(kmap->name), kmap);
  *ret_kmap = kmap;
}

// Add a binding to a kmap
static void _editor_init_kmap_add_binding(editor_t* editor, kmap_t* kmap, kbinding_def_t* binding_def) {
  char* cur_key_patt;
  cur_key_patt = strdup(binding_def->key_patt);
  _editor_init_kmap_add_binding_to_trie(&kmap->bindings->children, binding_def->cmd_name, cur_key_patt, binding_def->key_patt, binding_def->static_param);

  if (strcmp(binding_def->cmd_name, "cmd_show_help") == 0) {
    // TODO kind of hacky
    EON_SET_INFO(editor, "show_help: Press %s", cur_key_patt);
  }

  free(cur_key_patt);
}

int editor_add_binding_to_keymap(editor_t* editor, kmap_t* kmap, kbinding_def_t* binding_def) {
  _editor_init_kmap_add_binding(editor, kmap, binding_def);
  return EON_OK;
}


// Add a binding to a kmap trie
static int _editor_init_kmap_add_binding_to_trie(kbinding_t** trie, char* cmd_name, char* cur_key_patt, char* full_key_patt, char* static_param) {
  char* next_key_patt;
  kbinding_t* node;
  kinput_t input;

  // Find next_key_patt and add null-char to cur_key_patt
  next_key_patt = strchr(cur_key_patt, ' ');

  if (next_key_patt != NULL) {
    *next_key_patt = '\0';
    next_key_patt += 1;
  }

  // cur_key_patt points to a null-term cstring now
  // next_key_patt is either NULL or points to a null-term cstring

  // Parse cur_key_patt token as input
  memset(&input, 0, sizeof(kinput_t));

  if (strcmp("##", cur_key_patt) == 0) {
    input = EON_KINPUT_NUMERIC;

  } else if (strcmp("**", cur_key_patt) == 0) {
    input = EON_KINPUT_WILDCARD;

  } else if (_editor_key_to_input(cur_key_patt, &input) == EON_OK) {
    // Hi mom!
  } else {
    return EON_ERR;
  }

  // Add node for input if it doesn't already exist
  node = NULL;
  HASH_FIND(hh, *trie, &input, sizeof(kinput_t), node);

  if (!node) {
    node = calloc(1, sizeof(kbinding_t));
    node->input = input;
    HASH_ADD(hh, *trie, input, sizeof(kinput_t), node);
  }

  if (next_key_patt) {
    // Recurse for next key
    if (_editor_init_kmap_add_binding_to_trie(&node->children, cmd_name, next_key_patt, full_key_patt, static_param) != EON_OK) {
      free(node);
      return EON_ERR;
    }

  } else {
    // Leaf node, set cmd
    node->static_param = static_param ? strdup(static_param) : NULL;
    node->key_patt = strdup(full_key_patt);
    node->cmd_name = strdup(cmd_name);
    node->is_leaf = 1;
  }

  return EON_OK;
}

// Proxy for _editor_init_kmap with str in format '<name>,<default_cmd>,<allow_fallthru>'
static int _editor_init_kmap_by_str(editor_t* editor, kmap_t** ret_kmap, char* str) {
  char* args[3];
  args[0] = strtok(str,  ","); if (!args[0]) return EON_ERR;
  args[1] = strtok(NULL, ","); if (!args[1]) return EON_ERR;
  args[2] = strtok(NULL, ",");
  _editor_init_kmap(editor, ret_kmap, args[0], args[2] ? args[1] : NULL, atoi(args[2] ? args[2] : args[1]), NULL);
  return EON_OK;
}

// Proxy for _editor_init_kmap_add_binding with str in format '<cmd>,<key>,<param>'
static int _editor_init_kmap_add_binding_by_str(editor_t* editor, kmap_t* kmap, char* str) {
  char* args[3];
  args[0] = strtok(str,  ","); if (!args[0]) return EON_ERR;
  args[1] = strtok(NULL, ","); if (!args[1]) return EON_ERR;
  args[2] = strtok(NULL, ",");
  _editor_init_kmap_add_binding(editor, kmap, &((kbinding_def_t) {args[0], args[1], args[2]}));
  return EON_OK;
}

// Destroy a kmap
static void _editor_destroy_kmap(kmap_t* kmap, kbinding_t* trie) {
  kbinding_t* binding;
  kbinding_t* binding_tmp;
  int is_top;
  is_top = (trie == kmap->bindings ? 1 : 0);

  HASH_ITER(hh, trie, binding, binding_tmp) {

    if (binding->children) {
      _editor_destroy_kmap(kmap, binding->children);
    }

    HASH_DELETE(hh, trie, binding);
    if (binding->static_param) free(binding->static_param);
    if (binding->cmd_name) free(binding->cmd_name);
    if (binding->key_patt) free(binding->key_patt);
    free(binding);
  }

  if (is_top) {
    if (kmap->name) free(kmap->name);
    free(kmap);
  }
}

// Add a macro by str with format '<name> <key1> <key2> ... <keyN>'
static int _editor_add_macro_by_str(editor_t* editor, char* str) {
  int has_input;
  char* token;
  kmacro_t* macro;
  kinput_t input = { 0, 0, 0 };

  has_input = 0;
  macro = NULL;

  // Tokenize by space
  token = strtok(str, " ");

  while (token) {
    if (!macro) {
      // Make macro with <name> on first token
      macro = calloc(1, sizeof(kmacro_t));
      macro->name = strdup(token);

    } else {
      // Parse token as kinput_t
      if (_editor_key_to_input(token, &input) != EON_OK) {
        free(macro->name);
        free(macro);
        return EON_ERR;
      }

      // Add kinput_t to macro
      _editor_record_macro_input(macro, &input);
      has_input = 1;
    }

    // Get next token
    token = strtok(NULL, " ");
  }

  // Add macro to map if has_input
  if (has_input) {
    HASH_ADD_KEYPTR(hh, editor->macro_map, macro->name, strlen(macro->name), macro);
    return EON_OK;
  }

  // Fail
  if (macro) {
    free(macro->name);
    free(macro);
  }

  return EON_ERR;
}

// Init built-in syntax map
static void _editor_init_syntaxes(editor_t* editor) {
  _editor_init_syntax(editor, NULL, "generic", "\\.(c|cc|cpp|h|hpp|php|py|rb|erb|sh|pl|go|js|java|jsp|lua)$", -1, -1, (srule_def_t[]) {
    { "(?<![\\w%@$])("
      "abstract|alias|alignas|alignof|and|and_eq|arguments|array|as|asm|"
      "assert|auto|base|begin|bitand|bitor|bool|boolean|break|byte|"
      "callable|case|catch|chan|char|checked|class|clone|cmp|compl|const|"
      "const_cast|constexpr|continue|debugger|decimal|declare|decltype|"
      "def|default|defer|defined|del|delegate|delete|die|do|done|double|"
      "dynamic_cast|echo|elif|else|elseif|elsif|empty|end|enddeclare|"
      "endfor|endforeach|endif|endswitch|endwhile|ensure|enum|eq|esac|"
      "eval|event|except|exec|exit|exp|explicit|export|extends|extern|"
      "fallthrough|false|fi|final|finally|fixed|float|for|foreach|friend|"
      "from|func|function|ge|global|go|goto|gt|if|implements|implicit|"
      "import|in|include|include_once|inline|instanceof|insteadof|int|"
      "interface|internal|is|isset|lambda|le|let|list|local|lock|long|lt|m|map|"
      "module|mutable|namespace|native|ne|new|next|nil|no|noexcept|not|"
      "not_eq|null|nullptr|object|operator|or|or_eq|out|override|package|"
      "params|pass|print|private|protected|public|q|qq|qr|qw|qx|raise|"
      "range|readonly|redo|ref|register|reinterpret_cast|require|"
      "require_once|rescue|retry|return|s|sbyte|sealed|select|self|short|"
      "signed|sizeof|stackalloc|static|static_assert|static_cast|"
      "strictfp|string|struct|sub|super|switch|synchronized|template|"
      "then|this|thread_local|throw|throws|time|tr|trait|transient|true|"
      "try|type|typedef|typeid|typename|typeof|uint|ulong|unchecked|"
      "undef|union|unless|unsafe|unset|unsigned|until|use|ushort|using|"
      "var|virtual|void|volatile|when|while|with|xor|xor_eq|y|yield"
      ")\\b", NULL, KEYWORD_FG, KEYWORD_BG
    },
    { "[(){}<>\\[\\].,;:?!+=/\\\\%^*-]", NULL, PUNCTUATION_FG, PUNCTUATION_BG },
    { "(?<!\\w)[\\%@$][a-zA-Z_$][a-zA-Z0-9_]*\\b", NULL, VARIABLES_FG, VARIABLES_BG },
    { "\\b[A-Z_][A-Z0-9_]*\\b", NULL, CONSTANTS_FG, CONSTANTS_BG },
    { "\\b(-?(0x)?[0-9]+|true|false|null)\\b", NULL, BOOLS_INTS_FG, BOOLS_INTS_BG },
    { "/([^/]|\\\\/)+/(?!/)", NULL, REGEX_FG, REGEX_BG },
    { "'([^']|\\')*?'", NULL, SINGLE_QUOTE_STRING_FG, SINGLE_QUOTE_STRING_BG },
    { "\"(\\\"|[^\"])*?\"", NULL, DOUBLE_QUOTE_STRING_FG, DOUBLE_QUOTE_STRING_BG },
    { "/" "/.*$", NULL, COMMENT_FG, COMMENT_BG },
    { "^\\s*#( .*|)$", NULL, COMMENT_FG, COMMENT_BG },
    { "^#!/.*$", NULL, COMMENT_FG, COMMENT_BG },
    { "\\s--.*$", NULL, COMMENT_FG, COMMENT_BG }, // lua comment
    { "/\\" "*", "\\*" "/", COMMENT_FG, COMMENT_BG },
    { "\"\"\"", "\"\"\"", TRIPLE_QUOTE_COMMENT_FG, TRIPLE_QUOTE_COMMENT_BG },
    { "\\t+", NULL, TAB_WHITESPACE_FG, TAB_WHITESPACE_BG },
    { "\\s+$", NULL, WHITESPACE_FG, WHITESPACE_BG },
    { NULL, NULL, 0, 0 }
  });
}

// Init a single syntax
static void _editor_init_syntax(editor_t* editor, syntax_t** optret_syntax, char* name, char* path_pattern, int tab_width, int tab_to_space, srule_def_t* defs) {
  syntax_t* syntax;

  syntax = calloc(1, sizeof(syntax_t));
  syntax->name = strdup(name);
  syntax->path_pattern = strdup(path_pattern);
  syntax->tab_width = tab_width >= 1 ? tab_width : -1; // -1 means default
  syntax->tab_to_space = tab_to_space >= 0 ? (tab_to_space ? 1 : 0) : -1;

  while (defs && defs->re) {
    _editor_init_syntax_add_rule(syntax, defs);
    defs++;
  }

  HASH_ADD_KEYPTR(hh, editor->syntax_map, syntax->name, strlen(syntax->name), syntax);
  if (optret_syntax) *optret_syntax = syntax;
}

// Proxy for _editor_init_syntax with str in format '<name>,<path_pattern>,<tab_width>,<tab_to_space>'
static int _editor_init_syntax_by_str(editor_t* editor, syntax_t** ret_syntax, char* str) {
  char* args[4];
  args[0] = strtok(str,  ","); if (!args[0]) return EON_ERR;
  args[1] = strtok(NULL, ","); if (!args[1]) return EON_ERR;
  args[2] = strtok(NULL, ","); if (!args[2]) return EON_ERR;
  args[3] = strtok(NULL, ","); if (!args[3]) return EON_ERR;
  _editor_init_syntax(editor, ret_syntax, args[0], args[1], atoi(args[2]), atoi(args[3]), NULL);
  return EON_OK;
}

// Add rule to syntax
static void _editor_init_syntax_add_rule(syntax_t* syntax, srule_def_t* def) {
  srule_node_t* node;
  node = calloc(1, sizeof(srule_node_t));

  if (def->re_end) {
    node->srule = srule_new_multi(def->re, strlen(def->re), def->re_end, strlen(def->re_end), (uint16_t)def->fg, (uint16_t)def->bg);
  } else {
    node->srule = srule_new_single(def->re, strlen(def->re), 0, (uint16_t)def->fg, (uint16_t)def->bg);
  }

  if (node->srule) DL_APPEND(syntax->srules, node);
}

// Proxy for _editor_init_syntax_add_rule with str in format '<start>,<end>,<fg>,<bg>' or '<regex>,<fg>,<bg>'
static int _editor_init_syntax_add_rule_by_str(syntax_t* syntax, char* str) {
  char* args[4];
  int style_i;
  args[0] = strtok(str,  ","); if (!args[0]) return EON_ERR;
  args[1] = strtok(NULL, ","); if (!args[1]) return EON_ERR;
  args[2] = strtok(NULL, ","); if (!args[2]) return EON_ERR;
  args[3] = strtok(NULL, ",");
  style_i = args[3] ? 2 : 1;
  _editor_init_syntax_add_rule(syntax, &((srule_def_t) { args[0], style_i == 2 ? args[1] : NULL, atoi(args[style_i]), atoi(args[style_i + 1]) }));
  return EON_OK;
}

// Destroy a syntax
static void _editor_destroy_syntax_map(syntax_t* map) {
  syntax_t* syntax;
  syntax_t* syntax_tmp;
  srule_node_t* srule;
  srule_node_t* srule_tmp;
  HASH_ITER(hh, map, syntax, syntax_tmp) {
    HASH_DELETE(hh, map, syntax);
    DL_FOREACH_SAFE(syntax->srules, srule, srule_tmp) {
      DL_DELETE(syntax->srules, srule);
      srule_destroy(srule->srule);
      free(srule);
    }
    free(syntax->name);
    free(syntax->path_pattern);
    free(syntax);
  }
}

// Read rc file
static int _editor_init_from_rc_read(editor_t* editor, FILE* rc, char** ret_rc_data, size_t* ret_rc_data_len) {
  fseek(rc, 0L, SEEK_END);
  *ret_rc_data_len = (size_t)ftell(rc);
  fseek(rc, 0L, SEEK_SET);
  *ret_rc_data = malloc(*ret_rc_data_len + 1);
  int res = fread(*ret_rc_data, *ret_rc_data_len, 1, rc);
  (*ret_rc_data)[*ret_rc_data_len] = '\0';
  return EON_OK;
}

// Exec rc file, read stdout
static int _editor_init_from_rc_exec(editor_t* editor, char* rc_path, char** ret_rc_data, size_t* ret_rc_data_len) {
  char buf[512];
  size_t nbytes;
  char* data;
  size_t data_len;
  size_t data_cap;
  FILE* fp;

  // Popen rc file
  if ((fp = popen(rc_path, "r")) == NULL) {
    EON_RETURN_ERR(editor, "Failed to popen rc file %s", rc_path);
  }

  // Read output
  data = NULL;
  data_len = 0;
  data_cap = 0;

  while ((nbytes = fread(buf, 1, sizeof(buf), fp)) != 0) {
    if (data_len + nbytes >= data_cap) {
      data_cap += sizeof(buf);
      data = realloc(data, data_cap);
    }

    memmove(data + data_len, buf, nbytes);
    data_len += nbytes;
  }

  // Add null terminator
  if (data_len + 1 >= data_cap) {
    data_cap += 1;
    data = realloc(data, data_cap);
  }

  data[data_len] = '\0';

  // Return
  pclose(fp);
  *ret_rc_data = data;
  *ret_rc_data_len = data_len;
  return EON_OK;
}

// Parse rc file
static int _editor_init_from_rc(editor_t* editor, FILE* rc, char* rc_path) {
  int rv;
  size_t rc_data_len;
  char *rc_data;
  char *rc_data_stop;
  char* eol;
  char* bol;
  int fargc;
  char** fargv;
  struct stat statbuf;
  rv = EON_OK;
  rc_data = NULL;
  rc_data_len = 0;

  // Read or exec rc file
  if (fstat(fileno(rc), &statbuf) == 0 && statbuf.st_mode & S_IXUSR) {
    _editor_init_from_rc_exec(editor, rc_path, &rc_data, &rc_data_len);

  } else {
    _editor_init_from_rc_read(editor, rc, &rc_data, &rc_data_len);
  }

  rc_data_stop = rc_data + rc_data_len;

  // Make fargc, fargv
  int i;
  fargv = NULL;

  for (i = 0; i < 2; i++) {
    bol = rc_data;
    fargc = 1;

    while (bol < rc_data_stop) {
      eol = strchr(bol, '\n');
      if (!eol) eol = rc_data_stop - 1;

      if (*bol != ';') { // Treat semicolon lines as comments
        if (fargv) {
          *eol = '\0';
          fargv[fargc] = bol;
        }
        fargc += 1;
      }
      bol = eol + 1;
    }

    if (!fargv) {
      if (fargc < 2) break; // No options

      fargv = malloc((fargc + 1) * sizeof(char*));
      fargv[0] = "eon";
      fargv[fargc] = NULL;
    }
  }

  // Parse args
  if (fargv) {
    rv = _editor_init_from_args(editor, fargc, fargv);
    free(fargv);
  }

  free(rc_data);

  return rv;
}

// Parse cli args
static int _editor_init_from_args(editor_t* editor, int argc, char** argv) {
  int c, rv = EON_OK;
  kmap_t* cur_kmap;
  syntax_t* cur_syntax;

  cur_kmap   = NULL;
  cur_syntax = NULL;
  optind = 0;

  while (rv == EON_OK && (c = getopt(argc, argv, "ha:b:c:gn:H:i:K:k:l:M:m:Nn:p:S:s:t:vw:y:z:")) != -1) {
    switch (c) {
    case 'h':
      printf("eon version %s\n\n", EON_VERSION);
      printf("Usage: eon [options] [file:line]...\n\n");
      printf("    -h           Show this message\n");
      printf("    -a <1|0>     Enable/disable tab_to_space (default: %d)\n", EON_DEFAULT_TAB_TO_SPACE);
      printf("    -b <1|0>     Enable/disbale highlight bracket pairs (default: %d)\n", EON_DEFAULT_HILI_BRACKET_PAIRS);
      printf("    -c <column>  Color column\n");
      printf("    -g           Disable mouse\n");
      printf("    -H <1|0>     Enable/disable headless mode (default: 1 if no tty, else 0)\n");
      printf("    -i <1|0>     Enable/disable smart_indent (default: %d)\n", EON_DEFAULT_SMART_INDENT);
      printf("    -K <kdef>    Set current kmap definition (use with -k)\n");
      printf("    -k <kbind>   Add key binding to current kmap definition (use with -K)\n");
      printf("    -l <ltype>   Set linenum type (default: 0)\n");
      printf("    -M <macro>   Add a macro\n");
      printf("    -m <key>     Set macro toggle key (default: %s)\n", EON_DEFAULT_MACRO_TOGGLE_KEY);
      printf("    -N           Skip reading of rc file\n");
      printf("    -n <kmap>    Set init kmap (default: eon_normal)\n");
      printf("    -p <macro>   Set startup macro\n");
      printf("    -S <syndef>  Set current syntax definition (use with -s)\n");
      printf("    -s <synrule> Add syntax rule to current syntax definition (use with -S)\n");
      printf("    -t <size>    Set tab size (default: %d)\n", EON_DEFAULT_TAB_WIDTH);
      printf("    -v           Print version and exit\n");
      printf("    -w <1|0>     Enable/disable soft word wrap (default: %d)\n", EON_DEFAULT_SOFT_WRAP);
      printf("    -y <syntax>  Set override syntax for files opened at start up\n");
      printf("    -z <1|0>     Enable/disable trim_paste (default: %d)\n", EON_DEFAULT_TRIM_PASTE);
      printf("\n");
      printf("    file         At start up, open file\n");
      printf("    file:line    At start up, open file at line\n");
      printf("    kdef         '<name>,<default_cmd>,<allow_fallthru>'\n");
      printf("    kbind        '<cmd>,<key>,<param>'\n");
      printf("    ltype        0=absolute, 1=relative, 2=both\n");
      printf("    macro        '<name> <key1> <key2> ... <keyN>'\n");
      printf("    syndef       '<name>,<path_pattern>,<tab_width>,<tab_to_space>'\n");
      printf("    synrule      '<start>,<end>,<fg>,<bg>'\n");
      printf("    fg,bg        0=default     1=black       2=red         3=green\n");
      printf("                 4=yellow      5=blue        6=magenta     7=cyan\n");
      printf("                 8=white       256=bold      512=underline 1024=reverse\n");
      rv = EON_ERR;
      break;

    case 'a':
      editor->tab_to_space = atoi(optarg) ? 1 : 0;
      break;

    case 'b':
      editor->highlight_bracket_pairs = atoi(optarg) ? 1 : 0;
      break;

    case 'c':
      editor->color_col = atoi(optarg);
      break;

    case 'g':
      editor->no_mouse = 1;
      break;

    case 'H':
      editor->headless_mode = atoi(optarg) ? 1 : 0;
      break;

    case 'i':
      editor->smart_indent = atoi(optarg) ? 1 : 0;
      break;

    case 'K':
      if (_editor_init_kmap_by_str(editor, &cur_kmap, optarg) != EON_OK) {
        EON_LOG_ERR("Could not init kmap by str: %s\n", optarg);
        editor->exit_code = EXIT_FAILURE;
        rv = EON_ERR;
      }
      break;

    case 'k':
      if (!cur_kmap || _editor_init_kmap_add_binding_by_str(editor, cur_kmap, optarg) != EON_OK) {
        EON_LOG_ERR("Could not add key binding to kmap %p by str: %s\n", cur_kmap, optarg);
        editor->exit_code = EXIT_FAILURE;
        rv = EON_ERR;
      }
      break;

    case 'l':
      editor->linenum_type = atoi(optarg);
      if (editor->linenum_type < -1 || editor->linenum_type > 2) editor->linenum_type = 0;
      break;

    case 'M':
      if (_editor_add_macro_by_str(editor, optarg) != EON_OK) {
        EON_LOG_ERR("Could not add macro by str: %s\n", optarg);
        editor->exit_code = EXIT_FAILURE;
        rv = EON_ERR;
      }
      break;

    case 'm':
      if (_editor_set_macro_toggle_key(editor, optarg) != EON_OK) {
        EON_LOG_ERR("Could not set macro key to: %s\n", optarg);
        editor->exit_code = EXIT_FAILURE;
        rv = EON_ERR;
      }
      break;

    case 'N':
      // See _editor_should_skip_rc
      break;

    case 'n':
      editor->kmap_init_name = strdup(optarg);
      break;

    case 'p':
      editor->startup_macro_name = strdup(optarg);
      break;

    case 'S':
      if (_editor_init_syntax_by_str(editor, &cur_syntax, optarg) != EON_OK) {
        EON_LOG_ERR("Could not init syntax by str: %s\n", optarg);
        editor->exit_code = EXIT_FAILURE;
        rv = EON_ERR;
      }
      break;

    case 's':
      if (!cur_syntax || _editor_init_syntax_add_rule_by_str(cur_syntax, optarg) != EON_OK) {
        EON_LOG_ERR("Could not add style rule to syntax %p by str: %s\n", cur_syntax, optarg);
        editor->exit_code = EXIT_FAILURE;
        rv = EON_ERR;
      }
      break;

    case 't':
      editor->tab_width = atoi(optarg);
      break;

    case 'v':
      printf("eon version %s\n", EON_VERSION);
      rv = EON_ERR;
      break;

    case 'w':
      editor->soft_wrap = atoi(optarg);
      break;

    case 'y':
      editor->syntax_override = optarg;
      break;

    case 'z':
      editor->trim_paste = atoi(optarg) ? 1 : 0;
      break;

    default:
      rv = EON_ERR;
      break;
    }
  }

  return rv;
}

// Init status bar
static void _editor_init_status(editor_t* editor) {
  editor->status = bview_new(editor, NULL, 0, NULL);
  editor->status->type = EON_BVIEW_TYPE_STATUS;
  editor->rect_status.fg = RECT_STATUS_FG;
  editor->rect_status.bg = RECT_STATUS_BG;
}

// Init bviews
static void _editor_init_bviews(editor_t* editor, int argc, char** argv) {
  int i;
  char *path;
  int path_len;

#ifdef __APPLE__
  optind++;
#endif


  // Open bviews
  if (optind >= argc) {
    // Open blank
    editor_open_bview(editor, NULL, EON_BVIEW_TYPE_EDIT, NULL, 0, 1, 0, NULL, NULL, NULL);

  } else {
    // Open files or directories
    for (i = optind; i < argc; i++) {
      path = argv[i];
      path_len = strlen(path);

      // if a single path was provided, keep a reference
      if (i == optind && i + 1 == argc && util_is_dir(path)) {
        editor->start_dir = path;
      }

      editor_open_bview(editor, NULL, EON_BVIEW_TYPE_EDIT, path, path_len, 1, 0, NULL, NULL, NULL);
    }
  }
}

// Init headless mode
static int _editor_init_headless_mode(editor_t* editor) {
  fd_set readfds;
  ssize_t nbytes;
  char buf[1024];
  bview_t* bview;

  if (!editor->headless_mode) return EON_OK;

  // Open blank bview
  editor_open_bview(editor, NULL, EON_BVIEW_TYPE_EDIT, NULL, 0, 1, 0, NULL, NULL, &bview);

  // If we have a pipe, read stdin into bview
  if (isatty(STDIN_FILENO) == 1) return EON_OK;

  do {
    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);
    select(STDIN_FILENO + 1, &readfds, NULL, NULL, NULL);
    nbytes = 0;

    if (FD_ISSET(STDIN_FILENO, &readfds)) {
      nbytes = read(STDIN_FILENO, &buf, 1024);

      if (nbytes > 0) {
        mark_insert_before(bview->active_cursor->mark, buf, nbytes);
      }
    }
  } while (nbytes > 0);

  return EON_OK;
}

// Init startup macro if present
static int _editor_init_startup_macro(editor_t* editor) {
  kmacro_t* macro;

  if (!editor->startup_macro_name) return EON_OK;

  macro = NULL;
  HASH_FIND_STR(editor->macro_map, editor->startup_macro_name, macro);

  if (!macro) return EON_ERR;

  editor->macro_apply = macro;
  editor->macro_apply_input_index = 0;
  return EON_OK;
}

// Init/deinit commands
static int _editor_init_or_deinit_commands(editor_t* editor, int is_deinit) {
  cmd_t* cmd;
  cmd_t* tmp;
  HASH_ITER(hh, editor->cmd_map, cmd, tmp) {
    if (cmd->func_init) {
      cmd->func_init(cmd, is_deinit);
    }
  }
  return EON_OK;
}
