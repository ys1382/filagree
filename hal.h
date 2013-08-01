// Hardware Abstraction Layer

#ifndef HAL_H
#define HAL_H

#include <stdbool.h>
#include <inttypes.h>
#include "struct.h"
#include "vm.h"

void hal_print(const char *str);
void hal_loop();
void hal_save(struct context *context, const struct byte_array *key, const struct variable *value);
struct variable *hal_load(struct context *context, const struct byte_array *key);

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
void *hal_label (int32_t x, int32_t y,
                 int32_t *w, int32_t *h,
                 const char *str);
void *hal_input (struct variable *uictx,
                 int32_t x, int32_t y,
                 int32_t *w, int32_t *h,
                 struct variable *hint,
                 bool multiline);
void *hal_button(struct context *context,
                 struct variable *uictx,
                 int32_t x, int32_t y, int32_t *w, int32_t *h,
                 struct variable *logic,
                 const char *str, const char *img);
void *hal_table (struct context *context,
                 struct variable *uictx,
                 int x, int y, int w, int h,
                 struct variable *list, struct variable *logic);

struct variable *hal_ui_get(struct context *context, void *field);
void hal_ui_set(void *field, struct variable *value);

void hal_file_listen(struct context *context, const char *path, struct variable *listener);


#endif // NO_UI

#endif // HAL_H