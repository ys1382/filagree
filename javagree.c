#include <stdio.h>
#include <string.h>
#include "javagree.h"
#include "compile.h"

struct variable *variable_new_j2f(struct context *context, JNIEnv *env, jobject jo);

struct variable *c_callback(struct context *context)
{
    struct variable *args = (struct variable*)stack_pop(context->operand_stack);
    printf("c callback %u\n", args->list->length);
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

// gets jni class name
const char *jni_class_name(JNIEnv *env, jobject jo)
{
    jclass jocls = (*env)->GetObjectClass(env, jo);
    if (NULL == jocls) {
        DEBUGPRINT("cannot GetObjectClass\n");
        return NULL;
    }
    
    jclass clscls = (*env)->FindClass(env, "java/lang/Class");
    if (NULL == clscls) {
        DEBUGPRINT("cannot get Class\n");
        return NULL;
    }
    
    jmethodID getName = (*env)->GetMethodID(env, clscls, "getName", "()Ljava/lang/String;");
    if (NULL == getName) {
        DEBUGPRINT("cannot GetMethodID getName\n");
        return NULL;
    }
    
    jstring name = (*env)->CallObjectMethod(env, jocls, getName);
    if (NULL == name) {
        DEBUGPRINT("cannot call getName\n");
        return NULL;
    }
    
    const char *name2 = (*env)->GetStringUTFChars(env, name, NULL);
    DEBUGPRINT("vj2f %s\n", name2);
    return name2;
}

// jstring -> variable
struct variable *vj2f_str(struct context *context, JNIEnv *jenv, jstring js)
{
    const char *str = (*jenv)->GetStringUTFChars(jenv, js, NULL);
    struct byte_array *str2 = byte_array_from_string(str);
    return variable_new_str(context, str2);
}

// jint -> variable
struct variable *vj2f_int(struct context *context, JNIEnv *jenv, jint ji) {
    return variable_new_int(context, ji);
}

// jobjectArray -> variable
struct variable *variable_new_j2f_array(struct context *context, JNIEnv *env, jobjectArray joa, const char *name)
{
    jint length = (*env)->GetArrayLength(env, joa);
    struct array *list = array_new(length);
    for (jint i=0; i<length; i++) {
        jobject jo = (*env)->GetObjectArrayElement(env, joa, i);
        struct variable *v = variable_new_j2f(context, env, jo);
        array_set(list, i, v);
    }

    return variable_new_list(context, list);
}

// jobject -> variable
struct variable *variable_new_j2f(struct context *context, JNIEnv *env, jobject jo)
{
    const char *name = jni_class_name(env, jo);
    if (NULL == name) {
        DEBUGPRINT("cannot get class name\n");
        return NULL;
    }

    if (name[0] == '[')
        return variable_new_j2f_array(context, env, (jobjectArray)jo, &name[1]);
    if (!strcmp(name, "jint"))
        return vj2f_int(context, env, (jint)jo);
    if (!strcmp(name, "java.lang.String"))
        return vj2f_str(context, env, (jstring)jo);
    exit_message("can't handle it\n");

    return NULL;
}

JNIEXPORT jlong JNICALL Java_javagree_filagreeEval(JNIEnv *env,
                                                   jobject j_caller,
                                                   jobject j_callback,
                                                   jobjectArray j_args)
{
    struct context *context = context_new(NULL, true, true);
    struct variable *find = variable_new_map_str(context,
                                                 "env",        variable_new_void(context, env),
                                                 "j_caller",   variable_new_void(context, j_caller),
                                                 "j_callback", variable_new_void(context, j_callback),
                                                 "c_callback", variable_new_cfnc(context, &c_callback),
                                                 "args",       variable_new_j2f (context, env, j_args),
                                                 "a",          variable_new_str (context,  byte_array_from_string("z")),
                                                 NULL);
    context->singleton->callback = find;

    jobject program = (*env)->GetObjectArrayElement(env, j_args, 0);
    const char *program2 = (*env)->GetStringUTFChars(env, program, NULL);
    if (NULL == program2)
        return 0;
    struct byte_array *program3 = byte_array_from_string(program2);
    struct byte_array *program4 = build_string(program3);

    execute_with(context, program4);

    return (jlong)context;
}
