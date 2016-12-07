#include "mle.h"

int load_plugins(editor_t * editor);
int unload_plugins(void);
int trigger_plugin_event(char * event, cmd_context_t ctx);
