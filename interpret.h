//
//  interpret.h
//  filagree
//

#ifndef INTERPRET_H
#define INTERPRET_H

#include "vm.h"

void interpret_file(const struct byte_array *filename, struct variable *find);
void interpret_string(const char *str, struct variable *find);

#endif // INTERPRET_H
