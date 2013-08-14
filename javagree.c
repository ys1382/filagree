#include <stdio.h>
#include <string.h>
#include "javagree.h"
#include "compile.h"

struct variable *variable_new_j2f(struct context *context, JNIEnv *env, jobject jo);

/*
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

// construct variable with key-value pairs
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
*/

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

// Integer -> variable
struct variable *vj2f_int(struct context *context, JNIEnv *env, jobject ji)
{
    // get class Integer
    jclass jclass_of_integer = (*env)->GetObjectClass(env, ji);
    assert_message(jclass_of_integer, "no class Integer");

    // get method intValue
    jmethodID intValueMethod = (*env)->GetMethodID(env, jclass_of_integer, "intValue", "()I");
    assert_message(intValueMethod, "no method Integer.intValue");

    // get value
    jint intValue = (jint)((*env)->CallObjectMethod(env, ji, intValueMethod));

    // return new variable
    return variable_new_int(context, intValue);
}

// HashMap -> variable
struct variable *vj2f_map(struct context *context, JNIEnv *env, jobject hashmap)
{
    // get class HashMap
    jclass jclass_of_hashmap = (*env)->GetObjectClass(env, hashmap);
    assert_message(jclass_of_hashmap, "no class HashMap");

    // get method HashMap.entrySet
    jmethodID entrySetMethod = (*env)->GetMethodID(env, jclass_of_hashmap, "entrySet", "()Ljava/util/Set;");
    assert_message(entrySetMethod, "no method HashMap.entrySet");

    // get class Set
    jclass jclass_of_set = (*env)->FindClass(env, "java/util/Set"); // Problem during compilation !!!!!
    assert_message(jclass_of_set, "no class Set");

    // get method Set.iterator
    jmethodID iteratorMethod = (*env)->GetMethodID(env, jclass_of_set, "iterator", "()Ljava/util/Iterator;");
    assert_message(iteratorMethod, "no method Set.Iterator");

    // get class Iterator
    jclass jclass_of_iterator = (*env)->FindClass(env, "java/util/Iterator");
    assert_message(jclass_of_iterator, "no class Iterator");

    // get method Iterator.hasNext
    jmethodID hasNextMethod = (*env)->GetMethodID(env, jclass_of_iterator, "hasNext", "()Z");
    assert_message(hasNextMethod, "no method Iterator.hasNext");

    // method Iterator.next

    jmethodID nextMethod = (*env)->GetMethodID(env, jclass_of_iterator, "next", "()Ljava/lang/Object;");
    assert_message(nextMethod, "no method Iterator.next");

    // setup done, now to iterate
    
    struct variable *v = variable_new_list(context, NULL);

    // invoke HashMap.entrySet
    jobject jobject_of_entryset = (*env)->CallObjectMethod(env, hashmap, entrySetMethod);
    if (NULL == jobject_of_entryset)
        return v;
    
    // invoke Set.iterator
    jobject jobject_of_iterator = (*env)->CallObjectMethod(env, jobject_of_entryset, iteratorMethod);
    assert_message(jobject_of_iterator, "no HashMap.Iterator");

    // invoke Iterator.hasNext
    while ((*env)->CallBooleanMethod(env, jobject_of_iterator, hasNextMethod))
    {
        // invoke Iterator.next
        jobject entry = (*env)->CallObjectMethod(env, jobject_of_iterator, nextMethod);

        jclass jclass_of_entry = (*env)->GetObjectClass(env, entry);
        assert_message(jclass_of_entry, "no entry class");
        DEBUGPRINT("entry class is %s\n", jni_class_name(env, entry));

        jmethodID get_key = (*env)->GetMethodID(env, jclass_of_entry, "getKey", "()Ljava/lang/Object;");
        assert_message(get_key, "no method get_key");
        jmethodID get_value = (*env)->GetMethodID(env, jclass_of_entry, "getValue", "()Ljava/lang/Object;");
        assert_message(get_key, "no method get_value");

        jstring key = (jstring)((*env)->CallObjectMethod(env, entry, get_key));
        assert_message(key, "no key");
        jstring val = (jstring)((*env)->CallObjectMethod(env, entry, get_value));

        struct variable *key2 = variable_new_j2f(context, env, key);
        struct variable *val2 = variable_new_j2f(context, env, val);
        
        variable_map_insert(context, v, key2, val2);
    }

    return v;
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

// complex jobject -> variable
struct variable *vj2f_complex(struct context *context, JNIEnv *env, jobject jo)
{
    jclass jocls = (*env)->GetObjectClass(env, jo);
    if (NULL == jocls) {
        DEBUGPRINT("cannot GetObjectClass\n");
        return NULL;
    }

    jmethodID getMethods = env->GetMethodID(jCls, "getMethods", "()[Ljava/lang/reflect/Method;");
    if (NULL == getMethods) {
        DEBUGPRINT("cannot getMethods\n");
        return NULL;
    }

    jmethodID getFields  = env->GetMethodID(jCls, "getFields",  "()[Ljava/lang/reflect/Field;" );
    if (NULL == getFields) {
        DEBUGPRINT("cannot getFields\n");
        return NULL;
    }

    jobjectArray methods = (jobjectArray)env->CallObjectMethod(cls, midGetMethods);
    jobjectArray fields  = (jobjectArray)env->CallObjectMethod(cls, midGetFields );


    // todo: add closures to C functions
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
    if (!strcmp(name, "java.lang.Integer"))
        return vj2f_int(context, env, jo);
    if (!strcmp(name, "java.lang.String"))
        return vj2f_str(context, env, (jstring)jo);
    if (!strcmp(name, "java.util.HashMap"))
        return vj2f_map(context, env, (jobject)jo);

    return vj2f_complex(context, env, (jobject)jo);
}

struct byte_array *byte_array_from_jstring(JNIEnv *env, jstring string)
{
    printf("byte_array_from_jstring %s\n", jni_class_name(env, string));
    const char *string2 = (*env)->GetStringUTFChars(env, string, NULL);
    printf("c\n");
    return byte_array_from_string(string2);
}

struct variable *variable_new_java_find(struct context *context,
                                        JNIEnv *env,
                                        jobject callback,
                                        jstring name)
{
    printf("variable_new_java_find0\n");
    // java callback object name
    struct byte_array *name2 = byte_array_from_jstring(env, name);
    struct variable *name3 = variable_new_str(context, name2);

    printf("a\n");
    // java callback object, converted to filagree object
    struct variable *callback2 = variable_new_j2f(context, env, callback);

    printf("b\n");
    // filagree callback object
    struct variable *find = variable_new_list(context, NULL);
    variable_map_insert(context, find, name3, callback2);

    printf("variable_new_java_find1\n");
    return find;
}

JNIEXPORT jlong JNICALL Java_Javagree_eval(JNIEnv *env,
                                           jobject caller,
                                           jobject callback,
                                           jstring name,
                                           jstring program)
{
    printf("Java_Javagree_eval 0\n");
    // create filagree context
    struct context *context = context_new(NULL, true, true);

    // generate callback object
    struct variable *find = variable_new_java_find(context, env, callback, name);
    context->singleton->callback = find;

    // compile source to bytecode
    struct byte_array *program2 = byte_array_from_jstring(env, program);
    struct byte_array *program3 = build_string(program2);

    // run
    execute_with(context, program3);

    // return context for later use
    printf("Java_Javagree_eval 1\n");
    return (jlong)context;
}
