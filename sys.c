#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#include "hal.h"
#include "interpret.h"
#include "serial.h"
#include "struct.h"
#include "sys.h"
#include "variable.h"
#include "vm.h"
#include "util.h"
#include "node.h"

#define RESERVED_SYS  "sys"

struct string_func
{
    const char* name;
    callback2func* func;
};

// system functions

struct variable *sys_print(struct context *context)
{
    null_check(context);
    struct variable *args = (struct variable*)stack_pop(context->operand_stack);
    assert_message(args && args->type==VAR_SRC && args->list, "bad print arg");
    for (int i=1; i<args->list->length; i++) {
        struct variable *arg = (struct variable*)array_get(args->list, i);
        char buf[VV_SIZE];
        printf("%s\n", variable_value_str(context, arg, buf));
    }
    return NULL;
}

struct variable *sys_save(struct context *context)
{
    struct variable *value = (struct variable*)stack_pop(context->operand_stack);
    struct variable *v = (struct variable*)array_get(value->list, 1);
    struct variable *path = (struct variable*)array_get(value->list, 2);
    struct byte_array *bytes = byte_array_new();
    variable_serialize(context, bytes, v, true);
    int w = write_file(path->str, bytes);
    byte_array_del(bytes);
    return variable_new_int(context, w);
}

struct variable *sys_load(struct context *context)
{
    struct variable *value = (struct variable*)stack_pop(context->operand_stack);
    struct variable *path = (struct variable*)array_get(value->list, 1);
    struct byte_array *file_bytes = read_file(path->str);
    if (file_bytes == NULL)
        return NULL;
    struct variable *v = variable_deserialize(context, file_bytes);
    byte_array_del(file_bytes);
    return v;
}

struct variable *sys_write(struct context *context)
{
    struct variable *value = (struct variable*)stack_pop(context->operand_stack);
    struct variable *v = (struct variable*)array_get(value->list, 1);
    struct variable *path = (struct variable*)array_get(value->list, 2);

    struct byte_array *bytes = byte_array_new();
    variable_serialize(context, bytes, v, true);
    int w = write_file(path->str, bytes);
    return variable_new_int(context, w);
}

struct variable *sys_read(struct context *context)
{
    struct variable *value = (struct variable*)stack_pop(context->operand_stack);
    struct variable *path = (struct variable*)array_get(value->list, 1);
    struct byte_array *bytes = read_file(path->str);
    struct variable *result = NULL;
    if (bytes != NULL)
        result = variable_new_str(context, bytes);
    else {
        bytes = byte_array_from_string("could not load file");
        context->vm_exception = variable_new_str(context, bytes);
    }
    byte_array_del(bytes);
    return result;
}

struct variable *sys_run(struct context *context)
{
    struct variable *value = (struct variable*)stack_pop(context->operand_stack);
    struct variable *script = (struct variable*)array_get(value->list, 1);
    execute(script->str, NULL);
    return NULL;
}

struct variable *sys_interpret(struct context *context)
{
    stack_pop(context->operand_stack); // self
    struct variable *script = (struct variable*)stack_pop(context->operand_stack);
    char *str = byte_array_to_string(script->str);
    interpret_string(str, NULL);
    return NULL;
}

struct variable *sys_rm(struct context *context)
{
    struct variable *value = (struct variable*)stack_pop(context->operand_stack);
    struct variable *path = (struct variable*)array_get(value->list, 1);
    char *str = byte_array_to_string(path->str);
    remove(str);
    free(str);
    return NULL;
}

struct variable *sys_args(struct context *context)
{
    stack_pop(context->operand_stack); // self
    struct program_state *above = (struct program_state*)stack_peek(context->program_stack, 1);
    return variable_new_list(context, above->args);
}

struct variable *sys_bytes(struct context *context)
{
    struct variable *value = (struct variable*)stack_pop(context->operand_stack);
    struct variable *arg;
    int32_t n = 0;
    if (value->list->length > 1) {
        arg = (struct variable*)array_get(value->list, 1);
        assert_message(arg->type == VAR_INT, "bad bytes size type");
        n = arg->integer;
    }
    return variable_new_bytes(context, NULL, n);
}

struct variable *sys_atoi(struct context *context)
{
    struct variable *value = (struct variable*)stack_pop(context->operand_stack);
    char *str = (char*)((struct variable*)array_get(value->list, 1))->str->data;
    uint32_t offset = value->list->length > 2 ? ((struct variable*)array_get(value->list, 2))->integer : 0;

    int n=0, i=0;
    bool negative = false;
    if (str[offset] == '-') {
        negative = true;
        i++;
    };

    while (isdigit(str[offset+i]))
        n = n*10 + str[offset + i++] - '0';
    n *= negative ? -1 : 1;

    variable_push(context, variable_new_int(context, n));
    variable_push(context, variable_new_int(context, i));
    return variable_new_src(context, 2);
}

struct variable *sys_sin(struct context *context) // radians
{
    struct variable *arguments = (struct variable*)stack_pop(context->operand_stack);
    const int32_t n = ((struct variable*)array_get(arguments->list, 1))->integer;
    double s = sin(n);
    return variable_new_float(context, s);
}

const char *param_str(const struct variable *value, uint32_t index)
{
    if (index >= value->list->length)
        return NULL;
    const struct variable *strv = (struct variable*)array_get(value->list, index);
    assert_message(strv->type == VAR_STR, "param is not string");
    const struct byte_array *strb = strv->str;
    const char *str = byte_array_to_string(strb);
    return str;
}

int32_t param_int(const struct variable *value, uint32_t index) {
    if (index >= value->list->length)
        return 0;
    return ((struct variable*)array_get(value->list, index))->integer;
}

#ifndef NO_UI

static struct variable *param_var(const struct variable *value, uint32_t index) {
    if (index >= value->list->length)
        return NULL;
    return (struct variable*)array_get(value->list, index);
}

struct variable *two_ints(struct context *context, int32_t w, int32_t h)
{
    variable_push(context, variable_new_int(context, w));
    variable_push(context, variable_new_int(context, h));
    return variable_new_src(context, 2);
}

struct variable *sys_label(struct context *context)
{
    struct variable *value = (struct variable*)stack_pop(context->operand_stack);
    int32_t x = param_int(value, 1);
    int32_t y = param_int(value, 2);
    const char *str = param_str(value, 3);

    int32_t w=0,h=0;
    hal_label(x, y, &w, &h, str);
    return two_ints(context, w, h);
}

struct variable *sys_input(struct context *context)
{
    struct variable *value = (struct variable*)stack_pop(context->operand_stack);
    struct variable *uictx = (struct variable*)array_get(value->list, 1);
    int32_t x = param_int(value, 2);
    int32_t y = param_int(value, 3);
    const char *str = param_str(value, 4);

    int32_t w=0, h=0;
    hal_input(uictx, x, y, &w, &h, str, false);
    return two_ints(context, w, h);
}

const char *variable_keyed_string(struct context *context, struct variable *v, const char *key)
{
    if ((v == NULL) || (v->map == NULL))
        return NULL;
    struct byte_array *b = byte_array_from_string(key);
    struct variable *b2 = variable_new_str(context, b);
    struct variable *u = variable_map_get(context, v, b2);
    byte_array_del(b);
    if ((u == NULL) || (u->type != VAR_STR))
        return NULL;
    return byte_array_to_string(u->str);
}

struct variable *sys_button(struct context *context)
{
    struct variable *value = (struct variable*)stack_pop(context->operand_stack);
    struct variable *uictx = (struct variable*)array_get(value->list, 1);
    int32_t x = param_int(value, 2);
    int32_t y = param_int(value, 3);
    int32_t w = param_int(value, 4);
    int32_t h = param_int(value, 5);
    struct variable *item = (struct variable*)array_get(value->list, 6);
    struct byte_array *logic = byte_array_from_string("logic");
    struct variable *logic2 = variable_new_str(context, logic);

    hal_button(context, uictx, x, y, &w, &h,
               variable_map_get(context, item, logic2),
               variable_keyed_string(context, item, "text"),
               variable_keyed_string(context, item, "image"));
    
    byte_array_del(logic);
    return two_ints(context, w, h);
}

struct variable *sys_table(struct context *context)
{
    struct variable *value = (struct variable*)stack_pop(context->operand_stack);
    struct variable *uictx = (struct variable*)array_get(value->list, 1);
    int32_t x = param_int(value, 2);
    int32_t y = param_int(value, 3);
    int32_t w = param_int(value, 4);
    int32_t h = param_int(value, 5);

    struct byte_array *balist = byte_array_from_string("list");
    struct variable *balist2 = variable_new_str(context, balist);
    struct byte_array *balogic = byte_array_from_string("logic");
    struct variable *balogic2 = variable_new_str(context, balogic);

    struct variable *item = (struct variable*)array_get(value->list, 6);
    struct variable *list = variable_map_get(context, item, balist2);
    struct variable *logic = variable_map_get(context, item, balogic2);

    hal_table(context, uictx, x, y, w, h, list, logic);

    byte_array_del(balist);
    byte_array_del(balogic);
    return NULL;
}

struct variable *sys_graphics(struct context *context)
{
    const struct variable *value = (const struct variable*)stack_pop(context->operand_stack);
    const struct variable *shape = (const struct variable*)array_get(value->list, 1);
    hal_graphics(shape);
    return NULL;
}

struct variable *sys_synth(struct context *context)
{
    struct variable *arguments = (struct variable*)stack_pop(context->operand_stack);
    const struct byte_array *bytes = ((struct variable*)array_get(arguments->list, 1))->str;
    hal_synth(bytes->data, bytes->length);
    return NULL;
}

struct variable *sys_sound(struct context *context)
{
    struct variable *arguments = (struct variable*)stack_pop(context->operand_stack);
    const struct byte_array *url = ((struct variable*)array_get(arguments->list, 1))->str;
    hal_sound((const char*)url->data);
    return NULL;
}

struct variable *sys_window(struct context *context)
{
    struct variable *value = (struct variable*)stack_pop(context->operand_stack);
    int w=0, h=0;
    if (value->list->length > 2) {
        w = param_int(value, 1);
        h = param_int(value, 2);
    }

    struct variable *uictx = param_var(value, 2);
    struct variable *logic = param_var(value, 3);
    
    hal_window(context, uictx, &w, &h, logic);
    return two_ints(context, w, h);
}

struct variable *sys_load_form(struct context *context)
{
    struct variable *value = (struct variable*)stack_pop(context->operand_stack);
    const struct byte_array *key = ((struct variable*)array_get(value->list, 1))->str;
    hal_load_form(context, key);
    return NULL;
}

struct variable *sys_save_form(struct context *context)
{
    struct variable *value = (struct variable*)stack_pop(context->operand_stack);
    const struct byte_array *key = ((struct variable*)array_get(value->list, 1))->str;
    hal_save_form(context, key);
    return NULL;
}

struct variable *sys_loop(struct context *context)
{
    stack_pop(context->operand_stack); // self
    hal_loop();
    return NULL;
}

#endif // NO_UI

struct string_func builtin_funcs[] = {
	{"args",        &sys_args},
    {"print",       &sys_print},
    {"atoi",        &sys_atoi},
    {"read",        &sys_read},
    {"write",       &sys_write},
    {"save",        &sys_save},
    {"load",        &sys_load},
    {"remove",      &sys_rm},
    {"bytes",       &sys_bytes},
    {"sin",         &sys_sin},
    {"run",         &sys_run},
    {"interpret",   &sys_interpret},
    {"listen",      &sys_listen},
    {"send",        &sys_send},
    {"connect",     &sys_connect},
    {"disconnect",  &sys_disconnect},
#ifndef NO_UI
    {"window",      &sys_window},
    {"load_form",   &sys_load_form},
    {"save_form",   &sys_save_form},
    {"loop",        &sys_loop},
    {"label",       &sys_label},
    {"button",      &sys_button},
    {"input",       &sys_input},
    {"synth",       &sys_synth},
    {"sound",       &sys_sound},
    {"table",       &sys_table},
    {"graphics",    &sys_graphics},
#endif // NO_UI
};

struct variable *sys_new(struct context *context)
{
    struct map *sys_func_map = map_new(context);
    for (int i=0; i<ARRAY_LEN(builtin_funcs); i++) {
        struct byte_array *name = byte_array_from_string(builtin_funcs[i].name);
        struct variable *key = variable_new_str(context, name);
        byte_array_del(name);
        struct variable *value = variable_new_c(context, builtin_funcs[i].func);
        map_insert(sys_func_map, key, value);
    }
    return variable_new_map(context, sys_func_map);
}

// built-in member functions

#define FNC_STRING      "string"
#define FNC_LIST        "list"
#define FNC_TYPE        "type"
#define FNC_LENGTH      "length"
#define FNC_CHAR        "char"
#define FNC_HAS         "has"
#define FNC_KEYS        "keys"
#define FNC_VALUES      "values"
#define FNC_SERIALIZE   "serialize"
#define FNC_DESERIALIZE "deserialize"
#define FNC_SORT        "sort"
#define FNC_FIND        "find"
#define FNC_REPLACE     "replace"
#define FNC_PART        "part"
#define FNC_REMOVE      "remove"
#define FNC_INSERT      "insert"


int compar(struct context *context, const void *a, const void *b, struct variable *comparator)
{
    struct variable *av = *(struct variable**)a;
    struct variable *bv = *(struct variable**)b;

    if (comparator) {

        byte_array_reset(comparator->str);
        vm_call(context, comparator, av, bv, NULL);

        struct variable *result = (struct variable*)stack_pop(context->operand_stack);
        if (result->type == VAR_SRC)
            result = array_get(result->list, 0);
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
    while (width--)
        *(p0 + width) = *(p1 + width);
}

int heapcmp(struct context *context,
            size_t width, void *base0, uint32_t index0, void *base1, uint32_t index1,
            struct variable *comparator)
{
    uint8_t *p0 = (uint8_t*)base0 + index0 * width;
    uint8_t *p1 = (uint8_t*)base1 + index1 * width;
    return compar(context, p0, p1, comparator);
}

int heapsortfg(struct context *context, void *base, size_t nel, size_t width, struct variable *comparator)
{
    if (!nel)
        return 0;
    void *t = malloc(width); // the temporary value
    unsigned int n = nel, parent = nel/2, index, child; // heap indexes
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
            if (child + 1 < n  && // choose the largest child
                heapcmp(context, width, base, child+1, base, child, comparator) > 0) {
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

struct variable *cfnc_char(struct context *context)
{
    struct variable *args = (struct variable*)stack_pop(context->operand_stack);
    struct variable *from = (struct variable*)array_get(args->list, 0);
    struct variable *index = (struct variable*)array_get(args->list, 1);

    assert_message(from->type == VAR_STR, "char from a non-str");
    assert_message(index->type == VAR_INT, "non-int index");
    uint8_t n = byte_array_get(from->str, index->integer);
    return variable_new_int(context, n);
}

struct variable *cfnc_sort(struct context *context)
{
    struct variable *args = (struct variable*)stack_pop(context->operand_stack);
    struct variable *self = (struct variable*)array_get(args->list, 0);

    assert_message(self->type == VAR_LST, "sorting a non-list");
    struct variable *comparator = (args->list->length > 1) ?
        (struct variable*)array_get(args->list, 1) :
        NULL;

    int num_items = self->list->length;

    int success = heapsortfg(context, self->list->data, num_items, sizeof(struct variable*), comparator);
    assert_message(!success, "error sorting");
    return NULL;
}

struct variable *cfnc_chop(struct context *context, bool part)
{
    struct variable *args = (struct variable*)stack_pop(context->operand_stack);
    struct variable *self = (struct variable*)array_get(args->list, 0);
    struct variable *start = (struct variable*)array_get(args->list, 1);

    assert_message(start->type == VAR_INT, "non-integer index");
    int32_t beginning = start->integer;

    int32_t foraslongas;
    if (args->list->length > 2) {
        struct variable *length = (struct variable*)array_get(args->list, 2);
        assert_message(length->type == VAR_INT, "non-integer length");
        foraslongas = length->integer;
    } else
        foraslongas = part ? self->str->length - beginning : 1;

    struct variable *result = variable_copy(context, self);
    if (part)
        result = variable_part(context, result, beginning, foraslongas);
    else
        variable_remove(result, beginning, foraslongas);
    return result;
}

static inline struct variable *cfnc_part(struct context *context) {
    return cfnc_chop(context, true);
}

static inline struct variable *cfnc_remove(struct context *context) {
    return cfnc_chop(context, false);
}

struct variable *cfnc_find2(struct context *context, bool has)
{
    struct variable *args = (struct variable*)stack_pop(context->operand_stack);
    struct variable *self = (struct variable*)array_get(args->list, 0);
    struct variable *sought = (struct variable*)array_get(args->list, 1);
    struct variable *start = args->list->length > 2 ? (struct variable*)array_get(args->list, 2) : NULL;
    null_check(self);
    null_check(sought);

    struct variable *result = NULL;

    if (self->type == VAR_STR && sought->type == VAR_STR) {                     // search for substring
        assert_message(!start || start->type == VAR_INT, "non-integer index");
        int32_t beginning = start ? start->integer : 0;
        int32_t index = byte_array_find(self->str, sought->str, beginning);
        if (has)
            result = variable_new_bool(context, index != -1);
        else
            result = variable_new_int(context, index);

    } else if (self->type == VAR_LST) {
        for (int i=0; i<self->list->length; i++) {
            struct variable *v = (struct variable*)array_get(self->list, i);
            if ((sought->type == VAR_INT && v->type == VAR_INT && v->integer == sought->integer) ||
                (sought->type == VAR_STR && v->type == VAR_STR && byte_array_equals(sought->str, v->str)))
                result = has ? variable_new_bool(context, true) : v;
        }
    }
    if ((result == NULL) && (self->map != NULL) && (sought->type == VAR_STR))
        result = (struct variable*)map_get(self->map, sought);
    return result ? result : variable_new_nil(context);
}

struct variable *cfnc_find(struct context *context) {
    return cfnc_find2(context, false);
}

struct variable *cfnc_has(struct context *context) {
    return cfnc_find2(context, true);
}

struct variable *cfnc_insert(struct context *context)
{
    struct variable *args = (struct variable*)stack_pop(context->operand_stack);
    struct variable *self = (struct variable*)array_get(args->list, 0);
    struct variable *insertion = (struct variable*)array_get(args->list, 1);
    struct variable *start = args->list->length > 2 ? (struct variable*)array_get(args->list, 2) : NULL;
    null_check(self);
    null_check(insertion);
    assert_message(!start || start->type == VAR_INT, "non-integer index");

    int32_t position;
    switch (self->type) {
        case VAR_LST: {
            struct array *list = array_new_size(1);
            array_set(list, 0, insertion);
            insertion = variable_new_list(context, list);
            position = self->list->length;
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
    position = start ? start->integer : 0;

    struct variable *first = variable_part(context, variable_copy(context, self), 0, position);
    struct variable *second = variable_part(context, variable_copy(context, self), position, -1);
    struct variable *joined = variable_concatenate(context, 3, first, insertion, second);

    if (self->type == VAR_LST) {
        array_del(self->list);
        self->list = array_copy(joined->list);
    } else {
        byte_array_del(self->str);
        self->str = byte_array_copy(joined->str);
    } return joined;
}

struct variable *cfnc_serialize(struct context *context)
{
    struct variable *args = (struct variable*)stack_pop(context->operand_stack);
    struct variable *indexable = (struct variable*)array_get(args->list, 0);
    struct variable *typer = args->list->length > 1 ? (struct variable*)array_get(args->list, 1) : NULL;
    bool withType = !typer || typer->boolean; // default to true

    struct byte_array *bits = variable_serialize(context, NULL, indexable, withType);
    struct variable *result = variable_new_str(context, bits);
    byte_array_del(bits);
    return result;
}

struct variable *cfnc_deserialize(struct context *context)
{
    struct variable *args = (struct variable*)stack_pop(context->operand_stack);
    struct variable *indexable = (struct variable*)array_get(args->list, 0);
    struct byte_array *bits = indexable->str;
    byte_array_reset(bits);
    return variable_deserialize(context, bits);
}

//    a                b        c
// <sought> <replacement> [<start>]
// <start> <length> <replacement>
struct variable *cfnc_replace(struct context *context)
{
    struct variable *args = (struct variable*)stack_pop(context->operand_stack);
    struct variable *self = (struct variable*)array_get(args->list, 0);
    struct variable *a = (struct variable*)array_get(args->list, 1);
    struct variable *b = (struct variable*)array_get(args->list, 2);
    struct variable *c = args->list->length > 3 ? (struct variable*)array_get(args->list, 3) : NULL;

    null_check(self);
    null_check(b);
    assert_message(self->type == VAR_STR, "searching in a non-string");

    int32_t where = 0;
    struct byte_array *replaced = NULL;

    if (a->type == VAR_STR) { // find a, replace with b

        assert_message(b->type == VAR_STR, "non-string replacement");

        if (c) { // replace first match after index b

            assert_message(c->type == VAR_INT, "non-integer index");
            if (((where = byte_array_find(self->str, a->str, c->integer)) >= 0))
                replaced = byte_array_replace(self->str, b->str, where, b->str->length);

        } else {

            replaced = byte_array_copy(self->str);

            for(;;) { // replace all

                if ((where = byte_array_find(self->str, a->str, where)) < 0)
                    break;
                struct byte_array *replaced2 = byte_array_replace(replaced, b->str, where++, a->str->length);
                byte_array_del(replaced);
                replaced = replaced2;
            }
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

struct variable *builtin_method(struct context *context,
                                struct variable *indexable,
                                const struct variable *index)
{
    enum VarType it = indexable->type;
    char *idxstr = byte_array_to_string(index->str);
    struct variable *result = NULL;

    if (!strcmp(idxstr, FNC_LENGTH)) {
        int n;
        switch (indexable->type) {
            case VAR_LST: n = indexable->list->length;  break;
            case VAR_STR: n = indexable->str->length;   break;
            default:
                free(idxstr);
                exit_message("no length for non-indexable");
                return NULL;
        }
        result = variable_new_int(context, n);
    }
    else if (!strcmp(idxstr, FNC_TYPE)) {
        const char *typestr = var_type_str(it);
        struct byte_array *bats = byte_array_from_string(typestr);
        result = variable_new_str(context, bats);
        byte_array_del(bats);
    }

    else if (!strcmp(idxstr, FNC_STRING)) {
        struct byte_array *vv = variable_value(context, indexable);
        result = variable_new_str(context, vv);
        byte_array_del(vv);
    }

    else if (!strcmp(idxstr, FNC_LIST))
        result = variable_new_list(context, indexable->list);

    else if (!strcmp(idxstr, FNC_KEYS)) {
        assert_message(it == VAR_LST, "keys are only for list");

        struct variable *v = variable_new_list(context, NULL);
        if (indexable->map) {
            struct array *a = map_keys(indexable->map);
            for (int i=0; i<a->length; i++) {
                struct variable *u = variable_copy(context, (struct variable*)array_get(a, i));
                array_add(v->list, u);
            }
            array_del(a);
        }
        result = v;
    }

    else if (!strcmp(idxstr, FNC_VALUES)) {
        assert_message(it == VAR_LST, "values are only for list");
        if (indexable->map == NULL)
            result = variable_new_list(context, NULL);
        else {
            struct array *values = map_values(indexable->map);
            result = variable_new_list(context, (struct array*)values);
            array_del(values);
        }
    }

    else if (!strcmp(idxstr, FNC_SERIALIZE))
        result = variable_new_c(context, &cfnc_serialize);

    else if (!strcmp(idxstr, FNC_DESERIALIZE))
        result = variable_new_c(context, &cfnc_deserialize);

    else if (!strcmp(idxstr, FNC_SORT)) {
        assert_message(indexable->type == VAR_LST, "sorting non-list");
        result = variable_new_c(context, &cfnc_sort);
    }

    else if (!strcmp(idxstr, FNC_CHAR))
        result = variable_new_c(context, &cfnc_char);

    else if (!strcmp(idxstr, FNC_HAS))
        result = variable_new_c(context, &cfnc_has);

    else if (!strcmp(idxstr, FNC_FIND))
        result = variable_new_c(context, &cfnc_find);

    else if (!strcmp(idxstr, FNC_PART))
        result = variable_new_c(context, &cfnc_part);

    else if (!strcmp(idxstr, FNC_REMOVE))
        result = variable_new_c(context, &cfnc_remove);

    else if (!strcmp(idxstr, FNC_INSERT))
        result = variable_new_c(context, &cfnc_insert);

    else if (!strcmp(idxstr, FNC_REPLACE))
        result = variable_new_c(context, &cfnc_replace);

    free(idxstr);
    return result;
}
