#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <unistd.h>
#include "termbox.h"
#include "uthash.h"
#include "mlbuf.h"
#include "eon.h"

editor_t _editor;

int main(int argc, char** argv) {

  memset(&_editor, 0, sizeof(editor_t));
  setlocale(LC_ALL, "");

  if (editor_init(&_editor, argc, argv) == EON_OK) {

    if (!_editor.headless_mode) {
      _editor.linenum_type = EON_LINENUM_TYPE_NONE;
      tb_init();

      if (!_editor.no_mouse)
        tb_enable_mouse();

      // tb_select_output_mode(TB_OUTPUT_256);
    }

    editor_run(&_editor);

    if (_editor.headless_mode && _editor.active_edit) {
      buffer_write_to_fd(_editor.active_edit->buffer, STDOUT_FILENO, NULL);
    }

    editor_deinit(&_editor);

    // shut down termbox if not on headless mode
    if (!_editor.headless_mode) {
      tb_shutdown();
    }

  } else { // init failed
    editor_deinit(&_editor);
  }

  return _editor.exit_code;
}
