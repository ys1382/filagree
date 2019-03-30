#include "struct.h"
#include "util.h"
#include "file.h"
#include "sys.h"

#include <string.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/types.h>
#include <errno.h>
#include <fts.h>
#include <limits.h>
#include <utime.h>

#define INPUT_MAX_LEN    100000

long fsize(FILE* file) {
    const char *pe = "fseek";
    if (!fseek(file, 0, SEEK_END)) {
        long size = ftell(file);
        if (size < 0) {
            pe = "ftell";
        } else if (!fseek(file, 0, SEEK_SET)) {
            return size;
        }
    }
    perror(pe);
    return -1;
}

long file_size(const char *path) {
    FILE * file;
    if (!(file = fopen(path, "rb")))
        return -1;
    return fsize(file);
}

struct byte_array *read_file(const struct byte_array *filename_ba, uint32_t offset, long size) {
    FILE * file;
    char *str;
    const char *filename_str = byte_array_to_string(filename_ba);

    if (!(file = fopen(filename_str, "rb")))
        goto no_file;

    long available;
    if ((available = fsize(file)) < 0)
        goto no_file;
    available -= offset;

    //printf("read_file %s @%d size=%ld available=%ld\n", filename_str, offset, size, available);

    size = size ? MIN(size, available) : available;
    if (size <= 0)
        goto no_file;
    if (size > INPUT_MAX_LEN)
        goto no_file;
    if (!(str = (char*)malloc((size_t)size + 1)))
        goto no_file;
    if (fseek(file, offset, SEEK_SET))
        goto no_file;
    if (fread(str, 1, (size_t)size, file) == EOF)
        goto no_file;
    if (ferror(file))
        goto no_file;
    if (fclose(file))
        goto no_file;

    struct byte_array* ba = byte_array_new_size((uint32_t)size);
    ba->length = (uint32_t)size;
    memcpy(ba->data, str, size);
    //free(filename_str);
    free(str);
    
    return ba;

no_file:
    //free(filename_str);
    DEBUGPRINT("\nCould not read file %s\n", filename_str);
    return NULL;
}

static FILE *fopen2(const char *path) {
//    create_parent_folder_if_needed(path);
    
    // open file
    FILE* file = fopen(path, "r+"); // allows fseek for an existing file
    if (NULL == file)
        file = fopen(path, "w"); // creates the file
    if (NULL == file) {
        perror("fopen");
        return NULL;
    }
    return file;
}

int write_file(const struct byte_array* path, struct byte_array* bytes, uint32_t from, int32_t timestamp) {
    int result = -1;
    const char *path2 = byte_array_to_string(path);

    FILE *file = fopen2(path2);
    if (NULL == file)
        goto done;

    if (fseek(file, from, SEEK_SET)) {
        perror("fseek");
        goto done;
    }
    
    // write bytes
    if (NULL != bytes) {
        int r = (int)fwrite(bytes->data, 1, bytes->length, file);
        int s = fclose(file);
        result = (r<0) || s;
    }
    
done:
    return result;
}

int write_byte_array(struct byte_array* ba, FILE* file) {
    uint16_t len = ba->length;
    int n = (int)fwrite(ba->data, 1, len, file);
    return len - n;
}
