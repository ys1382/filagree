#ifndef COMPILE_H

#include "vm.h"

struct byte_array *build_string(const struct byte_array *input, const struct byte_array* path);
struct byte_array *build_file(const struct byte_array* path);
void compile_file(const char* str);

#endif // COMPILE_H
