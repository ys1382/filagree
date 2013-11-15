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

#endif // INTERPRET_H
