#ifndef UTIL_H
#define UTIL_H

#include <stdbool.h>
#include <setjmp.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>

#ifdef __ANDROID__
#include <android/log.h>
#define printf(...) __android_log_print(ANDROID_LOG_DEBUG, "filagree", __VA_ARGS__);
#endif

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

#ifdef __LP64__
#define VOID_INT int64_t
#define VOID_FLT long double
#else
#define VOID_INT int32_t
#define VOID_FLT double)(int32_t
#endif

#define ARRAY_LEN(x) (sizeof x / sizeof *x)
#define ITOA_LEN    19 // enough for 64-bit integer

extern jmp_buf trying;
char *make_message(const char *fmt, va_list ap);
void assert_message(bool assertion, const char *format, ...);
void *exit_message(const char *format, ...);
void null_check(const void* p);

#ifdef DEBUG
#define DEBUGPRINT(...)  printf(__VA_ARGS__);
#define DEBUGSPRINT(...) byte_array_format(context->pcbuf, true, __VA_ARGS__ );
#else
#define DEBUGPRINT(...)  {};
#define DEBUGSPRINT(...) {};
#endif // #ifdef DEBUG


#if !defined MAX
#define MAX(a,b) \
({ __typeof__ (a) __a = (a); \
__typeof__ (b) __b = (b); \
__a > __b ? __a : __b; })
#endif

#if !defined MIN
#define MIN(a,b) \
({ __typeof__ (a) __a = (a); \
__typeof__ (b) __b = (b); \
__a < __b ? __a : __b; })
#endif

#ifndef WAIT_ANY
#define WAIT_ANY -1 // for Android
#endif

struct number_string {
    uint8_t number;
    char* chars;
};

const char *num_to_string(const struct number_string *ns, int num_items, int num);
#define NUM_TO_STRING(ns, num) num_to_string(ns, ARRAY_LEN(ns), num)

// error messages

#define ERROR_ALLOC        "Could not allocate memory"

#endif // UTIL_H
