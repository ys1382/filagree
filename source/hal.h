// Hardware Abstraction Layer

#ifndef HAL_H
#define HAL_H

#include <stdbool.h>
#include <inttypes.h>
#include "struct.h"
#include "vm.h"


enum HAL_Event {
    CONNECTED,
    DISCONNECTED,
    MESSAGED,
    SENT,
    FILED,
    ERROR,
};


struct variable *event_string(struct context *context, enum HAL_Event event);


void hal_sleep(int32_t miliseconds);
void hal_timer(struct context *context,
               int32_t milliseconds,
               struct variable *logic,
               bool repeats);

void hal_file_listen(struct context *context, const char *path, struct variable *listener);
const char *hal_doc_path(const struct byte_array *path);
bool hal_open(const char *path);

void hal_loop(struct context *context);

#ifndef NO_UI

void hal_image();
void hal_sound();
void hal_audioloop();
void hal_graphics(const struct variable *shape);
void hal_synth(const uint8_t *bytes, uint32_t length);

void *hal_window(struct context *context,
                 struct variable *uictx,
                 int32_t *w, int32_t *h,
                 struct variable *logic);
void *hal_label (struct variable *uictx,
                 int32_t *w, int32_t *h,
                 const char *str);
void *hal_input (struct variable *uictx,
                 int32_t *w, int32_t *h,
                 const char *hint,
                 bool multiline,
                 bool readonly);
void *hal_button(struct context *context,
                 struct variable *uictx,
                 int32_t *w, int32_t *h,
                 struct variable *logic,
                 const char *str, const char *img);
void *hal_table (struct context *context,
                 struct variable *uictx,
                 struct variable *list,
                 struct variable *logic);
void hal_alert  (struct context *context,
                 const char *title,
                 const char *message,
                 struct variable *callback
                 struct variable *params);

struct variable *hal_ui_get(struct context *context, void *field);
void hal_ui_set(void *field, struct variable *value);
void hal_ui_put(void *field, int32_t x, int32_t y, int32_t w, int32_t h);


#endif // NO_UI

#endif // HAL_H