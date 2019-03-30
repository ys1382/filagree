#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/time.h>

#if defined(_WIN32) || defined(_WIN64)
#include <wait.h>
#endif

#include "interpret.h"
#include "serial.h"
#include "struct.h"
#include "sys.h"
#include "variable.h"
#include "vm.h"
#include "util.h"
#include "node.h"
#include "file.h"
#include <sys/wait.h>

struct string_func {
    const char* name;
    callback2func* func;
};

// system functions

struct variable *sys_print(struct context *context) {
    null_check(context);
    struct variable *args = (struct variable*)stack_pop(context->operand_stack);
    assert_message(args && args->type==VAR_SRC && args->list.ordered, "bad print arg");
    for (int i=1; i<args->list.ordered->length; i++) {
        struct variable *arg = (struct variable*)array_get(args->list.ordered, i);
        struct byte_array *str = variable_value(context, arg);
        if (arg->type == VAR_STR)
            str = byte_array_part(str, 1, str->length-2);
        printf("%s\n", byte_array_to_string(str));
    }
    return NULL;
}

struct variable *sys_save(struct context *context) {
    struct variable *args = (struct variable*)stack_pop(context->operand_stack);
    struct variable *v = param_var(args, 1);
    struct variable *path = param_var(args, 2);
    struct byte_array *bytes = byte_array_new();
    variable_serialize(context, bytes, v);
    int w = write_file(path->str, bytes, 0, -1);
    byte_array_del(bytes);
    return variable_new_int(context, w);
}

struct variable *sys_load(struct context *context) {
    struct variable *value = (struct variable*)stack_pop(context->operand_stack);
    struct variable *path = (struct variable*)array_get(value->list.ordered, 1);
    struct byte_array *file_bytes = read_file(path->str, 0, 0);
    if (NULL == file_bytes) {
        return variable_new_nil(context);
    }
    struct variable *v = variable_deserialize(context, file_bytes);
    byte_array_del(file_bytes);
    return v;
}

// returns contents and modification time of file
struct variable *sys_read(struct context *context) {
    struct variable *args = (struct variable*)stack_pop(context->operand_stack);
    struct variable *path = param_var(args, 1);
    uint32_t offset = param_int(args, 2);
    uint32_t length = param_int(args, 3);

    struct byte_array *bytes = read_file(path->str, offset, length);
    DEBUGPRINT("read %d bytes\n", bytes ? bytes->length : 0);

    struct variable *content = NULL;
    if (NULL != bytes) {
        content = variable_new_str(context, bytes);
        byte_array_del(bytes);
    } else {
        context->error = variable_new_str_chars(context, "could not load file");
        content = variable_new_nil(context);
    }
    return content;
}

struct variable *sys_exit(struct context *context) {
    struct variable *args = (struct variable*)stack_pop(context->operand_stack);
    int32_t ret = param_int(args, 1);
    //printf("\nexit %d\n", ret);
    exit(ret);
    return NULL;
}

struct variable *sys_now(struct context *context) {
    stack_pop(context->operand_stack); // sys

    struct timeval tv;
    struct timezone tzp;
    if (gettimeofday(&tv, &tzp)) {
        perror("gettimeofday");
        return variable_new_int(context, 0);
    }
    return variable_new_int(context, tv.tv_usec);
}

// runs bytecode
struct variable *sys_run(struct context *context) {
    struct variable *value = (struct variable*)stack_pop(context->operand_stack);
    struct variable *script = (struct variable*)array_get(value->list.ordered, 1);
    execute_with(context, script->str, true);
    return NULL;
}

// compiles and runs bytecode
struct variable *sys_interpret(struct context *context) {
    struct variable *value = (struct variable*)stack_pop(context->operand_stack);
    struct variable *script = (struct variable*)array_get(value->list.ordered, 1);
    interpret_string(context, script->str);
    return NULL;
}

// arguments most recently passed into a function
struct variable *sys_args(struct context *context) {
    stack_pop(context->operand_stack); // self
    struct program_state *above = (struct program_state*)stack_peek(context->program_stack, 1);
    struct variable *result = variable_copy(context, above->args);
    result->type = VAR_LST;
    return result;
}

// ascii to integer
struct variable *sys_atoi(struct context *context) {
    struct variable *value = (struct variable*)stack_pop(context->operand_stack);
    char *str = (char*)((struct variable*)array_get(value->list.ordered, 1))->str->data;
    uint32_t offset = value->list.ordered->length > 2 ?
        ((struct variable*)array_get(value->list.ordered, 2))->integer : 0;

    int n=0, i=0;
    bool negative = false;
    if (str[offset] == '-') {
        negative = true;
        i++;
    };

    while (isdigit(str[offset+i])) {
        n = n*10 + str[offset + i++] - '0';
    }
    n *= negative ? -1 : 1;

    variable_push(context, variable_new_int(context, n));
    variable_push(context, variable_new_int(context, i));
    return variable_new_src(context, 2);
}

char *param_str(const struct variable *value, uint32_t index) {
    if (index >= value->list.ordered->length) {
        return NULL;
    }
    const struct variable *strv = (struct variable*)array_get(value->list.ordered, index);
    char *str = NULL;
    switch (strv->type) {
        case VAR_NIL:                                           break;
        case VAR_STR: str = byte_array_to_string(strv->str);    break;
        default:      exit_message("wrong param type");         break;
    }
    return str;
}

int32_t param_int(const struct variable *value, uint32_t index) {
    struct variable *result = param_var(value, index);
    if (NULL == result) {
        return 0;
    }
    switch (result->type) {
        case VAR_INT: return result->integer;
        case VAR_FLT: return (int32_t)result->floater;
        case VAR_NIL: return 0;
        default:
            exit_message("not an int");
            return 0;
    }
}

bool param_bool(const struct variable *value, uint32_t index) {
    struct variable *result = param_var(value, index);
    if (NULL == result) {
        return NULL;
    }
    enum VarType type = result->type;
    switch (type) {
        case VAR_BOOL:  return result->boolean;
        case VAR_INT:   return result->integer;
        case VAR_NIL:   return false;
        case VAR_VOID:  return NULL != result->ptr;
            default:
            return true;
    }
}

struct variable *param_var(const struct variable *value, uint32_t index) {
    if (index >= value->list.ordered->length) {
        return NULL;
    }
    return (struct variable*)array_get(value->list.ordered, index);
}

void *param_void(const struct variable *value, uint32_t index) {
    struct variable *v = param_var(value, index);
    if (NULL == v) {
        return NULL;
    }
    switch (v->type) {
        case VAR_NIL:   return NULL;
        case VAR_VOID:  return v->ptr;
        default:
            return exit_message("expected void or nil");
    }
}

const char *remove_substring(const char *original, const char *toremove) {
    char *s, *copy = malloc(strlen(original)+1);
    strcpy(copy, original);
    while (toremove && (s = strstr(copy, toremove))) {
        char *splice = s + strlen(toremove);
        memmove(copy, splice, 1 + strlen(splice));
    }
    return copy;
}

struct string_func builtin_funcs[] = {
	{"args",        &sys_args},
    {"print",       &sys_print},
    {"atoi",        &sys_atoi},
    {"read",        &sys_read},
    {"save",        &sys_save},
    {"load",        &sys_load},
    {"run",         &sys_run},
    {"interpret",   &sys_interpret},
    {"listen",      &sys_socket_listen},
    {"send",        &sys_send},
    {"connect",     &sys_connect},
    {"disconnect",  &sys_disconnect},
    {"exit",        &sys_exit},
    {"now",         &sys_now}
};

struct variable *sys_new(struct context *context) {
    struct variable *sys = variable_new_list(context, NULL);

    for (int i=0; i<ARRAY_LEN(builtin_funcs); i++) {
        struct variable *key = variable_new_str_chars(context, builtin_funcs[i].name);
        struct variable *value = variable_new_cfnc(context, builtin_funcs[i].func);
        variable_dic_insert(context, sys, key, value);
    }

    return sys;
}

// built-in member functions

#define FNC_STRING      "string"
#define FNC_LIST        "ordered"
#define FNC_TYPE        "type"
#define FNC_LENGTH      "length"
#define FNC_CHAR        "char"
#define FNC_HAS         "has"
#define FNC_KEY         "key"
#define FNC_VAL         "val"
#define FNC_KEYS        "keys"
#define FNC_VALS        "vals"
#define FNC_SERIALIZE   "serialize"
#define FNC_DESERIALIZE "deserialize"
#define FNC_SORT        "sort"
#define FNC_FIND        "find"
#define FNC_REPLACE     "replace"
#define FNC_PART        "part"
#define FNC_REMOVE      "remove"
#define FNC_INSERT      "insert"
#define FNC_PACK        "pack"

int compar(struct context *context, const void *a, const void *b, struct variable *comparator) {
    struct variable *av = *(struct variable**)a;
    struct variable *bv = *(struct variable**)b;

    if (comparator) {

        byte_array_reset(comparator->str);
        vm_call(context, comparator, av, bv, NULL);

        struct variable *result = (struct variable*)stack_pop(context->operand_stack);
        if (result->type == VAR_SRC)
            result = array_get(result->list.ordered, 0);
        assert_message(result->type == VAR_INT, "non-integer comparison result");
        return result->integer;

    } else {

        enum VarType at = av->type;
        enum VarType bt = bv->type;

        if (at == VAR_INT && bt == VAR_INT) {
            // DEBUGPRINT("compare %p:%d to %p:%d : %d\n", av, av->integer, bv, bv->integer, av->integer - bv->integer);
            return av->integer - bv->integer;
        } else
            DEBUGPRINT("can't compare %s to %s\n", var_type_str(at), var_type_str(bt));

        vm_exit_message(context, "incompatible types for comparison");
        return 0;
    }
}

void heapset(size_t width, void *base0, uint32_t index0, void *base1, uint32_t index1) {
    uint8_t *p0 = (uint8_t*)base0 + index0 * width;
    uint8_t *p1 = (uint8_t*)base1 + index1 * width;
    while (width--) {
        *(p0 + width) = *(p1 + width);
    }
}

int heapcmp(struct context *context,
            size_t width,
            void *base0,
            uint32_t index0,
            void *base1,
            uint32_t index1,
            struct variable *comparator) {
    uint8_t *p0 = (uint8_t*)base0 + index0 * width;
    uint8_t *p1 = (uint8_t*)base1 + index1 * width;
    return compar(context, p0, p1, comparator);
}

int heapsortfg(struct context *context, void *base, size_t nel, size_t width, struct variable *comparator) {
    if (!nel) {
        return 0;
    }
    void *t = malloc(width); // the temporary value
    uint32_t n = (uint32_t)nel, parent = (int32_t)nel/2, index, child; // heap indexes
    for (;;) { // loop until array is sorted
        if (parent > 0) { // first stage - Sorting the heap
            heapset(width, t, 0, base, --parent);
        } else { // second stage - Extracting elements in-place
            if (!--n) { // make the heap smaller
                free(t);
                return 0; // When the heap is empty, we are done
            }
            heapset(width, t, 0, base, n);
            heapset(width, base, n, base, 0);
        }
        // insert operation - pushing t down the heap to replace the parent
        index = parent; // start at the parent index
        child = index * 2 + 1; // get its left child index
        while (child < n) {
            if (child + 1 < n // choose the largest child
                    && heapcmp(context, width, base, child+1, base, child, comparator) > 0) {
                child++; // right child exists and is bigger
            }
            // is the largest child larger than the entry?
            if (heapcmp(context, width, base, child, t, 0, comparator) > 0) {
                heapset(width, base, index, base, child);
                index = child; // move index to the child
                child = index * 2 + 1; // get the left child and go around again
            } else
                break; // t's place is found
        }
        // store the temporary value at its new location
        heapset(width, base, index, t, 0);
    }
}

static inline struct variable *cfnc_char(struct context *context) {
    struct variable *args = (struct variable*)stack_pop(context->operand_stack);
    struct variable *from = (struct variable*)array_get(args->list.ordered, 0);
    struct variable *index = (struct variable*)array_get(args->list.ordered, 1);

    assert_message(from->type == VAR_STR, "char from a non-str");
    assert_message(index->type == VAR_INT, "non-int index");
    uint8_t n = byte_array_get(from->str, index->integer);
    return variable_new_int(context, n);
}

static inline struct variable *cfnc_sort(struct context *context) {
    struct variable *args = (struct variable*)stack_pop(context->operand_stack);
    struct variable *self = (struct variable*)array_get(args->list.ordered, 0);

    assert_message(self->type == VAR_LST, "sorting a non-list");
    struct variable *comparator = (args->list.ordered->length > 1)
        ? (struct variable*)array_get(args->list.ordered, 1)
        : NULL;

    int num_items = self->list.ordered->length;

    int success = heapsortfg(context, self->list.ordered->data, num_items, sizeof(struct variable*), comparator);
    assert_message(!success, "error sorting");
    return NULL;
}

static inline struct variable *cfnc_chop(struct context *context, bool snip) {
    int32_t beginning, foraslongas;

    struct variable *args = (struct variable*)stack_pop(context->operand_stack);
    struct variable *self = (struct variable*)array_get(args->list.ordered, 0);

    enum VarType type = self->type;
    if (type == VAR_NIL) {
        return variable_new_nil(context);
    }
    assert_message(self->type == VAR_LST || self->type == VAR_STR, "can't chop");
    
    if (args->list.ordered->length > 1) {
        struct variable *start = (struct variable*)array_get(args->list.ordered, 1);
        assert_message(start->type == VAR_INT, "non-integer index");
        beginning = start->integer;
    } else {
        beginning = 0;
    }

    if (args->list.ordered->length > 2) {
        struct variable *length = (struct variable*)array_get(args->list.ordered, 2);
        assert_message(length->type == VAR_INT, "non-integer length");
        foraslongas = length->integer;
    } else {
        foraslongas = snip ? 1 : self->str->length - beginning;
    }

    struct variable *result = variable_part(context, self, beginning, foraslongas);
    if (snip) {
        variable_remove(self, beginning, foraslongas);
    }
    return result;
}

// returns list of items, indexed by optional start (default 0) and duration (1)
static inline  struct variable *cfnc_part(struct context *context) {
    return cfnc_chop(context, false);
}

// removes and returns list of items, indexed by optional start (default 0) and duration (1)
static inline struct variable *cfnc_remove(struct context *context) {
    return cfnc_chop(context, true);
}

static inline struct variable *cfnc_find2(struct context *context, bool has) {
    struct variable *args = (struct variable*)stack_pop(context->operand_stack);
    struct variable *self = (struct variable*)array_get(args->list.ordered, 0);
    struct variable *sought = (struct variable*)array_get(args->list.ordered, 1);
    struct variable *start = args->list.ordered->length > 2 ? (struct variable*)array_get(args->list.ordered, 2) : NULL;
    null_check(self);
    null_check(sought);

    struct variable *result = variable_find(context, self, sought, start);
    if (has) {
        return variable_new_bool(context, result->integer != -1);
    }
    return result;
}

static inline struct variable *cfnc_find(struct context *context) {
    return cfnc_find2(context, false);
}

static inline struct variable *cfnc_has(struct context *context) {
    return cfnc_find2(context, true);
}

static inline struct variable *cfnc_insert(struct context *context) { // todo: test
    struct variable *args = (struct variable*)stack_pop(context->operand_stack);
    struct variable *self = (struct variable*)array_get(args->list.ordered, 0);
    struct variable *insertion = (struct variable*)array_get(args->list.ordered, 1);
    struct variable *start = args->list.ordered->length > 2
        ? (struct variable*)array_get(args->list.ordered, 2)
        : NULL;
    null_check(self);
    null_check(insertion);
    assert_message(!start || start->type == VAR_INT, "non-integer index");

    int32_t position = 0;
    switch (self->type) {
        case VAR_LST: {
            struct array *list = array_new_size(1);
            array_set(list, 0, insertion);
            insertion = variable_new_list(context, list);
            position = self->list.ordered->length;
            array_del(list);
        } break;
        case VAR_STR:
            assert_message(insertion->type == VAR_STR, "insertion doesn't match destination");
            position = self->str->length;
            break;
        default:
            exit_message("bad insertion destination");
            break;
    }

    struct variable *first = variable_part(context, variable_copy(context, self), 0, position);
    struct variable *second = variable_part(context, variable_copy(context, self), position, -1);
    struct variable *joined = variable_concatenate(context, 3, first, insertion, second);
    
    if (self->type == VAR_LST) {
        array_del(self->list.ordered);
        self->list.ordered = array_copy(joined->list.ordered);
    } else {
        byte_array_del(self->str);
        self->str = byte_array_copy(joined->str);
    } return joined;
}

// VAR_LST -> VAR_SRC
static inline struct variable *cfnc_pack(struct context *context) {
    struct variable *args = (struct variable*)stack_pop(context->operand_stack);
    struct variable *indexable = (struct variable*)array_get(args->list.ordered, 0);
    assert_message(indexable->type == VAR_LST, "wrong type for packing");
    indexable->type = VAR_SRC;
    return indexable;
}

static inline struct variable *cfnc_serialize(struct context *context) {
    struct variable *args = (struct variable*)stack_pop(context->operand_stack);
    struct variable *indexable = (struct variable*)array_get(args->list.ordered, 0);

    struct byte_array *bits = variable_serialize(context, NULL, indexable);
    byte_array_reset(bits);
    struct variable *result = variable_new_str(context, bits);
    byte_array_del(bits);
    return result;
}

static inline struct variable *cfnc_deserialize(struct context *context) {
    struct variable *args = (struct variable*)stack_pop(context->operand_stack);
    struct variable *indexable = (struct variable*)array_get(args->list.ordered, 0);
    struct byte_array *bits = indexable->str;
    //byte_array_reset(bits);
    return variable_deserialize(context, bits);
}

//    a                b        c
// <sought> <replacement> [<start>]
// <start> <length> <replacement>
static inline struct variable *cfnc_replace(struct context *context) {
    struct variable *args = (struct variable*)stack_pop(context->operand_stack);
    struct variable *self = (struct variable*)array_get(args->list.ordered, 0);
    struct variable *a = (struct variable*)array_get(args->list.ordered, 1);
    struct variable *b = (struct variable*)array_get(args->list.ordered, 2);
    struct variable *c = args->list.ordered->length > 3 ?
        (struct variable*)array_get(args->list.ordered, 3) : NULL;

    null_check(self);
    null_check(b);
    assert_message(self->type == VAR_STR, "searching in a non-string");

    struct byte_array *replaced = NULL;

    if (a->type == VAR_STR) { // find a, replace with b

        assert_message(b->type == VAR_STR, "non-string replacement");
        int32_t where = 0;

        if (c) { // replace first match after index c
            assert_message(c->type == VAR_INT, "non-integer index");
            if (((where = byte_array_find(self->str, a->str, c->integer)) >= 0)) {
                replaced = byte_array_replace(self->str, b->str, where, b->str->length);
            }
        } else {
            replaced = byte_array_replace_all(self->str, a->str, b->str);
        }

    } else if (a->type == VAR_INT ) { // replace at index a, length b, insert c

        assert_message(a || a->type == VAR_INT, "non-integer count");
        assert_message(b || b->type == VAR_INT, "non-integer start");
        replaced = byte_array_replace(self->str, c->str, a->integer, b->integer);

    } else exit_message("replacement is not a string");

    null_check(replaced);
    struct variable *result = variable_new_str(context, replaced);
    byte_array_del(replaced);

    return result;
}

struct variable *variable_dic_list(struct context *context,
                                   struct variable *indexable,
                                   struct array* (*dic_list)(const struct dic*)) {
    assert_message(indexable->type == VAR_LST, "values are only for list");
    struct variable *result = variable_new_list(context, NULL);
    if (NULL != indexable->list.dic) {
        struct array *a = dic_list(indexable->list.dic);
        for (int i=0; i<a->length; i++) {
            struct variable *u = variable_copy(context, (struct variable*)array_get(a, i));
            array_add(result->list.ordered, u);
        }
        array_del(a);
    }
    return result;
}

// todo: use a number instead of a string
struct variable *builtin_method(struct context *context,
                                struct variable *indexable,
                                const struct variable *index) {
    enum VarType it = indexable->type;
    char *idxstr = byte_array_to_string(index->str);
    struct variable *result = NULL;

    if (!strcmp(idxstr, FNC_LENGTH)) {
        int n;
        switch (indexable->type) {
            case VAR_LST: n = indexable->list.ordered->length;  break;
            case VAR_STR: n = indexable->str->length;           break;
            case VAR_NIL: n = 0;                                break;
            default:
                free(idxstr);
                exit_message("no length for non-indexable");
                return NULL;
        }
        result = variable_new_int(context, n);
    } else if (!strcmp(idxstr, FNC_TYPE)) {
        const char *typestr = var_type_str(it);
        result = variable_new_str_chars(context, typestr);
    } else if (!strcmp(idxstr, FNC_STRING)) {
        switch (indexable->type) {
            case VAR_STR:
            case VAR_BYT:
            case VAR_FNC:
                result = variable_copy(context, indexable);
                break;
            default: {
                struct byte_array *vv = variable_value(context, indexable);
                result = variable_new_str(context, vv);
                byte_array_del(vv);
                break;
            }
        }
    } else if (!strcmp(idxstr, FNC_LIST)) {
        result = variable_new_list(context, indexable->list.ordered);
    } else if (!strcmp(idxstr, FNC_KEY)) {
        if (indexable->type == VAR_KVP) {
            result = indexable->kvp.key;
        } else {
            result = variable_new_nil(context);
        }
    } else if (!strcmp(idxstr, FNC_VAL)) {
        if (indexable->type == VAR_KVP) {
            result = indexable->kvp.val;
        } else {
            result = variable_new_nil(context);
        }
    } else if (!strcmp(idxstr, FNC_KEYS)) {
        result = variable_dic_list(context, indexable, &dic_keys);
    } else if (!strcmp(idxstr, FNC_VALS)) {
        result = variable_dic_list(context, indexable, &dic_vals);
    } else if (!strcmp(idxstr, FNC_PACK)) {
        result = variable_new_cfnc(context, &cfnc_pack);
    } else if (!strcmp(idxstr, FNC_SERIALIZE)) {
        result = variable_new_cfnc(context, &cfnc_serialize);
    } else if (!strcmp(idxstr, FNC_DESERIALIZE)) {
        result = variable_new_cfnc(context, &cfnc_deserialize);
    } else if (!strcmp(idxstr, FNC_SORT)) {
        assert_message(indexable->type == VAR_LST, "sorting non-list");
        result = variable_new_cfnc(context, &cfnc_sort);
    } else if (!strcmp(idxstr, FNC_CHAR)) {
        result = variable_new_cfnc(context, &cfnc_char);
    } else if (!strcmp(idxstr, FNC_HAS)) {
        result = variable_new_cfnc(context, &cfnc_has);
    } else if (!strcmp(idxstr, FNC_FIND)) {
        result = variable_new_cfnc(context, &cfnc_find);
    } else if (!strcmp(idxstr, FNC_PART)) {
        result = variable_new_cfnc(context, &cfnc_part);
    } else if (!strcmp(idxstr, FNC_REMOVE)) {
        result = variable_new_cfnc(context, &cfnc_remove);
    } else if (!strcmp(idxstr, FNC_INSERT)) {
        result = variable_new_cfnc(context, &cfnc_insert);
    } else if (!strcmp(idxstr, FNC_REPLACE)) {
        result = variable_new_cfnc(context, &cfnc_replace);
    }
    free(idxstr);
    return result;
}
