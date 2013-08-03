#include <stdio.h>
#include "javagree.h"
#include "compile.h"

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

JNIEXPORT jlong JNICALL Java_javagree_filagreeEval(JNIEnv *env,
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
    struct byte_array *program4 = build_string(program3);
    struct context *context = execute(program4, NULL, true);
    return (jlong)context;
}
