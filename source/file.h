#ifndef FILE_H
#define FILE_H

#include <stdbool.h>
#include <setjmp.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/stat.h>

struct byte_array *read_file(const struct byte_array *filename);
int write_file(const struct byte_array* filename, struct byte_array* bytes, int32_t timestamp);
long fsize(FILE* file);
int file_list(const char *path, int (*fn)(const char*, bool, long, void*), void *context);
long file_modified(const char *path);

#endif // FILE_H