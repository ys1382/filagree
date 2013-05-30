#include "struct.h"
#include "util.h"
#include "hal.h"
#include <string.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fts.h>
#include <ftw.h>
#include <limits.h>

#ifdef ANDROID
#include <android/log.h>
#define TAG "filagree"
#endif

#ifdef MBED

#include "mbed.h"

static Serial usbTxRx(USBTX, USBRX);

#endif // MBED

#if __linux

size_t xstrnlen(char *s, size_t maxlen)
{
	size_t i;
	for (i= 0; i<maxlen && *s; i++, s++);
	return i;
}

char *strnstr(const char *s, const char *find, size_t slen)
{
	char c, sc;
	size_t len;
    
	if ((c = *find++) != '\0') {
		len = strlen(find);
		do {
			do {
				if (slen-- < 1 || (sc = *s++) == '\0')
					return (NULL);
			} while (sc != c);
			if (len > slen)
				return (NULL);
		} while (strncmp(s, find, len) != 0);
		s--;
	}
	return ((char *)s);
}

#endif // __linux


#define MESSAGE_MAX 100

const char *make_message(const char *format, va_list ap)
{
    static char message[MESSAGE_MAX];
    vsnprintf(message, MESSAGE_MAX, format, ap);
    return message;
}

void exit_message2(const char *format, va_list list)
{
    const char *message = make_message(format, list);
    hal_print(message);
    va_end(list);
    exit(1);
}

void assert_message(bool assertion, const char *format, ...)
{
    if (assertion)
        return;
    va_list list;
    va_start(list, format);
    exit_message2(format, list);
}

void *exit_message(const char *format, ...)
{
    va_list list;
    va_start(list, format);
    exit_message2(format, list);
    return NULL;
}

void null_check(const void *pointer) {
    if (pointer == NULL)
        exit_message("null pointer");
}

const char *num_to_string(const struct number_string *ns, int num_items, int num)
{
    for (int i=0; i<num_items; i++) // reverse lookup nonterminal string
        if (num == ns[i].number)
            return ns[i].chars;
    exit_message("num not found");
    return NULL;
}
