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
#include <sys/types.h>
#include <errno.h>
#include <fts.h>
#include <limits.h>
#include <utime.h>


#define INPUT_MAX_LEN    100000


long fsize(FILE* file) {
    if (!fseek(file, 0, SEEK_END)) {
        long size = ftell(file);
        if (size >= 0 && !fseek(file, 0, SEEK_SET))
            return size;
    }
    return -1;
}


struct byte_array *read_file(const struct byte_array *filename_ba, uint32_t offset, long size)
{
    FILE * file;
    char *str;
    const char *filename_str = hal_doc_path(filename_ba);

    if (!(file = fopen(filename_str, "rb")))
        goto no_file;

    long available;
    if ((available = fsize(file)) < 0)
        goto no_file;
    available -= offset;
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

int write_file(const struct byte_array* path, struct byte_array* bytes, uint32_t from, int32_t timestamp)
{
    // open file
    const char *path2 = hal_doc_path(path);
    int result = -1;
    FILE* file = fopen(path2, "r+"); // allows fseek for an existing file
    if (NULL == file)
        file = fopen(path2, "w"); // creates the file
    if (NULL == file) {
        DEBUGPRINT("could not open file %s\n", path2);
        goto done;
    }
    if (fseek(file, from, SEEK_SET))
        goto done;
    
    // write bytes
    if (NULL != bytes)
    {
        int r = (int)fwrite(bytes->data, 1, bytes->length, file);
        DEBUGPRINT("\twrote %d bytes to %s\n", r, path2);
        int s = fclose(file);
        result = (r<0) || s;
    }
    
    // set timestamp
    if (timestamp > 0)
    {
        struct utimbuf timestamp2;
        timestamp2.modtime = timestamp;
        if (utime(path2, &timestamp2))
            DEBUGPRINT("could not set timestamp for file %s\n", path2);
        goto done;
        result = 0;
    }
    
done:
    return result;
}



int write_byte_array(struct byte_array* ba, FILE* file) {
    uint16_t len = ba->length;
    int n = (int)fwrite(ba->data, 1, len, file);
    return len - n;
}

char* build_path(const char* dir, const char* name)
{
    int dirlen = dir ? (int)strlen(dir) : 0;
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


long file_modified(const char *path)
{
    struct stat fst;
    bzero(&fst,sizeof(fst));
    if (stat(path, &fst))
        return -1;
    return fst.st_mtime;
}
