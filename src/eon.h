#ifndef __EON_H
#define __EON_H

#include <stdint.h>
#include <limits.h>
#include "termbox.h"
#include "uthash.h"
#include "mlbuf.h"

typedef struct editor_s editor_t; // A container for editor-wide globals
typedef struct bview_s bview_t; // A view of a buffer
typedef struct bview_rect_s bview_rect_t; // A rectangle in bview with a default styling
typedef struct bview_listener_s bview_listener_t; // A listener to buffer events in a bview
typedef void (*bview_listener_cb_t)(bview_t* bview, baction_t* action, void* udata); // A bview_listener_t callback
typedef struct cursor_s cursor_t; // A cursor (insertion mark + selection bound mark) in a buffer
typedef struct loop_context_s loop_context_t; // Context for a single _editor_loop
typedef struct cmd_s cmd_t; // A command definition
typedef struct cmd_context_s cmd_context_t; // Context for a single command invocation
typedef struct kinput_s kinput_t; // A single key input (similar to a tb_event from termbox)
typedef struct kmacro_s kmacro_t; // A sequence of kinputs and a name
typedef struct kmap_s kmap_t; // A map of keychords to functions
typedef struct kmap_node_s kmap_node_t; // A node in a list of keymaps
typedef struct kbinding_def_s kbinding_def_t; // A definition of a keymap
typedef struct kbinding_s kbinding_t; // A single binding in a keymap
typedef struct syntax_s syntax_t; // A syntax definition
typedef struct syntax_node_s syntax_node_t; // A node in a linked list of syntaxes
typedef struct srule_def_s srule_def_t; // A definition of a syntax
typedef struct async_proc_s async_proc_t; // An asynchronous process
typedef void (*async_proc_cb_t)(async_proc_t* self, char* buf, size_t buf_len); // An async_proc_t callback
typedef struct editor_prompt_params_s editor_prompt_params_t; // Extra params for editor_prompt
typedef struct tb_event tb_event_t; // A termbox event
typedef struct prompt_history_s prompt_history_t; // A map of prompt histories keyed by prompt_str
typedef struct prompt_hnode_s prompt_hnode_t; // A node in a linked list of prompt history
typedef int (*cmd_func_t)(cmd_context_t* ctx); // A command function
typedef int (*cb_func_t)(cmd_context_t* ctx, char * action); // A command function


// kinput_t
struct kinput_s {
    uint32_t ch;
    uint16_t key;
    uint8_t meta;
};

// bview_rect_t
struct bview_rect_s {
    int x;
    int y;
    int w;
    int h;
    uint16_t fg;
    uint16_t bg;
};

// editor_t
struct editor_s {
    int w;
    int h;
    bview_t* top_bviews;
    bview_t* all_bviews;
    bview_t* active;
    bview_t* active_edit;
    bview_t* active_edit_root;
    bview_t* status;
    bview_t* prompt;
    bview_rect_t rect_edit;
    bview_rect_t rect_status;
    bview_rect_t rect_prompt;
    syntax_t* syntax_map;
    int is_display_disabled;
    kmacro_t* macro_map;
    kinput_t macro_toggle_key;
    kmacro_t* macro_record;
    kmacro_t* macro_apply;
    size_t macro_apply_input_index;
    int is_recording_macro;
    char* startup_macro_name;
    cmd_t* cmd_map;
    kmap_t* kmap_map;
    kmap_t* kmap_normal;
    kmap_t* kmap_prompt_input;
    kmap_t* kmap_prompt_yn;
    kmap_t* kmap_prompt_yna;
    kmap_t* kmap_prompt_ok;
    kmap_t* kmap_prompt_isearch;
    kmap_t* kmap_prompt_menu;
    kmap_t* kmap_menu;
    prompt_history_t* prompt_history;
    char* kmap_init_name;
    kmap_t* kmap_init;
    async_proc_t* async_procs;
    FILE* tty;
    int ttyfd;
    char* syntax_override;
    int linenum_type;
    int tab_width;
    int tab_to_space;
    int trim_paste;
    int smart_indent;
    int read_rc_file;
    int highlight_bracket_pairs;
    int color_col;
    int soft_wrap;
    int viewport_scope_x; // TODO cli option
    int viewport_scope_y; // TODO cli option
    int headless_mode;
    loop_context_t* loop_ctx;
    int loop_depth;
    int is_in_init;
    char* insertbuf;
    size_t insertbuf_size;
    #define EON_ERRSTR_SIZE 256
    char errstr[EON_ERRSTR_SIZE];
    char infostr[EON_ERRSTR_SIZE];
    int exit_code;
    int bview_tab_width;
    int no_mouse;
    char * start_dir;
};

// srule_def_t
struct srule_def_s {
    char* re;
    char* re_end;
    uint16_t fg;
    uint16_t bg;
};

// syntax_node_t
struct syntax_node_s {
    srule_t* srule;
    syntax_node_t* next;
    syntax_node_t* prev;
};

// syntax_t
struct syntax_s {
    char* name;
    char* path_pattern;
    int tab_width;
    int tab_to_space;
    srule_node_t* srules;
    UT_hash_handle hh;
};

// bview_t
struct bview_s {
    #define EON_BVIEW_TYPE_EDIT 0
    #define EON_BVIEW_TYPE_STATUS 1
    #define EON_BVIEW_TYPE_PROMPT 2
    editor_t* editor;
    int x;
    int y;
    int w;
    int h;
    int is_resized;
    int type;
    int linenum_width;
    int abs_linenum_width;
    int rel_linenum_width;
    bview_rect_t rect_caption;
    bview_rect_t rect_lines;
    bview_rect_t rect_margin_left;
    bview_rect_t rect_buffer;
    bview_rect_t rect_margin_right;
    buffer_t* buffer;
    bint_t viewport_x;
    bint_t viewport_x_vcol;
    bint_t viewport_y;
    bline_t* viewport_bline;
    int viewport_scope_x;
    int viewport_scope_y;
    bview_t* split_parent;
    bview_t* split_child;
    float split_factor;
    int split_is_vertical;
    char* prompt_str;
    // char* path;
    bint_t startup_linenum;
    kmap_node_t* kmap_stack;
    kmap_node_t* kmap_tail;
    cursor_t* cursors;
    cursor_t* active_cursor;
    char* last_search;
    srule_t* isearch_rule;
    int tab_width;
    int tab_to_space;
    syntax_t* syntax;
    async_proc_t* async_proc;
    cb_func_t menu_callback;
    int is_menu;
    char init_cwd[PATH_MAX + 1];
    bview_listener_t* listeners;
    bview_t* top_next;
    bview_t* top_prev;
    bview_t* all_next;
    bview_t* all_prev;
};

// bview_listener_t
struct bview_listener_s {
    bview_listener_cb_t callback;
    void* udata;
    bview_listener_t* next;
    bview_listener_t* prev;
};

// cursor_t
struct cursor_s {
    bview_t* bview;
    mark_t* mark;
    mark_t* anchor;
    int is_anchored;
    int is_asleep;
    srule_t* sel_rule;
    char* cut_buffer;
    cursor_t* next;
    cursor_t* prev;
};

// kmacro_t
struct kmacro_s {
    char* name;
    kinput_t* inputs;
    size_t inputs_len;
    size_t inputs_cap;
    UT_hash_handle hh;
};

// cmd_t
struct cmd_s {
    char* name;
    cmd_func_t func;
    int (*func_viewport)(cmd_t* self);
    int (*func_init)(cmd_t* self, int is_deinit);
    int (*func_display)(cmd_t* self);
    void* udata;
    int is_resolved;
    int is_dead;
    UT_hash_handle hh;
};

// kbinding_def_t
struct kbinding_def_s {
    #define EON_KBINDING_DEF(pcmdname, pkeypatt)             { (pcmdname), (pkeypatt), NULL }
    #define EON_KBINDING_DEF_EX(pcmdname, pkeypatt, pstatp)  { (pcmdname), (pkeypatt), (pstatp) }
    char* cmd_name;
    char* key_patt;
    char* static_param;
};

// kbinding_t
struct kbinding_s {
    kinput_t input;
    char* cmd_name;
    cmd_t* cmd;
    char* static_param;
    char* key_patt;
    int is_leaf;
    kbinding_t* children;
    UT_hash_handle hh;
};

// kmap_node_t
struct kmap_node_s {
    kmap_t* kmap;
    bview_t* bview;
    kmap_node_t* next;
    kmap_node_t* prev;
};

// kmap_t
struct kmap_s {
    char* name;
    kbinding_t* bindings;
    int allow_fallthru;
    char* default_cmd_name;
    cmd_t* default_cmd;
    UT_hash_handle hh;
};

// cmd_context_t
struct cmd_context_s {
    #define EON_PASTEBUF_INCR 1024
    editor_t* editor;
    loop_context_t* loop_ctx;
    cmd_t* cmd;
    buffer_t* buffer;
    bview_t* bview;
    cursor_t* cursor;
    kinput_t input;
    char* static_param;
    int is_user_input;
    kinput_t* pastebuf;
    size_t pastebuf_len;
    size_t pastebuf_size;
    int has_pastebuf_leftover;
    kinput_t pastebuf_leftover;
};

// loop_context_t
struct loop_context_s {
    #define EON_LOOP_CTX_MAX_NUMERIC_LEN 20
    #define EON_LOOP_CTX_MAX_NUMERIC_PARAMS 8
    #define EON_LOOP_CTX_MAX_WILDCARD_PARAMS 8
    #define EON_LOOP_CTX_MAX_COMPLETE_TERM_SIZE 256
    bview_t* invoker;
    char numeric[EON_LOOP_CTX_MAX_NUMERIC_LEN + 1];
    kbinding_t* numeric_node;
    int numeric_len;
    uintmax_t numeric_params[EON_LOOP_CTX_MAX_NUMERIC_PARAMS];
    int numeric_params_len;
    uint32_t wildcard_params[EON_LOOP_CTX_MAX_WILDCARD_PARAMS];
    int wildcard_params_len;
    kbinding_t* binding_node;
    int need_more_input;
    int should_exit;
    char* prompt_answer;
    // cmd_func_t prompt_callack;
    prompt_hnode_t* prompt_hnode;
    int tab_complete_index;
    char tab_complete_term[EON_LOOP_CTX_MAX_COMPLETE_TERM_SIZE];
    cmd_t* last_cmd;
    str_t last_insert;
};

// async_proc_t
struct async_proc_s {
    editor_t* editor;
    void* owner;
    async_proc_t** owner_aproc;
    FILE* rpipe;
    FILE* wpipe;
    pid_t pid;
    int rfd;
    int wfd;
    int is_done;
    int is_solo;
    async_proc_cb_t callback;
    async_proc_t* next;
    async_proc_t* prev;
};

// editor_prompt_params_t
struct editor_prompt_params_s {
    char* data;
    int data_len;
    kmap_t* kmap;
    bview_listener_cb_t prompt_cb;
    void* prompt_cb_udata;
};

// prompt_history_t
struct prompt_history_s {
    char* prompt_str;
    prompt_hnode_t* prompt_hlist;
    UT_hash_handle hh;
};

// prompt_hnode_t
struct prompt_hnode_s {
    char* data;
    bint_t data_len;
    prompt_hnode_t* prev;
    prompt_hnode_t* next;
};

// editor functions
int editor_init(editor_t* editor, int argc, char** argv);
int editor_deinit(editor_t* editor);
int editor_run(editor_t* editor);
int editor_bview_edit_count(editor_t* editor);
int editor_close_bview(editor_t* editor, bview_t* bview, int* optret_num_closed);
int editor_count_bviews_by_buffer(editor_t* editor, buffer_t* buffer);
int editor_page_menu(editor_t* editor, cb_func_t callback, char* opt_buf_data, int opt_buf_data_len, async_proc_t* opt_aproc, bview_t** optret_menu);
int editor_prompt_menu(editor_t* editor, cb_func_t callback, char* opt_buf_data, int opt_buf_data_len);
int editor_open_bview(editor_t* editor, bview_t* parent, int type, char* opt_path, int opt_path_len, int make_active, bint_t linenum, bview_rect_t* opt_rect, buffer_t* opt_buffer, bview_t** optret_bview);
int editor_prompt(editor_t* editor, char* prompt, editor_prompt_params_t* params, char** optret_answer);
int editor_close_prompt(editor_t* editor, bview_t * invoker);
int editor_set_prompt_str(editor_t* editor, char * str);
int editor_set_active(editor_t* editor, bview_t* bview);
int editor_register_cmd(editor_t* editor, cmd_t* cmd);
int editor_add_binding_to_keymap(editor_t* editor, kmap_t* kmap, kbinding_def_t* binding_def);

// bview functions
bview_t* bview_get_split_root(bview_t* self);
bview_t* bview_new(editor_t* editor, char* opt_path, int opt_path_len, buffer_t* opt_buffer);
bview_t* bview_new_cwd(editor_t* editor, char* opt_path, int opt_path_len, const char* opt_cwd, buffer_t* opt_buffer);
int bview_add_cursor_asleep(bview_t* self, bline_t* bline, bint_t col, cursor_t** optret_cursor);
int bview_add_cursor(bview_t* self, bline_t* bline, bint_t col, cursor_t** optret_cursor);
int bview_add_listener(bview_t* self, bview_listener_cb_t callback, void* udata);
int bview_set_line_bg(bview_t* self, bint_t line_index, int color);
int bview_move_to_line(bview_t* self, bint_t number);
int bview_scroll_viewport(bview_t* self, int offset);
int bview_center_viewport_y(bview_t* self);
int bview_destroy(bview_t* self);
int bview_destroy_listener(bview_t* self, bview_listener_t* listener);
int bview_draw(bview_t* self);
int bview_draw_cursor(bview_t* self, int set_real_cursor);
int bview_get_active_cursor_count(bview_t* self);
int bview_get_screen_coords(bview_t* self, mark_t* mark, int* ret_x, int* ret_y, struct tb_cell** optret_cell);
int bview_max_viewport_y(bview_t* self);
int bview_open(bview_t* self, char* path, int path_len);
int bview_pop_kmap(bview_t* bview, kmap_t** optret_kmap);
int bview_push_kmap(bview_t* bview, kmap_t* kmap);
int bview_rectify_viewport(bview_t* self);
int bview_remove_cursor(bview_t* self, cursor_t* cursor);
int bview_remove_cursors_except(bview_t* self, cursor_t* one);
int bview_resize(bview_t* self, int x, int y, int w, int h);
int bview_set_syntax(bview_t* self, char* opt_syntax);
int bview_split(bview_t* self, int is_vertical, float factor, bview_t** optret_bview);
int bview_unsplit(bview_t* parent, bview_t* child);
int bview_wake_sleeping_cursors(bview_t* self);
int bview_zero_viewport_y(bview_t* self);

// cursor functions
int cursor_clone(cursor_t* cursor, int use_srules, cursor_t** ret_clone);
int cursor_cut_copy(cursor_t* cursor, int is_cut, int use_srules, int append);
int cursor_destroy(cursor_t* cursor);
int cursor_drop_anchor(cursor_t* cursor, int use_srules);
int cursor_get_lo_hi(cursor_t* cursor, mark_t** ret_lo, mark_t** ret_hi);
int cursor_lift_anchor(cursor_t* cursor);
int cursor_replace(cursor_t* cursor, int interactive, char* opt_regex, char* opt_replacement);
int cursor_select_between(cursor_t* cursor, mark_t* a, mark_t* b, int use_srules);
int cursor_select_by(cursor_t* cursor, const char* strat);
int cursor_select_by_bracket(cursor_t* cursor);
int cursor_select_by_string(cursor_t* cursor);
int cursor_select_by_word_back(cursor_t* cursor);
int cursor_select_by_word(cursor_t* cursor);
int cursor_select_by_word_forward(cursor_t* cursor);
int cursor_toggle_anchor(cursor_t* cursor, int use_srules);
int cursor_uncut(cursor_t* cursor);

// cmd functions
int cmd_apply_macro_by(cmd_context_t* ctx);
int cmd_apply_macro(cmd_context_t* ctx);
int cmd_browse(cmd_context_t* ctx);
int cmd_close(cmd_context_t* ctx);
int cmd_copy_by(cmd_context_t* ctx);
int cmd_copy(cmd_context_t* ctx);
int cmd_ctag(cmd_context_t* ctx);
int cmd_cut_by(cmd_context_t* ctx);
int cmd_cut(cmd_context_t* ctx);
int cmd_cut_or_close(cmd_context_t* ctx);
int cmd_delete_after(cmd_context_t* ctx);
int cmd_delete_before(cmd_context_t* ctx);
int cmd_delete_word_after(cmd_context_t* ctx);
int cmd_delete_word_before(cmd_context_t* ctx);
int cmd_drop_cursor_column(cmd_context_t* ctx);
int cmd_drop_sleeping_cursor(cmd_context_t* ctx);
int cmd_find_word(cmd_context_t* ctx);
int cmd_fsearch(cmd_context_t* ctx);
int cmd_grep(cmd_context_t* ctx);
int cmd_indent(cmd_context_t* ctx);
int cmd_insert_data(cmd_context_t* ctx);
int cmd_insert_newline_above(cmd_context_t* ctx);
int cmd_insert_newline(cmd_context_t* ctx);
int cmd_insert_tab(cmd_context_t* ctx);
int cmd_isearch(cmd_context_t* ctx);
int cmd_less(cmd_context_t* ctx);
int cmd_mouse_move(cmd_context_t* ctx, int mouse_down, int x, int y);
int cmd_scroll_up(cmd_context_t* ctx);
int cmd_scroll_down(cmd_context_t* ctx);
int cmd_move_beginning(cmd_context_t* ctx);
int cmd_move_bol(cmd_context_t* ctx);
int cmd_move_bracket_back(cmd_context_t* ctx);
int cmd_move_bracket_forward(cmd_context_t* ctx);
int cmd_move_down(cmd_context_t* ctx);
int cmd_move_end(cmd_context_t* ctx);
int cmd_move_eol(cmd_context_t* ctx);
int cmd_move_left(cmd_context_t* ctx);
int cmd_move_page_down(cmd_context_t* ctx);
int cmd_move_page_up(cmd_context_t* ctx);
int cmd_move_relative(cmd_context_t* ctx);
int cmd_move_right(cmd_context_t* ctx);
int cmd_move_to_line(cmd_context_t* ctx);
int cmd_move_until_back(cmd_context_t* ctx);
int cmd_move_until_forward(cmd_context_t* ctx);
int cmd_move_up(cmd_context_t* ctx);
int cmd_move_word_back(cmd_context_t* ctx);
int cmd_move_word_forward(cmd_context_t* ctx);
int cmd_next(cmd_context_t* ctx);
int cmd_noop(cmd_context_t* ctx);
int cmd_open_file(cmd_context_t* ctx);
int cmd_open_new(cmd_context_t* ctx);
int cmd_open_replace_file(cmd_context_t* ctx);
int cmd_open_replace_new(cmd_context_t* ctx);
int cmd_outdent(cmd_context_t* ctx);
int cmd_pop_kmap(cmd_context_t* ctx);
int cmd_prev(cmd_context_t* ctx);
int cmd_push_kmap(cmd_context_t* ctx);
int cmd_quit(cmd_context_t* ctx);
int cmd_redo(cmd_context_t* ctx);
int cmd_redraw(cmd_context_t* ctx);
int cmd_remove_extra_cursors(cmd_context_t* ctx);
int cmd_replace(cmd_context_t* ctx);
int cmd_save_as(cmd_context_t* ctx);
int cmd_save(cmd_context_t* ctx);
int cmd_search(cmd_context_t* ctx);
int cmd_search_next(cmd_context_t* ctx);
int cmd_select_beginning(cmd_context_t* ctx);
int cmd_select_end(cmd_context_t* ctx);
int cmd_select_bol(cmd_context_t* ctx);
int cmd_select_eol(cmd_context_t* ctx);
int cmd_select_up(cmd_context_t* ctx);
int cmd_select_down(cmd_context_t* ctx);
int cmd_select_left(cmd_context_t* ctx);
int cmd_select_right(cmd_context_t* ctx);
int cmd_select_word_back(cmd_context_t* ctx);
int cmd_select_word_forward(cmd_context_t* ctx);
int cmd_select_current_word(cmd_context_t* ctx);
int cmd_select_current_line(cmd_context_t* ctx);
int cmd_new_cursor_up(cmd_context_t* ctx);
int cmd_new_cursor_down(cmd_context_t* ctx);
int cmd_set_opt(cmd_context_t* ctx);
int cmd_shell(cmd_context_t* ctx);
int cmd_show_help(cmd_context_t* ctx);
int cmd_split_horizontal(cmd_context_t* ctx);
int cmd_split_vertical(cmd_context_t* ctx);
int cmd_toggle_anchor(cmd_context_t* ctx);
int cmd_toggle_mouse_mode(cmd_context_t* ctx);
int cmd_uncut(cmd_context_t* ctx);
int cmd_undo(cmd_context_t* ctx);
int cmd_viewport_bot(cmd_context_t* ctx);
int cmd_viewport_mid(cmd_context_t* ctx);
int cmd_viewport_top(cmd_context_t* ctx);
int cmd_wake_sleeping_cursors(cmd_context_t* ctx);

// async functions
async_proc_t* async_proc_new(editor_t* editor, void* owner, async_proc_t** owner_aproc, char* shell_cmd, int rw, async_proc_cb_t callback);
int async_proc_set_owner(async_proc_t* aproc, void* owner, async_proc_t** owner_aproc);
int async_proc_destroy(async_proc_t* aproc, int preempt);
int async_proc_drain_all(async_proc_t* aprocs, int* ttyfd);

// util functions
const char * util_get_url(const char * url);
size_t util_download_file(const char * url, const char * target);
int util_shell_exec(editor_t* editor, char* cmd, long timeout_s, char* input, size_t input_len, char* opt_shell, char** optret_output, size_t* optret_output_len);
int util_popen2(char* cmd, char* opt_shell, int* optret_fdread, int* optret_fdwrite, pid_t* optret_pid);
int util_get_bracket_pair(uint32_t ch, int* optret_is_closing);
int util_is_file(char* path, char* opt_mode, FILE** optret_file);
int util_is_dir(char* path);
char * util_read_file(char* path);
void util_expand_tilde(char* path, int path_len, char** ret_path);
int util_pcre_match(char* re, char* subject, int subject_len, char** optret_capture, int* optret_capture_len);
int util_pcre_replace(char* re, char* subj, char* repl, char** ret_result, int* ret_result_len);
int util_timeval_is_gt(struct timeval* a, struct timeval* b);
char* util_escape_shell_arg(char* str, int l);
int rect_printf(bview_rect_t rect, int x, int y, uint16_t fg, uint16_t bg, const char *fmt, ...);
int rect_printf_attr(bview_rect_t rect, int x, int y, const char *fmt, ...);
void str_append_stop(str_t* str, char* data, char* data_stop);
void str_append(str_t* str, char* data);
void str_append_len(str_t* str, char* data, size_t data_len);
void str_ensure_cap(str_t* str, size_t cap);
void str_clear(str_t* str);
void str_free(str_t* str);
void str_append_replace_with_backrefs(str_t* str, char* subj, char* repl, int pcre_rc, int* pcre_ovector, int pcre_ovecsize);

// plugin_opt struct, used on plugin boot
// when doing get_option(value)
typedef struct plugin_opt {
  int type;
  char * value;
} plugin_opt;

// Globals
extern editor_t _editor;
extern cmd_context_t * plugin_ctx;

// Macros
#define EON_VERSION "1.1.1"

#define EON_OK 0
#define EON_ERR 1

#define EON_PROMPT_YES "yes"
#define EON_PROMPT_NO "no"
#define EON_PROMPT_ALL "all"

#define EON_DEFAULT_TAB_WIDTH 2
#define EON_DEFAULT_TAB_TO_SPACE 1
#define EON_DEFAULT_TRIM_PASTE 1
#define EON_DEFAULT_SMART_INDENT 0
#define EON_DEFAULT_MACRO_TOGGLE_KEY "M-r"
#define EON_DEFAULT_HILI_BRACKET_PAIRS 1
#define EON_DEFAULT_READ_RC_FILE 1
#define EON_DEFAULT_SOFT_WRAP 0

#define EON_LOG_ERR(fmt, ...) do { \
    fprintf(stderr, (fmt), __VA_ARGS__); \
} while (0)

#define EON_SET_ERR(editor, fmt, ...) do { \
    snprintf((editor)->errstr, EON_ERRSTR_SIZE, (fmt), __VA_ARGS__); \
} while (0)

#define EON_SET_INFO(editor, fmt, ...) do { \
    snprintf((editor)->infostr, EON_ERRSTR_SIZE, (fmt), __VA_ARGS__); \
} while (0)

#define EON_RETURN_ERR(editor, fmt, ...) do { \
    EON_SET_ERR((editor), (fmt), __VA_ARGS__); \
    return EON_ERR; \
} while (0)

#define EON_MIN(a,b) (((a)<(b)) ? (a) : (b))
#define EON_MAX(a,b) (((a)>(b)) ? (a) : (b))

#define EON_BVIEW_IS_EDIT(bview) ((bview)->type == EON_BVIEW_TYPE_EDIT)
#define EON_BVIEW_IS_MENU(bview) ((bview)->is_menu && EON_BVIEW_IS_EDIT(bview))
#define EON_BVIEW_IS_POPUP(bview) ((bview)->type == EON_BVIEW_TYPE_POPUP)
#define EON_BVIEW_IS_STATUS(bview) ((bview)->type == EON_BVIEW_TYPE_STATUS)
#define EON_BVIEW_IS_PROMPT(bview) ((bview)->type == EON_BVIEW_TYPE_PROMPT)

#define EON_MARK_COL_TO_VCOL(pmark) ( \
    (pmark)->col >= (pmark)->bline->char_count \
    ? (pmark)->bline->char_vwidth \
    : ( (pmark)->col <= 0 ? 0 : (pmark)->bline->chars[(pmark)->col].vcol ) \
)

#define EON_COL_TO_VCOL(pline, pcol, pmax) ( \
    (pcol) >= (pline)->char_count \
    ? (pmax) \
    : ( (pcol) <= 0 ? 0 : (pline)->chars[(pcol)].vcol ) \
)

// Sentinel values for numeric and wildcard kinputs
#define EON_KINPUT_NUMERIC (kinput_t){ 0xffffffff, 0xffff, 0x40 }
#define EON_KINPUT_WILDCARD (kinput_t){ 0xffffffff, 0xffff, 0x80 }

#define EON_LINENUM_TYPE_NONE -1
#define EON_LINENUM_TYPE_ABS 0
#define EON_LINENUM_TYPE_REL 1
#define EON_LINENUM_TYPE_BOTH 2

#define EON_PARAM_WILDCARD(pctx, pn) ( \
    (pn) < (pctx)->loop_ctx->wildcard_params_len \
    ? (pctx)->loop_ctx->wildcard_params[(pn)] \
    : 0 \
)

#define EON_BRACKET_PAIR_MAX_SEARCH 10000
#define EON_RE_WORD_FORWARD "((?<=\\w)\\W|$)"
#define EON_RE_WORD_BACK "((?<=\\W)\\w|^)"

/*
TODO
--- HIGH
[ ] pass in (bline_t* opt_hint) to buffer_get_* and start from there instead of first_line
[ ] refactor buffer_set_mmapped to avoid huge mallocs
[ ] review default key bindings
[ ] review lel command letters
[ ] guard against mixed api use, refcounting
[ ] overlapping multi rules / range+hili should be separate in styling / srule priority / isearch hili in middle of multiline rule
    [ ] rewrite _buffer_apply_styles_multis and _buffer_bline_apply_style_multi
    [ ] get rid of bol_rule
    [ ] test at tests/test_buffer_srule_overlap.c
    [ ] bugfix: insert lines, drop anchor at eof, delete up, type on 1st line, leftover styling?
[ ] crash when M-e cat'ing huge files? (why does malloc crash program with large values?)
[ ] move macros out of eon.h if only used in one source file
--- LOW
[ ] after bad shell cmd, EBADF on stdin/stdout ?
[ ] consider find_budge=0 by default, emulate find_budge=1 in calling code
[ ] use_srules sucks
[ ] undo/redo should center viewport?
[ ] smart indent
[ ] func_viewport, func_display
[ ] ctrl-enter in prompt inserts newline
[ ] when opening path check if a buffer exists that already has it open via inode
[ ] undo stack with same loop# should get undone as a group option
[ ] refactor kmap, ** and ## is kind of inelegant, trie code not easy to grok
[ ] refactor aproc and menu code
[ ] ensure multi_cursor_code impl for all appropriate
[ ] segfault hunt: async proc broken pipe
[ ] use EON_RETURN_ERR more
[ ] pgup/down in isearch to control viewport
[ ] drop/goto mark with char
[ ] last cmd status code indicator
[ ] click to set cursor/focus
[ ] buffer_repeat
[ ] multi-level undo/redo
[ ] prompt history view
[ ] bview_config_t to use in editor and individual bviews
[ ] configurable colors, status line, caption line
*/

#endif
