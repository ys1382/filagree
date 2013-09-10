#ifndef COMPILE_H

#include "vm.h"

struct byte_array *build_string(const struct byte_array *input);
struct byte_array *build_file(const struct byte_array* filename);
void compile_file(const char* str);

#endif // COMPILE_H
