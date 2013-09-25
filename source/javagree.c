#include <stdio.h>
#include <string.h>
#include "javagree.h"
#include "compile.h"
#include "sys.h"

struct variable *variable_new_j2f(struct context *context, JNIEnv *env, jobject jo);
struct variable *variable_new_j2f_ex(struct context *context, JNIEnv *env, jobject jo, jobject parent);
jobject vf2j(JNIEnv *env, struct variable *f);

// gets jni class name
const char *jni_class_name(JNIEnv *env, jobject jo)
{
    assert_message(NULL != jo, "jni_class_name: null object");

    // get object class
    jclass jocls = (*env)->GetObjectClass(env, jo);
    assert_message(jocls, "cannot GetObjectClass");

    // get Class
    jclass clscls = (*env)->FindClass(env, "java/lang/Class");
    assert_message(clscls, "cannot get Class");

    // get Class.getName
    jmethodID getName = (*env)->GetMethodID(env, clscls, "getName", "()Ljava/lang/String;");
    assert_message(getName, "cannot GetMethodID getName");

    // get name
    jstring name = (*env)->CallObjectMethod(env, jocls, getName);
    assert_message(name, "cannot call getName");
    
    return (*env)->GetStringUTFChars(env, name, NULL);
}

// jstring -> variable
struct variable *vj2f_str(struct context *context, JNIEnv *jenv, jstring js)
{
    const char *str = (*jenv)->GetStringUTFChars(jenv, js, NULL);
    return variable_new_str_chars(context, str);
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
#ifdef __ANDROID__
    jint intValue = (jint)((*env)->CallIntMethod(env, ji, intValueMethod));
#else
    jint intValue = (jint)((*env)->CallObjectMethod(env, ji, intValueMethod));
#endif

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
    jclass jclass_of_set = (*env)->FindClass(env, "java/util/Set");
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

    //// setup done, now to iterate
    
    struct variable *v = variable_new_list(context, NULL);

    // call HashMap.entrySet
    jobject jobject_of_entryset = (*env)->CallObjectMethod(env, hashmap, entrySetMethod);
    if (NULL == jobject_of_entryset)
        return v;
    
    // call Set.iterator
    jobject jobject_of_iterator = (*env)->CallObjectMethod(env, jobject_of_entryset, iteratorMethod);
    assert_message(jobject_of_iterator, "no HashMap.Iterator");

    // call Iterator.hasNext
    while ((*env)->CallBooleanMethod(env, jobject_of_iterator, hasNextMethod))
    {
        // invoke Iterator.next to get entry
        jobject entry = (*env)->CallObjectMethod(env, jobject_of_iterator, nextMethod);

        // get entry's Class
        jclass jclass_of_entry = (*env)->GetObjectClass(env, entry);
        assert_message(jclass_of_entry, "no entry class");
        DEBUGPRINT("entry class is %s\n", jni_class_name(env, entry));

        // get method getKey
        jmethodID get_key = (*env)->GetMethodID(env, jclass_of_entry, "getKey", "()Ljava/lang/Object;");
        assert_message(get_key, "no method getKey");

        // get method getValue
        jmethodID get_value = (*env)->GetMethodID(env, jclass_of_entry, "getValue", "()Ljava/lang/Object;");
        assert_message(get_key, "no method getValue");

        // get key
        jstring key = (jstring)((*env)->CallObjectMethod(env, entry, get_key));
        assert_message(key, "no key");

        // get value
        jstring val = (jstring)((*env)->CallObjectMethod(env, entry, get_value));
        assert_message(val, "no val");

        // translate Java -> filagree
        struct variable *key2 = variable_new_j2f(context, env, key);
        struct variable *val2 = variable_new_j2f(context, env, val);

        // put in result
        variable_map_insert(context, v, key2, val2);
    }

    return v;
}

// jobjectArray -> variable
struct variable *variable_new_j2f_array(struct context *context,
                                        JNIEnv *env,
                                        jobjectArray joa,
                                        const char *name)
{
    jint length = (*env)->GetArrayLength(env, joa);
    struct array *list = array_new(length);

    for (jint i=0; i<length; i++)
    {
        // get object from JNI array
        jobject jo = (*env)->GetObjectArrayElement(env, joa, i);

        // translate Java -> filagree
        struct variable *v = variable_new_j2f(context, env, jo);

        // store result
        array_set(list, i, v);
    }

    return variable_new_list(context, list);
}

// bespoke Java class jobject -> filagree variable
struct variable *vj2f_bespoke(struct context *context, JNIEnv *env, jobject jo)
{
    // get object class
    jclass jocls = (*env)->GetObjectClass(env, jo);
    assert_message(jocls, "cannot GetObjectClass");

    // get Class
    jclass clscls = (*env)->FindClass(env, "java/lang/Class");
    assert_message(clscls, "cannot get Class");

    // get method getMethods
    jmethodID getMethods = (*env)->GetMethodID(env, clscls, "getMethods", "()[Ljava/lang/reflect/Method;");
    assert_message(getMethods, "cannot getMethods");

    // get method getFields
    jmethodID getFields  = (*env)->GetMethodID(env, clscls, "getFields",  "()[Ljava/lang/reflect/Field;" );
    assert_message(getFields, "cannot getFields");
    
    // get object's class' methods
    jobjectArray methods = (jobjectArray)(*env)->CallObjectMethod(env, jocls, getMethods);
    jint numMethods = (*env)->GetArrayLength(env, methods);

    // get object's class' fields
    jobjectArray fields  = (jobjectArray)(*env)->CallObjectMethod(env, jocls, getFields );
    jint numFields = (*env)->GetArrayLength(env, fields);

    struct variable *result = variable_new_list(context, NULL);

    // convert methods to filagree c function variables
    for (int i=0; i<numMethods; i++)
    {
        jobject m = (*env)->GetObjectArrayElement(env, methods, i);
        struct variable *v = variable_new_j2f_ex(context, env, m, jo);
        variable_map_insert(context, result, v->kvp.key, v->kvp.val);
    }

    // convert fields to filagree list variables
    for (int i=0; i<numFields; i++)
    {
        jobject f = (*env)->GetObjectArrayElement(env, fields, i);
        struct variable *v = variable_new_j2f_ex(context, env, f, jo);
        variable_map_insert(context, result, v->kvp.key, v->kvp.val);
    }

    //char buf[1000];
    //DEBUGPRINT("bespoke: %s\n", variable_value_str(context, result, buf));
    return result;
}

// filagree variable -> Java Object Array
jobject vf2j_lst(JNIEnv *env, struct variable *f)
{
    uint32_t length = f->list->length;
    jclass ocls = (*env)->FindClass(env, "java/lang/Object");
    jobjectArray result = (jobjectArray)(*env)->NewObjectArray(env, length, ocls, NULL);

    for (int i=0; i<length; i++)
    {
        struct variable *v = (struct variable*)array_get(f->list, i);
        jobject jo = vf2j(env, v);
        (*env)->SetObjectArrayElement(env, result, i, jo);
    }  

    return result;
}

// filagree variable -> Java HashMap
jobject vf2j_map(JNIEnv *env, struct variable *f)
{
    jclass hmcls = (*env)->FindClass(env, "java/util/HashMap");
    assert_message(hmcls, "cannot get class HashMap");    

    struct array *keys = map_keys(f->map);
    uint32_t length = keys->length;
    struct array *vals = map_vals(f->map);

    jmethodID init = (*env)->GetMethodID(env, hmcls, "<init>", "(I)V");
    jobject result = (*env)->NewObject(env, hmcls, init, length);

    jmethodID put = (*env)->GetMethodID(env, hmcls, "put", "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");

    for (int i=0; i<length; i++)
    {
        struct variable *fkey = (struct variable *)array_get(keys, i);
        jobject jkey = vf2j(env, fkey);

        struct variable *fval = (struct variable *)array_get(vals, i);
        jobject jval = vf2j(env, fval);

        (*env)->CallObjectMethod(env, result, put, jkey, jval);
    }

    return result;
}

// filagree variable -> Java Integer
jobject vf2j_int(JNIEnv *env, struct variable *f)
{
    // get class Integer
    jclass intcls = (*env)->FindClass(env, "java/lang/Integer");
    assert_message(intcls, "can't get class Integer");

    // get Integer constructor
    jmethodID constructor = (*env)->GetMethodID(env, intcls, "<init>", "(I)V");
    assert_message(constructor, "can't get Integer constructor");

    // call constructor
    return (*env)->NewObject(env, intcls, constructor, f->integer);
}

jobject vf2j_str(JNIEnv *env, struct variable *f)
{
    const char *str2 = byte_array_to_string(f->str);
    return (*env)->NewStringUTF(env, str2);
}

jbyteArray vf2j_fnc(JNIEnv *env, struct variable *f)
{
    jbyteArray result = (*env)->NewByteArray(env, f->str->size);
    (*env)->SetByteArrayRegion(env, result, 0, f->str->size, (jbyte*)f->str->data);
    return result;
}

// filagree variable -> Java Object
jobject vf2j(JNIEnv *env, struct variable *f)
{
    switch (f->type)
    {
        case VAR_NIL:   return NULL;
        case VAR_INT:   return vf2j_int(env, f);
        case VAR_FNC:   return vf2j_fnc(env, f);
        case VAR_STR:   return vf2j_str(env, f);
        case VAR_SRC:
        case VAR_LST:   return f->map ? vf2j_map(env, f) : vf2j_lst(env, f);
        default:
            DEBUGPRINT("type = %d\n", f->type);
            exit_message("vf2j type not implemented yet");
            return NULL;
    }
}

// JNI Method invoke
struct variable *invoke(struct context *context,
                        JNIEnv *env,
                        jobject this,
                        jobject method,
                        jvalue *args)
{
    // get method class
    jclass m_cls = (*env)->GetObjectClass(env, method);
    assert_message(m_cls, "cannot Method.GetObjectClass");

    // get MethodID invoke
    const char *param_list = "(Ljava/lang/Object;[Ljava/lang/Object;)Ljava/lang/Object;";
    jmethodID invoke = (*env)->GetMethodID(env, m_cls, "invoke", param_list);
    invoke = (*env)->FromReflectedMethod (env, method);
    assert_message(invoke, "cannot GetMethodID Method.invoke");

    //DEBUGPRINT("CallObjectMethodA %p %p\n", this, args[0].l);
    
    // invoke invoke
    jobject result = (*env)->CallObjectMethodA(env, this, invoke, args);
    if (!result)
        printf("cannot call Method.invoke. Oh, the irony!");

    // translate result from Java to filagree
    struct variable *ret = variable_new_j2f(context, env, result);
    if (ret->type == VAR_LST)
        ret->type = VAR_SRC;
    return ret;
}

struct method_closure {
    JNIEnv *env;
    jobject this;
    jobject method;
};

struct method_closure *method_closure_new(JNIEnv *env, jobject method, jobject this)
{
    assert_message(env, "env");
    assert_message(method, "method");
    assert_message(this, "this");
    struct method_closure *mc = malloc(sizeof(struct method_closure));
    mc->env = env;
    mc->this = this;
    mc->method = method;
    return mc;
}

// Callback from filagree. Invokes requested Java method. Returns Java response.
struct variable *cgree(struct context *context)
{
    struct variable *arguments = (struct variable*)stack_pop(context->operand_stack);

    // get method
    struct variable *closure = (struct variable*)array_get(arguments->list, 1);
    assert_message(closure->type == VAR_VOID, "wrong closure type");
    struct method_closure *mc = (struct method_closure *)closure->ptr;
    JNIEnv *env = mc->env;
    jobject this = mc->this;
    jobject method = mc->method;

    // build argument list
    struct variable *v0 = (struct variable *)array_get(arguments->list, 0);
    struct variable *v1 = (struct variable *)array_get(arguments->list, 1);
    printf("remove args %d, %d\n", v0->type, v1->type);
    array_remove(arguments->list, 0, 2); // don't pass "this" or closure in args

    jobjectArray jargs = vf2j(env, arguments);
    int length = (*env)->GetArrayLength (env, jargs);
    jvalue *jargs2 = (jvalue *)malloc (sizeof (jvalue) * length);
    for (int i = 0; i < length; i ++)
    {
        jargs2[i].l = (*env)->GetObjectArrayElement (env, jargs, i);
    }

    // call method
    return invoke(context, env, this, method, jargs2);
}

struct byte_array *byte_array_from_jstring(JNIEnv *env, jstring string)
{
    const char *string2 = (*env)->GetStringUTFChars(env, string, NULL);
    return byte_array_from_string(string2);
}

struct variable *jobject_name(struct context *context, JNIEnv *env, jobject jo)
{
    // get object class
    jclass jocls = (*env)->GetObjectClass(env, jo);
    assert_message(jocls, "cannot GetObjectClass");

    // get method getName
    jmethodID getName = (*env)->GetMethodID(env, jocls, "getName", "()Ljava/lang/String;");
    assert_message(getName, "cannot GetMethodID Method.getName");

    // call getName
    jstring name = (*env)->CallObjectMethod(env, jo, getName);
    assert_message(name, "cannot call getName");

    struct byte_array *name2 = byte_array_from_jstring(env, name);
    //DEBUGPRINT("jobject_name %s\n", byte_array_to_string(name2));
    return variable_new_str(context, name2);
}

struct variable *vj2f_fnc(struct context *context, JNIEnv *env, jobject method, jobject this)
{
    //DEBUGPRINT("method %p . %p\n", this, method);

    // get Method name
    struct variable *key = jobject_name(context, env, method);

    // get c callback, store Method in its closure
    struct variable *val = variable_new_cfnc(context, &cgree);
    struct method_closure *mc = method_closure_new(env, method, this);
    val->closure = variable_new_void(context, mc);
    //DEBUGPRINT("vj2f_fnc: val=%p clo=%p ptr=%p mc=%p mthod=%p\n",
    //           val, val->closure, val->closure->ptr, mc, mc->method);

    // construct filagree key-value pair
    struct variable *kvp = variable_new_kvp(context, key, val);
    return kvp;
}

struct variable *vj2f_fld(struct context *context, JNIEnv *env, jobject fld, jobject parent)
{
    // get Field Class
    jclass fldcls = (*env)->GetObjectClass(env, fld);
    assert_message(fldcls, "cannot GetObjectClass");

    // get Method Field.get
    jmethodID get = (*env)->GetMethodID(env, fldcls, "get", "(Ljava/lang/Object;)Ljava/lang/Object;");
    assert_message(get, "cannot GetMethodID Field.get");

    // get value
    jobject value = (*env)->CallObjectMethod(env, fld, get, parent);
    assert_message(value, "cannot call get");

    // get field name
    struct variable *key = jobject_name(context, env, fld);

    // get field value
    struct variable *val = variable_new_j2f(context, env, value);

    // construct filagree key-value pair
    return variable_new_kvp(context, key, val);
}

// jobject -> variable
struct variable *variable_new_j2f_ex(struct context *context, JNIEnv *env, jobject jo, jobject parent)
{
    //DEBUGPRINT("variable_new_j2f_ex\n");
    if (NULL == jo)
        return variable_new_nil(context);

    const char *name = jni_class_name(env, jo);
    assert_message(name, "cannot get class name");
    //DEBUGPRINT("variable_new_j2f: %s\n", name);

    if (name[0] == '[')
        return variable_new_j2f_array(context, env, (jobjectArray)jo, &name[1]);
    if (!strcmp(name, "java.lang.Integer"))
        return vj2f_int(context, env, jo);
    if (!strcmp(name, "java.lang.String"))
        return vj2f_str(context, env, (jstring)jo);
    if (!strcmp(name, "java.util.HashMap"))
        return vj2f_map(context, env, jo);
    if (!strcmp(name, "java.lang.reflect.Method"))
        return vj2f_fnc(context, env, jo, parent);
    if (!strcmp(name, "java.lang.reflect.Field"))
        return vj2f_fld(context, env, jo, parent);

    //DEBUGPRINT("vj2f_bespoke %s\n", name);
    return vj2f_bespoke(context, env, jo);
}

struct variable *variable_new_j2f(struct context *context, JNIEnv *env, jobject jo) {
    return variable_new_j2f_ex(context, env, jo, NULL);
}

struct variable *variable_new_java_find(struct context *context,
                                        JNIEnv  *env,
                                        jobject callback,
                                        jstring name,
                                        jobject sys)
{
    // java callback object name
    struct byte_array *name2 = byte_array_from_jstring(env, name);
    struct variable *name3 = variable_new_str(context, name2);

    // java callback object, translated to filagree object
    struct variable *callback2 = variable_new_j2f(context, env, callback);

    // sys object
    if (NULL != sys)
    {
        struct variable *javaSys = variable_new_j2f(context, env, sys);
        map_union(javaSys->map, context->sys->map);
        context->sys = javaSys;
    }

    // filagree callback object
    struct variable *find = variable_new_list(context, NULL);
    variable_map_insert(context, find, name3, callback2);
    return find;
}

#ifdef __ANDROID__
JNIEXPORT jint JNICALL Java_com_java_javagree_Javagree_evalSource(
          JNIEnv *env,
          jobject caller,
          jobject callback,
          jstring name,
          jstring program,
          jobject sys)
#else
JNIEXPORT jint JNICALL Java_Javagree_evalSource
        (JNIEnv *env,
         jobject caller,
         jobject callback,
         jstring name,
         jstring program,
         jobject sys)
#endif
{
    // create filagree context
    struct context *context = context_new(NULL, true, true);

    // generate callback object
    struct variable *find = variable_new_java_find(context, env, callback, name, sys);
    context->singleton->callback = find;

    // compile source to bytecode
    struct byte_array *program2 = byte_array_from_jstring(env, program);
    struct byte_array *program3 = build_string(program2);

    // run
    execute_with(context, program3, false);

    DEBUGPRINT("eval done\n");
    // return context for later use
    return (jint)context;
}

#ifdef __ANDROID__
JNIEXPORT jint JNICALL Java_com_java_javagree_Javagree_evalBytes(
        JNIEnv *env,
        jobject caller,
        jobject callback,
        jstring name,
        jbyteArray program,
        jobject sys)
#else
JNIEXPORT jint JNICALL Java_Javagree_evalBytes

        (JNIEnv *env,
        jobject caller,
        jobject callback,
        jstring name,
        jbyteArray program,
        jobject sys)
#endif
{
    // create filagree context
    struct context *context = context_new(NULL, true, true);
    
    // generate callback object
    struct variable *find = variable_new_java_find(context, env, callback, name, sys);
    context->singleton->callback = find;

    jsize size = (*env)->GetArrayLength(env, program);
    jbyte *program2 = (jbyte *)(*env)->GetByteArrayElements(env, program, NULL);
    struct byte_array *program3 = byte_array_new_data(size, (uint8_t*)program2);

    DEBUGPRINT("evalBytes %d\n", size);
    // run
    execute_with(context, program3, false);
    
    DEBUGPRINT("eval done\n");

    // return context for later use
    return (jint)context;
}
