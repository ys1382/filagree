#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <unistd.h>
#include <errno.h>

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

struct variable *sys_save(struct context *context)
{
    struct variable *args = (struct variable*)stack_pop(context->operand_stack);
    struct variable *v = param_var(context, args, 1);
    struct variable *path = param_var(context, args, 2);
    struct byte_array *bytes = byte_array_new();
    variable_serialize(context, bytes, v);
    int w = write_file(path->str, bytes, 0, -1);
    byte_array_del(bytes);
    return variable_new_int(context, w);
}

struct variable *sys_load(struct context *context)
{
    struct variable *value = (struct variable*)stack_pop(context->operand_stack);
    struct variable *path = (struct variable*)array_get(value->list.ordered, 1);
    struct byte_array *file_bytes = read_file(path->str, 0, 0);
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
    uint32_t from = param_int(args, 3);
    uint32_t timestamp = param_int(args, 4);
    
    int w = write_file(path->str, v->str, from, timestamp);
    return variable_new_int(context, w);
}

struct variable *sys_open(struct context *context)
{
    struct variable *args = (struct variable*)stack_pop(context->operand_stack);
    struct variable *path = param_var(context, args, 1);
    char *path2 = byte_array_to_string(path->str);

    bool result = hal_open(path2);

    struct variable *result2 = variable_new_bool(context, result);
    variable_push(context, result2);
    free(path2);
    return NULL;
}

// returns contents and modification time of file
struct variable *sys_read(struct context *context)
{
    struct variable *args = (struct variable*)stack_pop(context->operand_stack);
    struct variable *path = param_var(context, args, 1);
    uint32_t offset = param_int(args, 2);
    uint32_t length = param_int(args, 3);

    struct byte_array *bytes = read_file(path->str, offset, length);
    DEBUGPRINT("read %d bytes\n", bytes ? bytes->length : 0);

    struct variable *content = NULL;
    if (NULL != bytes)
    {
        content = variable_new_str(context, bytes);
        byte_array_del(bytes);
    }
    else
    {
        context->error = variable_new_str_chars(context, "could not load file");
        content = variable_new_nil(context);
    }
    variable_push(context, content);

    long mod = file_modified(byte_array_to_string(path->str));
    struct variable *mod2 = variable_new_int(context, (int32_t)mod);
    variable_push(context, mod2);
    struct variable *result = variable_new_src(context, 2);

    return result;
}

struct variable *sys_loop(struct context *context)
{
    stack_pop(context->operand_stack); // args
    hal_loop(context);
    return NULL;
}

struct variable *sys_exit(struct context *context)
{
    struct variable *args = (struct variable*)stack_pop(context->operand_stack);
    int32_t ret = param_int(args, 1);
    DEBUGPRINT("\nexit %d\n", ret);
    exit(ret);
    return NULL;
}

struct variable *sys_sleep(struct context *context)
{
    struct variable *args = (struct variable*)stack_pop(context->operand_stack);
    int32_t milliseconds = param_int(args, 1);
    hal_sleep(milliseconds);
    return NULL;
}

struct variable *sys_timer(struct context *context)
{
    struct variable *args = (struct variable*)stack_pop(context->operand_stack);
    int32_t milliseconds = param_int(args, 1);
    struct variable *logic = param_var(context, args, 2);
    bool repeats = false;
    if (args->list.ordered->length > 3)
    {
        struct variable *arg3 = array_get(args->list.ordered, 3);
        repeats = arg3->integer;
    }
    
    hal_timer(context, milliseconds, logic, repeats);
    return NULL;
}

struct variable *sys_forkexec(struct context *context)
{
    struct variable *args = (struct variable*)stack_pop(context->operand_stack);

    const char *app = param_str(args, 1);

    uint32_t argc = args->list.ordered->length - 2;
    char **argv = malloc(sizeof(char*) * (argc+1));
    for (int i=1; i<argc+2; i++)
        argv[i-1] = param_str(args, i);
    argv[argc+1] = NULL;
    
    pid_t pid = fork();
    if (pid < 0)
        perror("fork");
    else if (pid == 0)
    {
        if (execv(app, argv) < 0)
            perror("execv");
        exit(0);
    }

    return variable_new_int(context, pid);
}


// runs bytecode
struct variable *sys_run(struct context *context)
{
    struct variable *value = (struct variable*)stack_pop(context->operand_stack);
    struct variable *script = (struct variable*)array_get(value->list.ordered, 1);
    execute_with(context, script->str, true);
    return NULL;
}

// compiles and runs bytecode
struct variable *sys_interpret(struct context *context)
{
    struct variable *value = (struct variable*)stack_pop(context->operand_stack);
    struct variable *script = (struct variable*)array_get(value->list.ordered, 1);
    interpret_string(context, script->str);
    return NULL;
}

// deletes file or folder
struct variable *sys_move(struct context *context)
{
    struct variable *value = (struct variable*)stack_pop(context->operand_stack);
    const char *src = param_str(value, 1);
    const char *dst = param_str(value, 2);
    assert_message((strlen(src)>1) && (strlen(dst)>1), "oops");

    char mvcmd[100];
    sprintf(mvcmd, "mv %s %s", src, dst);
    if (system(mvcmd))
        DEBUGPRINT("\nCould not mv from %s to %s\n", src, dst);
    return NULL;
}

#if !(TARGET_OS_IPHONE)

// deletes file or folder
struct variable *sys_rm(struct context *context)
{
    struct variable *value = (struct variable*)stack_pop(context->operand_stack);
    struct variable *path = (struct variable*)array_get(value->list.ordered, 1);
    char *path2 = byte_array_to_string(path->str);
    assert_message(strlen(path2)>1, "oops");
    char rmcmd[100];
    sprintf(rmcmd, "rm -rf %s", path2);
    if (system(rmcmd))
        DEBUGPRINT("\nCould not rm %s\n", path2);
    free(path2);
    return NULL;
}

// creates directory
struct variable *sys_mkdir(struct context *context)
{
    struct variable *value = (struct variable*)stack_pop(context->operand_stack);
    struct variable *path = (struct variable*)array_get(value->list.ordered, 1);
    char *path2 = byte_array_to_string(path->str);

    char mkcmd[100];
    sprintf(mkcmd, "mkdir -p %s", path2);
    if (system(mkcmd))
        DEBUGPRINT("\nCould not mkdir %s\n", path2);

    free(path2);
    return NULL;
}

#else
struct variable *sys_mkdir(struct context *context);
struct variable *sys_rm(struct context *context);
#endif

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
    char *str = (char*)((struct variable*)array_get(value->list.ordered, 1))->str->data;
    uint32_t offset = value->list.ordered->length > 2 ?
        ((struct variable*)array_get(value->list.ordered, 2))->integer : 0;

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
    const int32_t n = ((struct variable*)array_get(arguments->list.ordered, 1))->integer;
    double s = sin(n);
    return variable_new_float(context, s);
}

char *param_str(const struct variable *value, uint32_t index)
{
    if (index >= value->list.ordered->length)
        return NULL;
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
    if (index >= value->list.ordered->length)
        return 0;
    struct variable *result = (struct variable*)array_get(value->list.ordered, index);
    if (result->type == VAR_NIL)
        return 0;
    assert_message(result->type == VAR_INT, "not an int");
    return result->integer;
}

bool param_bool(const struct variable *value, uint32_t index) {
    if (index >= value->list.ordered->length)
        return false;
    struct variable *result = (struct variable*)array_get(value->list.ordered, index);
    enum VarType type = result->type;
    switch (type) {
        case VAR_BOOL:  return result->boolean;
        case VAR_INT:   return result->integer;
        case VAR_NIL:   return false;
        case VAR_VOID:  return NULL != result->ptr;
            default:
            return true;
    }
    if ((type == VAR_NIL) || ((type == VAR_INT) && !result->integer))
        return false;
    assert_message(type == VAR_BOOL, "not a bool");
    return result->boolean;
}


struct variable *param_var(struct context *context, const struct variable *value, uint32_t index) {
    if (index >= value->list.ordered->length)
        return NULL;
    struct variable *v = (struct variable*)array_get(value->list.ordered, index);

    return v;
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
    struct variable *arguments = (struct variable*)stack_pop(context->operand_stack);
    struct variable *uictx = (struct variable*)array_get(arguments->list.ordered, 1);
    const char *str = param_str(arguments, 2);

    int32_t w=0,h=0;
    void *label = hal_label(uictx, &w, &h, str);
    return ui_result(context, label, w, h);
}

struct variable *sys_input(struct context *context)
{
    struct variable *arguments = (struct variable*)stack_pop(context->operand_stack);
    struct variable *uictx = (struct variable*)array_get(arguments->list.ordered, 1);
    const char *hint = param_str(arguments, 2);
    bool multiline = param_bool(arguments, 3);
    bool readonly = param_bool(arguments, 4);

    int32_t w=0, h=0;
    void *input = hal_input(uictx, &w, &h, hint, multiline, readonly);
    return ui_result(context, input, w, h);
}

struct variable *sys_button(struct context *context)
{
    struct variable *value = (struct variable*)stack_pop(context->operand_stack);
    struct variable *uictx = (struct variable*)array_get(value->list.ordered, 1);
    struct variable *logic = param_var(context, value, 2);
    char *text = param_str(value, 3);
    char *image = param_str(value, 4);

    int32_t w=0,h=0;
    void *button = hal_button(context, uictx, &w, &h, logic, text, image);
    return ui_result(context, button, w, h);
}

struct variable *sys_table(struct context *context)
{
    struct variable *value = (struct variable*)stack_pop(context->operand_stack);
    struct variable *uictx = (struct variable*)array_get(value->list.ordered, 1);
    struct variable *list = param_var(context, value, 2);
    struct variable *logic = param_var(context, value, 3);

    void *table = hal_table(context, uictx, list, logic);

    return ui_result(context, table, 0,0);
}

// set the text in the UI control
struct variable *sys_ui_set(struct context *context)
{
    struct variable *arguments = (struct variable*)stack_pop(context->operand_stack);
    struct variable *widget    = (struct variable*)array_get(arguments->list.ordered, 1);
    struct variable *value     = (struct variable*)array_get(arguments->list.ordered, 2);
    if (widget->type != VAR_NIL)
        hal_ui_set(widget->ptr, value);
    return NULL;
}

// put the control on screen at x,y
struct variable *sys_ui_put(struct context *context)
{
    struct variable *arguments = (struct variable*)stack_pop(context->operand_stack);
    struct variable *widget    = (struct variable*)array_get(arguments->list.ordered, 1);
    int32_t x = param_int(arguments, 2);
    int32_t y = param_int(arguments, 3);
    int32_t w = param_int(arguments, 4);
    int32_t h = param_int(arguments, 5);
    hal_ui_put(widget->ptr, x, y, w, h);
    return NULL;
}

struct variable *sys_ui_get(struct context *context)
{
    struct variable *arguments = (struct variable*)stack_pop(context->operand_stack);
    struct variable *widget    = (struct variable*)array_get(arguments->list.ordered, 1);
    return hal_ui_get(context, widget->ptr);
}

struct variable *sys_graphics(struct context *context)
{
    const struct variable *value = (const struct variable*)stack_pop(context->operand_stack);
    const struct variable *shape = (const struct variable*)array_get(value->list.ordered, 1);
    hal_graphics(shape);
    return NULL;
}

struct variable *sys_synth(struct context *context)
{
    struct variable *arguments = (struct variable*)stack_pop(context->operand_stack);
    const struct byte_array *bytes = ((struct variable*)array_get(arguments->list.ordered, 1))->str;
    hal_synth(bytes->data, bytes->length);
    return NULL;
}

struct variable *sys_sound(struct context *context)
{
    struct variable *arguments = (struct variable*)stack_pop(context->operand_stack);
    const struct byte_array *url = ((struct variable*)array_get(arguments->list.ordered, 1))->str;
    hal_sound((const char*)url->data);
    return NULL;
}

struct variable *sys_window(struct context *context)
{
    struct variable *value = (struct variable*)stack_pop(context->operand_stack);
    int w=0, h=0;
    if (value->list.ordered->length > 2) {
        w = param_int(value, 1);
        h = param_int(value, 2);
    }

    struct variable *uictx = param_var(context, value, 2);
    struct variable *logic = param_var(context, value, 3);

    context->singleton->keepalive = true; // so that context_del isn't called when UI is active
    hal_window(context, uictx, &w, &h, logic);
    variable_push(context, variable_new_int(context, w));
    variable_push(context, variable_new_int(context, h));
    return variable_new_src(context, 2);
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

const char *remove_substring(const char *original, const char *toremove)
{
    char *s, *copy = malloc(strlen(original)+1);
    strcpy(copy, original);
    while (toremove && (s = strstr(copy, toremove)))
    {
        char *splice = s + strlen(toremove);
        memmove(copy, splice, 1 + strlen(splice));
    }
    return copy;
}

// this is the fn parameter passed into file_list()
int file_list_callback(const char *path, bool dir, long mod, void *fl_context)
{
    // -> /
    path = remove_substring(path, "//");
    path = remove_substring(path, hal_doc_path(NULL));
    //printf("file_list_callback %s\n", path);

    struct file_list_context *flc = (struct file_list_context*)fl_context;
    struct variable *path3 = variable_new_str_chars(flc->context, path);

    struct variable *key2 = variable_new_str_chars(flc->context, RESERVED_DIR);
    struct variable *value = variable_new_bool(flc->context, dir);
    struct variable *metadata = variable_new_list(flc->context, NULL);
    variable_map_insert(flc->context, metadata, key2, value);

    key2 = variable_new_str_chars(flc->context, RESERVED_MODIFIED);
    value = variable_new_int(flc->context, (int32_t)mod);
    variable_map_insert(flc->context, metadata, key2, value);
    variable_map_insert(flc->context, flc->result, path3, metadata);

    return 0;
}

struct variable *sys_file_list(struct context *context)
{
    struct variable *arguments = (struct variable*)stack_pop(context->operand_stack);
    struct variable *path = array_get(arguments->list.ordered, 1);
    const char *path2 = hal_doc_path(path->str);

    struct variable *result = variable_new_list(context, NULL);
    struct file_list_context flc = {context, result};

    file_list(path2, &file_list_callback, &flc);
    return flc.result;
}

struct string_func builtin_funcs[] = {
	{"args",        &sys_args},
    {"print",       &sys_print},
    {"atoi",        &sys_atoi},
    {"read",        &sys_read},
    {"write",       &sys_write},
    {"open",        &sys_open},
    {"save",        &sys_save},
    {"load",        &sys_load},
    {"rm",          &sys_rm},
    {"move",        &sys_move},
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
    {"loop",        &sys_loop},
    {"exit",        &sys_exit},
    {"sleep",       &sys_sleep},
    {"timer",       &sys_timer},
    {"forkexec",    &sys_forkexec},
#ifndef NO_UI
    {"window",      &sys_window},
    {"label",       &sys_label},
    {"button",      &sys_button},
    {"input",       &sys_input},
    {"synth",       &sys_synth},
    {"sound",       &sys_sound},
    {"table",       &sys_table},
    {"graphics",    &sys_graphics},
    {"ui_get",      &sys_ui_get},
    {"ui_set",      &sys_ui_set},
    {"ui_put",      &sys_ui_put},
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


int compar(struct context *context, const void *a, const void *b, struct variable *comparator)
{
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
    struct variable *from = (struct variable*)array_get(args->list.ordered, 0);
    struct variable *index = (struct variable*)array_get(args->list.ordered, 1);

    assert_message(from->type == VAR_STR, "char from a non-str");
    assert_message(index->type == VAR_INT, "non-int index");
    uint8_t n = byte_array_get(from->str, index->integer);
    return variable_new_int(context, n);
}

struct variable *cfnc_sort(struct context *context)
{
    struct variable *args = (struct variable*)stack_pop(context->operand_stack);
    struct variable *self = (struct variable*)array_get(args->list.ordered, 0);

    assert_message(self->type == VAR_LST, "sorting a non-list");
    struct variable *comparator = (args->list.ordered->length > 1) ?
        (struct variable*)array_get(args->list.ordered, 1) :
        NULL;

    int num_items = self->list.ordered->length;

    int success = heapsortfg(context, self->list.ordered->data, num_items, sizeof(struct variable*), comparator);
    assert_message(!success, "error sorting");
    return NULL;
}

struct variable *cfnc_chop(struct context *context, bool snip)
{
    int32_t beginning, foraslongas;

    struct variable *args = (struct variable*)stack_pop(context->operand_stack);
    struct variable *self = (struct variable*)array_get(args->list.ordered, 0);

    if (args->list.ordered->length > 1) {
        struct variable *start = (struct variable*)array_get(args->list.ordered, 1);
        assert_message(start->type == VAR_INT, "non-integer index");
        beginning = start->integer;
    }
    else beginning = 0;

    if (args->list.ordered->length > 2) {
        struct variable *length = (struct variable*)array_get(args->list.ordered, 2);
        assert_message(length->type == VAR_INT, "non-integer length");
        foraslongas = length->integer;
    } else
        foraslongas = snip ? 1 : self->str->length - beginning;

    struct variable *result = variable_part(context, self, beginning, foraslongas);
    if (snip)
        variable_remove(self, beginning, foraslongas);
    return result;
}

static inline struct variable *cfnc_part(struct context *context) {
    return cfnc_chop(context, false);
}

static inline struct variable *cfnc_remove(struct context *context) {
    return cfnc_chop(context, true);
}

struct variable *cfnc_find2(struct context *context, bool has)
{
    struct variable *args = (struct variable*)stack_pop(context->operand_stack);
    struct variable *self = (struct variable*)array_get(args->list.ordered, 0);
    struct variable *sought = (struct variable*)array_get(args->list.ordered, 1);
    struct variable *start = args->list.ordered->length > 2 ?
        (struct variable*)array_get(args->list.ordered, 2) : NULL;
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
    struct variable *self = (struct variable*)array_get(args->list.ordered, 0);
    struct variable *insertion = (struct variable*)array_get(args->list.ordered, 1);
    struct variable *start = args->list.ordered->length > 2 ?
        (struct variable*)array_get(args->list.ordered, 2) : NULL;
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

struct variable *cfnc_serialize(struct context *context)
{
    struct variable *args = (struct variable*)stack_pop(context->operand_stack);
    struct variable *indexable = (struct variable*)array_get(args->list.ordered, 0);

    struct byte_array *bits = variable_serialize(context, NULL, indexable);
    struct variable *result = variable_new_str(context, bits);
    byte_array_del(bits);
    return result;
}

struct variable *cfnc_deserialize(struct context *context)
{
    struct variable *args = (struct variable*)stack_pop(context->operand_stack);
    struct variable *indexable = (struct variable*)array_get(args->list.ordered, 0);
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

struct variable *variable_map_list(struct context *context,
                                   struct variable *indexable,
                                   struct array* (*map_list)(const struct map*))
{
    assert_message(indexable->type == VAR_LST, "values are only for list");
    struct variable *result = variable_new_list(context, NULL);
    if (NULL != indexable->list.map) {
        struct array *a = map_list(indexable->list.map);
        for (int i=0; i<a->length; i++) {
            struct variable *u = variable_copy(context, (struct variable*)array_get(a, i));
            array_add(result->list.ordered, u);
        }
        array_del(a);
    }
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
            case VAR_LST: n = indexable->list.ordered->length;  break;
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
        result = variable_new_list(context, indexable->list.ordered);

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

    else if (!strcmp(idxstr, FNC_KEYS))
        result = variable_map_list(context, indexable, &map_keys);

    else if (!strcmp(idxstr, FNC_VALS))
        result = variable_map_list(context, indexable, &map_vals);

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

    free(idxstr);
    return result;
}
