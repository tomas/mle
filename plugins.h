#include "mle.h"

int load_plugins(editor_t * editor);
int unload_plugins(void);
int trigger_plugin_event(const char * event, bview_t * view);
