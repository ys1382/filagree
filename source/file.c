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


long fsize(FILE* file)
{
    if (!fseek(file, 0, SEEK_END))
    {
        long size = ftell(file);
        if (size >= 0 && !fseek(file, 0, SEEK_SET))
            return size;
    }
    return -1;
}

long file_size(const char *path)
{
    FILE * file;
    if (!(file = fopen(path, "rb")))
        return -1;
    return fsize(file);
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

    printf("read_file %s size=%ld available=%ld\n", filename_str, size, available);

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

int file_mkdir(const char *path)
{
    char *mkcmd = malloc(strlen(path) + 20);
    if (NULL == mkcmd)
    {
        perror("malloc");
        return 1;
    }
    sprintf(mkcmd, "mkdir -p %s", path);
    if (system(mkcmd))
    {
        printf("\nCould not mkdir %s\n", path);
        return 2;
    }
    free(mkcmd);
    return 0;
}

int create_parent_folder_if_needed(const char *path)
{
    const char *last_slash = strrchr(path, '/');
    if (NULL == last_slash)
        return 0;
    path = strndup(path, last_slash - path);
    //DEBUGPRINT("parent=%s\n", path);

    struct stat sb;
    int e = stat(path, &sb);
    if (0 == e)
    {
        if (sb.st_mode & S_IFREG) {
            DEBUGPRINT("parent folder of %s exists as a file.\n", path);
            return -1;
        }
    }
    else
    {
        DEBUGPRINT("stat failed.\n");
        if (errno == ENOENT)
        {
            if (file_mkdir(path))
                return -2;
        }
    }
    return 0;
}

static FILE *fopen2(const char *path)
{
    create_parent_folder_if_needed(path);
    
    // open file
    FILE* file = fopen(path, "r+"); // allows fseek for an existing file
    if (NULL == file)
        file = fopen(path, "w"); // creates the file
    if (NULL == file)
    {
        perror("fopen");
        return NULL;
    }
    return file;
}

int write_file(const struct byte_array* path, struct byte_array* bytes, uint32_t from, int32_t timestamp)
{
    int result = -1;
    const char *path2 = hal_doc_path(path);

    FILE *file = fopen2(path2);
    if (NULL == file)
        goto done;

    if (fseek(file, from, SEEK_SET))
    {
        perror("fseek");
        goto done;
    }
    
    // write bytes
    if (NULL != bytes)
    {
        int r = (int)fwrite(bytes->data, 1, bytes->length, file);
        int s = fclose(file);
        result = (r<0) || s;
    }
    
    file_set_timestamp(path2, timestamp);
    
done:
    return result;
}

bool file_set_timestamp(const char *path, long timestamp)
{
    if (timestamp > 0)
    {
        struct utimbuf timestamp2;
        timestamp2.modtime = timestamp;
        if (utime(path, &timestamp2))
        {
            perror("utime");
            printf("could not set timestamp for file %s\n", path);
            return 1;
        }
    }
    return 0;
}


int write_byte_array(struct byte_array* ba, FILE* file)
{
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
	if (NULL == ftsp)
    {
        perror("fts_open");
        DEBUGPRINT("cannot fts_open %s: %d\n", path, errno);
		return (-1);
    }
	while (NULL != (cur = fts_read(ftsp)))
    {
		switch (cur->fts_info)
        {
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
#elif defined(_WIN32) || defined(_WIN64) || defined(__linux)
        long mod = cur->fts_statp->st_mtim.tv_sec;
#else
        long mod = cur->fts_statp->st_mtimespec.tv_sec;
#endif
        struct file_list_context *flc2 = (struct file_list_context *)flc;
		error = fn(cur->fts_path, dir, mod, flc2);
		if (error != 0)
        {
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


long file_timestamp(const char *path)
{
    struct stat fst;
    bzero(&fst,sizeof(fst));
    if (stat(path, &fst))
        return -1;
    return fst.st_mtime;
}
