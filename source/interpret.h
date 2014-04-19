//
//  interpret.h
//  filagree
//

#ifndef INTERPRET_H
#define INTERPRET_H

#include "vm.h"

void interpret_string(struct context *context, struct byte_array *script);
void interpret_file(struct byte_array *path,
                    struct byte_array *args);

// run a file, using the same context
struct context *interpret_file_with(struct context *context, struct byte_array *path);
struct context *interpret_string_with(struct context *context, struct byte_array *script);


#endif // INTERPRET_H
