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
#include "file.h"

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
    struct variable *args = (struct variable*)stack_pop(context->operand_stack);
    struct variable *v = param_var(context, args, 1);
    struct variable *path = param_var(context, args, 2);
    struct byte_array *bytes = byte_array_new();
    variable_serialize(context, bytes, v);
    int w = write_file(path->str, bytes, -1);
    byte_array_del(bytes);
    return variable_new_int(context, w);
}

struct variable *sys_load(struct context *context)
{
    struct variable *value = (struct variable*)stack_pop(context->operand_stack);
    struct variable *path = (struct variable*)array_get(value->list, 1);
    struct byte_array *file_bytes = read_file(path->str);
    if (NULL == file_bytes)
        return variable_new_nil(context);
    struct variable *v = variable_deserialize(context, file_bytes);
    byte_array_del(file_bytes);
    return v;
}

struct variable *sys_write(struct context *context)
{
    struct variable *args = (struct variable*)stack_pop(context->operand_stack);
    struct variable *path = param_var(context, args, 1);
    struct variable *v = param_var(context, args, 2);

    int w = write_file(path->str, v->str, -1);
    return variable_new_int(context, w);
}

// returns contents and modification time of file
struct variable *sys_read(struct context *context)
{
    struct variable *args = (struct variable*)stack_pop(context->operand_stack);
    struct variable *path = param_var(context, args, 1);

    struct variable *content = NULL;

    struct byte_array *bytes = read_file(path->str);
    if (NULL != bytes)
    {
        content = variable_new_str(context, bytes);
    }
    else
    {
        context->error = variable_new_str_chars(context, "could not load file");
        content = variable_new_nil(context);
    }
    byte_array_del(bytes);
    variable_push(context, content);

    long mod = file_modified(byte_array_to_string(path->str));
    struct variable *mod2 = variable_new_int(context, mod);
    variable_push(context, mod2);
    struct variable *result = variable_new_src(context, 2);
    
    return result;
}

// runs bytecode
struct variable *sys_run(struct context *context)
{
    struct variable *value = (struct variable*)stack_pop(context->operand_stack);
    struct variable *script = (struct variable*)array_get(value->list, 1);
    execute_with(context, script->str, true);
    return NULL;
}

// compiles and runs bytecode
struct variable *sys_interpret(struct context *context)
{
    struct variable *value = (struct variable*)stack_pop(context->operand_stack);
    struct variable *script = (struct variable*)array_get(value->list, 1);
    interpret_string(context, script->str);
    return NULL;
}

// deletes file or folder
struct variable *sys_rm(struct context *context)
{
    struct variable *value = (struct variable*)stack_pop(context->operand_stack);
    struct variable *path = (struct variable*)array_get(value->list, 1);
    char *path2 = byte_array_to_string(path->str);
    char rmcmd[100];
    sprintf(rmcmd, "rm -rf %s", path2);
    system(rmcmd);
    free(path2);
    return NULL;
}

// creates directory
struct variable *sys_mkdir(struct context *context)
{
    struct variable *value = (struct variable*)stack_pop(context->operand_stack);
    struct variable *path = (struct variable*)array_get(value->list, 1);
    char *path2 = byte_array_to_string(path->str);
    char mkcmd[100];
    sprintf(mkcmd, "mkdir -p %s", path2);
    system(mkcmd);
    free(path2);
    return NULL;
}

// arguments most recently passed into a function
struct variable *sys_args(struct context *context)
{
    stack_pop(context->operand_stack); // self
    struct program_state *above = (struct program_state*)stack_peek(context->program_stack, 1);
    struct variable *result = variable_copy(context, above->args);
    result->type = VAR_LST;
    return result;
}

// ascii to integer
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

// sine
struct variable *sys_sin(struct context *context) // radians
{
    struct variable *arguments = (struct variable*)stack_pop(context->operand_stack);
    const int32_t n = ((struct variable*)array_get(arguments->list, 1))->integer;
    double s = sin(n);
    return variable_new_float(context, s);
}

char *param_str(const struct variable *value, uint32_t index)
{
    if (index >= value->list->length)
        return NULL;
    const struct variable *strv = (struct variable*)array_get(value->list, index);
    char *str = NULL;
    switch (strv->type) {
        case VAR_NIL:                                           break;
        case VAR_STR: str = byte_array_to_string(strv->str);    break;
        default:      exit_message("wrong param type");         break;
    }
    return str;
}

// todo: handle nil return
int32_t param_int(const struct variable *value, uint32_t index) {
    if (index >= value->list->length)
        return 0;
    struct variable *result = (struct variable*)array_get(value->list, index);
    assert_message(result->type == VAR_INT, "not an int");
    return result->integer;
}

struct variable *param_var(struct context *context, const struct variable *value, uint32_t index) {
    if (index >= value->list->length)
        return NULL;
    struct variable *v = (struct variable*)array_get(value->list, index);

    //char buf[1000];
    //DEBUGPRINT("param_var %p->%p : %s\n", v, v->map, variable_value_str(context, v, buf));

    return v; //variable_copy(context, v);
}

#ifndef NO_UI

struct variable *ui_result(struct context *context, void *widget, int32_t w, int32_t h)
{
    struct variable *widget2 = variable_new_void(context, widget);
    variable_push(context, widget2);
    variable_push(context, variable_new_int(context, w));
    variable_push(context, variable_new_int(context, h));
    return variable_new_src(context, 3);
}

struct variable *sys_label(struct context *context)
{
    struct variable *value = (struct variable*)stack_pop(context->operand_stack);
    int32_t x = param_int(value, 1);
    int32_t y = param_int(value, 2);
    const char *str = param_str(value, 3);

    int32_t w=0,h=0;
    void *label = hal_label(x, y, &w, &h, str);
    return ui_result(context, label, w, h);
}

struct variable *sys_input(struct context *context)
{
    struct variable *arguments = (struct variable*)stack_pop(context->operand_stack);
    struct variable *uictx = (struct variable*)array_get(arguments->list, 1);
    int32_t x = param_int(arguments, 2);
    int32_t y = param_int(arguments, 3);
    int32_t w=0, h=0;

    struct variable *values = (struct variable*)array_get(arguments->list, 4);
    struct variable *hint = NULL;
    if (values->type == VAR_LST)
        hint = (struct variable *)array_get(values->list, 1);

    void *input = hal_input(uictx, x, y, &w, &h, hint, false);
    return ui_result(context, input, w, h);
}

struct variable *sys_button(struct context *context)
{
    struct variable *value = (struct variable*)stack_pop(context->operand_stack);
    struct variable *uictx = (struct variable*)array_get(value->list, 1);
    int32_t x = param_int(value, 2);
    int32_t y = param_int(value, 3);
    struct variable *logic = param_var(context, value, 4);
    char *text = param_str(value, 5);
    char *image = param_str(value, 6);

    int32_t w,h;
    void *button = hal_button(context, uictx, x, y, &w, &h, logic, text, image);
    return ui_result(context, button, w, h);
}

struct variable *sys_table(struct context *context)
{
    struct variable *value = (struct variable*)stack_pop(context->operand_stack);
    struct variable *uictx = (struct variable*)array_get(value->list, 1);
    int32_t x = param_int(value, 2);
    int32_t y = param_int(value, 3);
    int32_t w = param_int(value, 4);
    int32_t h = param_int(value, 5);
    struct variable *list = param_var(context, value, 6);
    struct variable *logic = param_var(context, value, 7);

    void *table = hal_table(context, uictx, x, y, w, h, list, logic);

    return ui_result(context, table, w, h);
}

struct variable *sys_ui_set(struct context *context)
{
    struct variable *arguments = (struct variable*)stack_pop(context->operand_stack);
    struct variable *widget    = (struct variable*)array_get(arguments->list, 1);
    struct variable *value     = (struct variable*)array_get(arguments->list, 2);
    hal_ui_set(widget->ptr, value);
    return NULL;
}

struct variable *sys_ui_get(struct context *context)
{
    struct variable *arguments = (struct variable*)stack_pop(context->operand_stack);
    struct variable *widget    = (struct variable*)array_get(arguments->list, 1);
    return hal_ui_get(context, widget->ptr);
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
    DEBUGPRINT("sys_window\n");
    struct variable *value = (struct variable*)stack_pop(context->operand_stack);
    int w=0, h=0;
    if (value->list->length > 2) {
        w = param_int(value, 1);
        h = param_int(value, 2);
    }

    struct variable *uictx = param_var(context, value, 2);
    struct variable *logic = param_var(context, value, 3);

    //pthread_mutex_lock(&context->singleton->lock);
    context->singleton->num_threads++;
    //pthread_mutex_unlock(&context->singleton->lock);

    void *window = hal_window(context, uictx, &w, &h, logic);
    return ui_result(context, window, w, h);
}

struct variable *sys_loop(struct context *context)
{
    stack_pop(context->operand_stack); // self
    gil_unlock(context, "sys_loop");
    hal_loop(context);
    return NULL;
}

#endif // NO_UI

struct variable *sys_file_listen(struct context *context)
{
    struct variable *arguments = (struct variable*)stack_pop(context->operand_stack);
    const char *path = param_str(arguments, 1);
    struct variable *listener = param_var(context, arguments, 2);
    gil_unlock(context, "sys_file_listen");
    hal_file_listen(context, path, listener);
    return NULL;
}

// this is the fn parameter passed into file_list()
int file_list_callback(const char *path, bool dir, long mod, void *fl_context)
{
    printf("file_list_callback %s\n", path);//, isDir ? "/" : "");

    struct file_list_context *flc = (struct file_list_context*)fl_context;
    struct variable *path3 = variable_new_str_chars(flc->context, path);

    struct variable *key2 = variable_new_str_chars(flc->context, RESERVED_DIR);
    struct variable *value = variable_new_bool(flc->context, dir);
    struct variable *metadata = variable_new_list(flc->context, NULL);
    variable_map_insert(flc->context, metadata, key2, value);

    key2 = variable_new_str_chars(flc->context, RESERVED_MODIFIED);
    value = variable_new_int(flc->context, mod);
    variable_map_insert(flc->context, metadata, key2, value);
    variable_map_insert(flc->context, flc->result, path3, metadata);

    return 0;
}

struct variable *sys_file_list(struct context *context)
{
    struct variable *arguments = (struct variable*)stack_pop(context->operand_stack);
    const char *path = param_str(arguments, 1);
    struct variable *result = variable_new_list(context, NULL);
    struct file_list_context flc = {context, result};
    file_list(path, &file_list_callback, &flc);
    return flc.result;
}

struct string_func builtin_funcs[] = {
	{"args",        &sys_args},
    {"print",       &sys_print},
    {"atoi",        &sys_atoi},
    {"read",        &sys_read},
    {"write",       &sys_write},
    {"save",        &sys_save},
    {"load",        &sys_load},
    {"rm",          &sys_rm},
    {"mkdir",       &sys_mkdir},
    {"sin",         &sys_sin},
    {"run",         &sys_run},
    {"interpret",   &sys_interpret},
    {"listen",      &sys_socket_listen},
    {"send",        &sys_send},
    {"connect",     &sys_connect},
    {"disconnect",  &sys_disconnect},
    {"file_list",   &sys_file_list},
    {"file_listen", &sys_file_listen},
#ifndef NO_UI
    {"window",      &sys_window},
    {"loop",        &sys_loop},
    {"label",       &sys_label},
    {"button",      &sys_button},
    {"input",       &sys_input},
    {"synth",       &sys_synth},
    {"sound",       &sys_sound},
    {"table",       &sys_table},
    {"graphics",    &sys_graphics},
    {"ui_get",      &sys_ui_get},
    {"ui_set",      &sys_ui_set},
#endif // NO_UI
};

struct variable *sys_new(struct context *context)
{
    struct variable *sys = variable_new_list(context, NULL);

    for (int i=0; i<ARRAY_LEN(builtin_funcs); i++)
    {
        struct variable *key = variable_new_str_chars(context, builtin_funcs[i].name);
        struct variable *value = variable_new_cfnc(context, builtin_funcs[i].func);
        variable_map_insert(context, sys, key, value);
    }

    return sys;
}

// built-in member functions

#define FNC_STRING      "string"
#define FNC_LIST        "list"
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
#define FNC_EXIT        "exit"
#define FNC_SLEEP       "sleep"
#define FNC_TIMER       "timer"

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
    int32_t beginning, foraslongas;

    struct variable *args = (struct variable*)stack_pop(context->operand_stack);
    struct variable *self = (struct variable*)array_get(args->list, 0);

    if (args->list->length > 1) {
        struct variable *start = (struct variable*)array_get(args->list, 1);
        assert_message(start->type == VAR_INT, "non-integer index");
        beginning = start->integer;
    }
    else beginning = 0;

    if (args->list->length > 2) {
        struct variable *length = (struct variable*)array_get(args->list, 2);
        assert_message(length->type == VAR_INT, "non-integer length");
        foraslongas = length->integer;
    } else
        foraslongas = part ? self->str->length - beginning : 1;

    struct variable *result = variable_part(context, self, beginning, foraslongas);
    variable_remove(self, beginning, foraslongas);
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

    struct variable *result = variable_find(context, self, sought, start);
    if (has)
        return variable_new_bool(context, result->type != VAR_NIL);
    return result;
}

struct variable *cfnc_find(struct context *context) {
    return cfnc_find2(context, false);
}

struct variable *cfnc_has(struct context *context) {
    return cfnc_find2(context, true);
}

struct variable *cfnc_insert(struct context *context) // todo: test
{
    struct variable *args = (struct variable*)stack_pop(context->operand_stack);
    struct variable *self = (struct variable*)array_get(args->list, 0);
    struct variable *insertion = (struct variable*)array_get(args->list, 1);
    struct variable *start = args->list->length > 2 ? (struct variable*)array_get(args->list, 2) : NULL;
    null_check(self);
    null_check(insertion);
    assert_message(!start || start->type == VAR_INT, "non-integer index");

    int32_t position = 0;
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
    //position = start ? start->integer : 0;

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

    struct byte_array *bits = variable_serialize(context, NULL, indexable);
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

struct variable *cfnc_exit(struct context *context)
{
    struct variable *args = (struct variable*)stack_pop(context->operand_stack);
    int32_t ret = param_int(args, 1);
    exit(ret);
    return NULL;
}

struct variable *cfnc_sleep(struct context *context)
{
    struct variable *args = (struct variable*)stack_pop(context->operand_stack);
    int32_t milliseconds = param_int(args, 1);
    hal_sleep(milliseconds);
    return NULL;
}

struct variable *cfnc_timer(struct context *context)
{
    struct variable *args = (struct variable*)stack_pop(context->operand_stack);
    int32_t milliseconds = param_int(args, 1);
    struct variable *logic = param_var(context, args, 2);
    bool repeats = false;
    if (args->list->length > 3)
    {
        struct variable *arg3 = array_get(args->list, 3);
        repeats = arg3->integer;
    }

    hal_timer(context, milliseconds, logic, repeats);
    return NULL;
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

    struct byte_array *replaced = NULL;

    if (a->type == VAR_STR) { // find a, replace with b

        assert_message(b->type == VAR_STR, "non-string replacement");
        int32_t where = 0;

        if (c) { // replace first match after index c

            assert_message(c->type == VAR_INT, "non-integer index");
            if (((where = byte_array_find(self->str, a->str, c->integer)) >= 0))
                replaced = byte_array_replace(self->str, b->str, where, b->str->length);

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
        result = variable_new_str_chars(context, typestr);
    }

    else if (!strcmp(idxstr, FNC_STRING)) {
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
    }

    else if (!strcmp(idxstr, FNC_LIST))
        result = variable_new_list(context, indexable->list);

    else if (!strcmp(idxstr, FNC_KEY)) {
        if (indexable->type == VAR_KVP)
            result = indexable->kvp.key;
        else
            result = variable_new_nil(context);
    }

    else if (!strcmp(idxstr, FNC_VAL)) {
        if (indexable->type == VAR_KVP)
            result = indexable->kvp.val;
        else
            result = variable_new_nil(context);
    }

    else if (!strcmp(idxstr, FNC_KEYS)) {
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

    else if (!strcmp(idxstr, FNC_VALS)) {
        assert_message(it == VAR_LST, "values are only for list");
        if (NULL == indexable->map)
            result = variable_new_list(context, NULL);
        else {
            struct array *values = map_vals(indexable->map);
            result = variable_new_list(context, (struct array*)values);
            array_del(values);
        }
    }

    else if (!strcmp(idxstr, FNC_SERIALIZE))
        result = variable_new_cfnc(context, &cfnc_serialize);

    else if (!strcmp(idxstr, FNC_DESERIALIZE))
        result = variable_new_cfnc(context, &cfnc_deserialize);

    else if (!strcmp(idxstr, FNC_SORT)) {
        assert_message(indexable->type == VAR_LST, "sorting non-list");
        result = variable_new_cfnc(context, &cfnc_sort);
    }

    else if (!strcmp(idxstr, FNC_CHAR))
        result = variable_new_cfnc(context, &cfnc_char);

    else if (!strcmp(idxstr, FNC_HAS))
        result = variable_new_cfnc(context, &cfnc_has);

    else if (!strcmp(idxstr, FNC_FIND))
        result = variable_new_cfnc(context, &cfnc_find);

    else if (!strcmp(idxstr, FNC_PART))
        result = variable_new_cfnc(context, &cfnc_part);

    else if (!strcmp(idxstr, FNC_REMOVE))
        result = variable_new_cfnc(context, &cfnc_remove);

    else if (!strcmp(idxstr, FNC_INSERT))
        result = variable_new_cfnc(context, &cfnc_insert);

    else if (!strcmp(idxstr, FNC_REPLACE))
        result = variable_new_cfnc(context, &cfnc_replace);

    else if (!strcmp(idxstr, FNC_EXIT))
        result = variable_new_cfnc(context, &cfnc_exit);
    
    else if (!strcmp(idxstr, FNC_SLEEP))
        result = variable_new_cfnc(context, &cfnc_sleep);

    else if (!strcmp(idxstr, FNC_TIMER))
        result = variable_new_cfnc(context, &cfnc_timer);

    free(idxstr);
    return result;
}
