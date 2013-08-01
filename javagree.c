#include <stdio.h>
#include "javagree.h"
#include "vm.h"

void *variable_map_str2obj(struct context *context, struct variable *v, const char *key)
{
    struct byte_array *key2 = byte_array_from_string(key);
    struct variable *key3 = variable_new_str(context, key2);
    struct variable *key4 = variable_map_get(context, context->singleton->find, key3);
    return key4->ptr;
}

struct variable *jni_find(context_p context, struct variable *key)
{
//    JNIEnv *env = variable_map_get_str(key, "env");
//    jobject obj = variable_map_get_str(key, "obj");

    // find class or variable
    // find variable

    return NULL;
}

struct jni_ctx {
    JNIEnv  *env;
    jobject caller;
    jobject callback;
};

struct jni_ctx *jni_ctx_new(JNIEnv *env, jobject caller, jobject callback)
{
    struct jni_ctx *j = malloc(sizeof(struct jni_ctx));
    j->env = env;
    j->caller = caller;
    j->callback = callback;
    return j;
}

JNIEXPORT jlong JNICALL Java_javagree_filagree_eval(JNIEnv *env,
                                                 jobject caller,
                                                 jobject callback,
                                                 jobjectArray args)
{
    jobject program = (*env)->GetObjectArrayElement(env, args, 0);
    const char *program2 = (*env)->GetStringUTFChars(env, program, NULL);
    if (NULL == program2)
        return 0;
    struct byte_array *program3 = byte_array_from_string(program2);
    struct jni_ctx *jctx = jni_ctx_new(env, caller, callback);
    struct context *context = execute(program3, (void*)jctx, false);
    return (jlong)context;
}
