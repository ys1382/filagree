#include <stdio.h>
#include "javagree.h"
#include "compile.h"

struct variable *c_callback(struct context *context)
{
    struct variable *args = (struct variable*)stack_pop(context->operand_stack);
//    printf("c callback %u\n", args->list->length);
    return NULL;
}

void *variable_map_str2obj(struct context *context, struct variable *v, const char *key)
{
    struct byte_array *key2 = byte_array_from_string(key);
    struct variable *key3 = variable_new_str(context, key2);
    struct variable *key4 = variable_map_get(context, context->singleton->callback, key3);
    return key4->ptr;
}

struct variable *variable_new_map_str(struct context *context, const char *key, ...)
{
    struct variable *v = variable_new_list(context, NULL);

    va_list argp;
    va_start(argp, key);

    for (; key; key = va_arg(argp, const char*))
    {
        void *val = va_arg(argp, void*);
        struct byte_array *key2 = byte_array_from_string(key);
        struct variable *key3 = variable_new_str(context, key2);

        variable_map_insert(context, v, key3, val);
    }

    va_end(argp);

    return v;
}

JNIEXPORT jlong JNICALL Java_javagree_filagreeEval(JNIEnv *j_env,
                                                   jobject j_caller,
                                                   jobject j_callback,
                                                   jobjectArray j_args)
{
    struct context *context = context_new(NULL, true, true);
    struct variable *find = variable_new_map_str(context,
                                                 "j_env",      variable_new_void(context, j_env),
                                                 "j_caller",   variable_new_void(context, j_caller),
                                                 "j_callback", variable_new_void(context, j_callback),
                                                 "c_callback", variable_new_cfnc(context, &c_callback),
                                                 "a", variable_new_str(context, byte_array_from_string("z")),
                                                 NULL);
    context->singleton->callback = find;

    jobject program = (*j_env)->GetObjectArrayElement(j_env, j_args, 0);
    const char *program2 = (*j_env)->GetStringUTFChars(j_env, program, NULL);
    if (NULL == program2)
        return 0;
    struct byte_array *program3 = byte_array_from_string(program2);
    struct byte_array *program4 = build_string(program3);

    execute_with(context, program4);

    return (jlong)context;
}
