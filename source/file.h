#ifndef FILE_H
#define FILE_H

#include <stdbool.h>
#include <setjmp.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/stat.h>

#define RESERVED_DIR      "dir"
#define RESERVED_MODIFIED "modified"

struct byte_array *read_file(const struct byte_array *filename_ba, uint32_t from, long size);
int write_file(const struct byte_array* path, struct byte_array* bytes, uint32_t from, int32_t timestamp);
long fsize(FILE* file);
int file_list(const char *path, int (*fn)(const char*, bool, long, void*), void *context);
long file_modified(const char *path);

#endif // FILE_H