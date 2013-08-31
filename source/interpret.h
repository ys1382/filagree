//
//  interpret.h
//  filagree
//

#ifndef INTERPRET_H
#define INTERPRET_H

#include "vm.h"

void interpret_string(struct context *context, struct byte_array *script);

#endif // INTERPRET_H
