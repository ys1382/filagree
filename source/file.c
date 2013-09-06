#include "struct.h"
#include "util.h"
#include "file.h"
#include "hal.h"
#include "sys.h"

#include <string.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <stdio.h>
#include <dirent.h>
#include <ftw.h>
#include <sys/types.h>
#include <errno.h>
#include <fts.h>
#include <limits.h>


#define INPUT_MAX_LEN    100000


long fsize(FILE* file) {
    if (!fseek(file, 0, SEEK_END)) {
        long size = ftell(file);
        if (size >= 0 && !fseek(file, 0, SEEK_SET))
            return size;
    }
    return -1;
}

struct byte_array *read_file(const struct byte_array *filename_ba)
{
    FILE * file;
    size_t read;
    char *str;
    long size;
    
    char* filename_str = byte_array_to_string(filename_ba);

    if (!(file = fopen(filename_str, "rb"))) {
        free(filename_str);
        DEBUGPRINT("\nCould not read file %s\n", filename_str);
        return NULL;
    }
    free(filename_str);

    if ((size = fsize(file)) < 0)
        exit_message("could not get file size");
    if (size == 0)
        return byte_array_new();
    if (size > INPUT_MAX_LEN)
        exit_message("input file is too big");
    if (!(str = (char*)malloc((size_t)size + 1)))
        exit_message("could not malloc");

    read = fread(str, 1, (size_t)size, file);
    if (feof(file) || ferror(file))
        exit_message("could not read file");
    if (fclose(file))
        exit_message("could not close file");

    struct byte_array* ba = byte_array_new_size(size);
    ba->length = size;
    memcpy(ba->data, str, size);
    free(str);
    return ba;
}

int write_byte_array(struct byte_array* ba, FILE* file) {
    uint16_t len = ba->length;
    int n = fwrite(ba->data, 1, len, file);
    return len - n;
}

int write_file(const struct byte_array* filename, struct byte_array* bytes)
{
    char *fname = byte_array_to_string(filename);
    FILE* file = fopen(fname, "w");
    if (NULL == file) {
        DEBUGPRINT("could not write file %s\n", fname);
        free(fname);
        return -1;
    }
    free(fname);

    int r = fwrite(bytes->data, 1, bytes->length, file);
    DEBUGPRINT("\twrote %d bytes to %s\n", r, fname);
    int s = fclose(file);
    return (r<0) || s;
}

char* build_path(const char* dir, const char* name)
{
    int dirlen = dir ? strlen(dir) : 0;
    char* path = (char*)malloc(dirlen + 1 + strlen(name));
    const char* slash = (dir && dirlen && (dir[dirlen] != '/')) ? "/" : "";
    sprintf(path, "%s%s%s", dir ? dir : "", slash, name);
    return path;
}

int file_list(const char *path, int (*fn)(const char*, bool, long, void*), void *flc)
{
	const char *paths[2];
	FTSENT *cur;
	FTS *ftsp;
	int sverrno, error=0;

    DEBUGPRINT("file_list %s\n", path);
	paths[0] = path;
	paths[1] = NULL;
	ftsp = fts_open((char * const *)paths, FTS_COMFOLLOW | FTS_NOCHDIR, NULL);
	if (NULL == ftsp) {
        printf("cannot fts_open %s: %d\n", path, errno);
		return (-1);
    }
	while (NULL != (cur = fts_read(ftsp))) {
		switch (cur->fts_info) {
            case FTS_DP:
                continue;       // we only visit in preorder
            case FTS_DC:
                errno = ELOOP;
                error = -1;
                goto done;
		}
        bool dir = S_ISDIR(cur->fts_statp->st_mode);
#ifdef __ANDROID__
        long mod = cur->fts_statp->st_atime;
#else
        long mod = cur->fts_statp->st_mtimespec.tv_sec;
#endif
        struct file_list_context *flc2 = (struct file_list_context *)flc;
		error = fn(cur->fts_path, dir, mod, flc2);
		if (error != 0) {
            printf("callback failed\n");
			break;
        }
	}
done:
	sverrno = errno;
	(void) fts_close(ftsp);
	errno = sverrno;
	return (error);
}
