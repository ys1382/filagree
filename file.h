#ifndef FILE_H
#define FILE_H

#include <stdbool.h>
#include <setjmp.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/stat.h>


#define ERROR_FSIZE     "Could not get length of file"
#define ERROR_FOPEN     "Could not open file"
#define ERROR_FREAD     "Could not read file"
#define ERROR_FCLOSE    "Could not close file"

struct byte_array *read_file(const struct byte_array *filename);
int write_file(const struct byte_array* filename, struct byte_array* bytes);
long fsize(FILE* file);
int file_list(const char *path, int (*fn)(const char*, bool, void*), void *context);

#endif // FILE_H