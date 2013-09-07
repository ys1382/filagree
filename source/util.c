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
#include <limits.h>

#ifdef ANDROID
#include <android/log.h>
#define TAG "filagree"
#endif


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
    if (NULL == pointer)
        exit_message(ERROR_NULL);
}

const char *num_to_string(const struct number_string *ns, int num_items, int num)
{
    for (int i=0; i<num_items; i++) // reverse lookup nonterminal string
        if (num == ns[i].number)
            return ns[i].chars;
    exit_message("num not found");
    return NULL;
}

static struct number_string hal_events[] = {
    {FILED,         "filed"},
    {CONNECTED,     "connected"},
    {DISCONNECTED,  "disconnected"},
    {MESSAGED,      "messaged"},
    {SENT,          "sent"},
    {ERROR,         "error"},
};

struct byte_array *event_string(enum HAL_Event event)
{
    const char *str = NUM_TO_STRING(hal_events, event);
    return byte_array_from_string(str);
}
