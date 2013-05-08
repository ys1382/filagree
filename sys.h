#ifndef SYS_H
#define SYS_H


#include "vm.h"


struct variable *sys_new(struct context *context);

struct variable *sys_func(struct context *context, struct byte_array *name);

struct variable *builtin_method(struct context *context,
								struct variable *indexable,
                                const struct variable *index);

char *param_str(const struct variable *value, uint32_t index);

int32_t param_int(const struct variable *value, uint32_t index);

struct variable *param_var(struct context *context, const struct variable *value, uint32_t index);


#endif // SYS_H