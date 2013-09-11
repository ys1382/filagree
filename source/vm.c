#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <pthread.h>
#include <errno.h>

#include "util.h"
#include "serial.h"
#include "vm.h"
#include "variable.h"
#include "sys.h"
#include "node.h"

bool run(struct context *context, struct byte_array *program, struct map *env, bool in_context);
void display_code(struct context *context, struct byte_array *code);
const char* indentation(struct context *context);

#ifdef DEBUG

#define VM_DEBUGPRINT(...) DEBUGPRINT(__VA_ARGS__ ); if (!context->runtime) return;
#define INDENT context->indent++;
#define UNDENT context->indent--;

#else // not DEBUG

#define INDENT
#define UNDENT
#define VM_DEBUGPRINT(...)

#endif // not DEBUG

#define VAR_MAX         9999
#define GIL_SWITCH      100

// assertions //////////////////////////////////////////////////////////////

jmp_buf trying;

uint16_t current_thread_id() {
    return ((unsigned int)(VOID_INT)pthread_self() >> 12) & 0xFFF;
}

static void vm_exit() {
    DEBUGPRINT("exiting thread %" PRIu16 "\n", current_thread_id());
    longjmp(trying, 1);
}

void set_error(struct context *context, const char *format, va_list list)
{
    if (context == NULL)
    {
        DEBUGPRINT("can't set error because context is null");
        return;
    }
    null_check(format);
    const char *message = make_message(format, list);
    DEBUGPRINT("vm_error: %s\n", message);
    context->error = variable_new_err(context, message);
}

void *vm_exit_message(struct context *context, const char *format, ...)
{
    // make error variable
    va_list list;
    va_start(list, format);
    set_error(context, format, list);
    va_end(list);

    vm_exit();
    return NULL;
}

void vm_assert(struct context *context, bool assertion, const char *format, ...)
{
    if (!assertion) {

        // make error variable
        va_list list;
        va_start(list, format);
        set_error(context, format, list);
        va_end(list);

        vm_exit();
    }
}

void vm_null_check(struct context *context, const void* p) {
    vm_assert(context, p, ERROR_NULL);
}

// state ///////////////////////////////////////////////////////////////////

struct program_state *program_state_new(struct context *context, struct map *env)
{
    null_check(context);
    struct program_state *state = (struct program_state*)malloc(sizeof(struct program_state));
    state->named_variables = env ? map_copy(context, env) : map_new(context);
    state->args = variable_new_list(context, NULL); // todo: prevent GC
    stack_push(context->program_stack, state);
    return state;
}

void program_state_del(struct context *context, struct program_state *state)
{
    map_del(state->named_variables);
    free(state);
}

static inline void cfnc_length(struct context *context) {
    struct variable *args = (struct variable*)stack_pop(context->operand_stack);
    struct variable *indexable = (struct variable*)array_get(args->list, 0);
    assert_message(indexable->type==VAR_LST || indexable->type==VAR_STR, "no length for non-indexable");
    struct variable *result = variable_new_int(context, indexable->list->length);
    stack_push(context->operand_stack, result);
}

struct context *context_new(struct context *parent, // parent context
                            bool runtime,           // for interpreting bytecode
                            bool sys_funcs)         // create sys funcs
{
    struct context *context = (struct context*)malloc(sizeof(struct context));
    null_check(context);

    if (parent == NULL) // I am the mother of all contexts
    {
        struct context_shared *singleton = malloc(sizeof(struct context_shared));
        assert_message(!pthread_mutex_init(&singleton->gil, NULL), "gil init");
        assert_message(!pthread_cond_init(&singleton->thread_cond, NULL), "threads init");
        singleton->all_variables = array_new();
        singleton->callback = NULL;
        singleton->tick = 0;
        singleton->num_threads = 0;
        context->singleton = singleton;
    } else {
        context->singleton = parent->singleton;
    }

    context->program_stack = stack_new();
    context->operand_stack = stack_new();
    context->runtime = runtime;
    context->indent = 0;
    context->error = NULL;
    context->sys = sys_funcs ? sys_new(context) : NULL;

    return context;
}

void context_del(struct context *context)
{
    // wait for spawned threads
    struct context_shared *s = context->singleton;
    gil_lock(context, "context_del");

    while (s->num_threads > 0)
    {
        assert_message(!pthread_cond_wait(&s->thread_cond, &s->gil), "could not wait");

        if (s->num_threads == 0) // last remaining thread
        {
            gil_unlock(context, "context_del b");
            pthread_cond_destroy(&s->thread_cond);
            pthread_mutex_destroy(&s->gil);

            struct array *vars = s->all_variables;
            for (int i=0; i<vars->length; i++)
            {
                struct variable *v = (struct variable *)array_get(vars, i);
                variable_del(context, v);
            }
            array_del(vars);
        }
    }

    gil_unlock(context, "context_del c");

    while (!stack_empty(context->program_stack))
    {
        struct program_state *s = (struct program_state *)stack_pop(context->program_stack);
        program_state_del(context, s);
    }

    stack_del(context->program_stack);
    stack_del(context->operand_stack);
    free(context);
}

// garbage collection //////////////////////////////////////////////////////

void unmark_all(struct context *context)
{
    struct array *vars = context->singleton->all_variables;
    for (int i=0; i<vars->length; i++) {
        struct variable *v = (struct variable*)array_get(vars, i);
        variable_unmark(v);
    }
}

void mark_map(struct map *map, bool mark)
{
    if (map == NULL)
        return;

    struct array *a = map_keys(map);
    struct array *b = map_vals(map);
    for (int i=0; i<a->length; i++) {
        struct variable *aiv = (struct variable*)array_get(a,i);
        struct variable *biv = (struct variable*)array_get(b,i);
        if (mark) {
            variable_mark(aiv);
            variable_mark(biv);
        } else {
            variable_unmark(aiv);
            variable_unmark(biv);
        }
    }
    array_del(a);
    array_del(b);
}

void garbage_collect(struct context *context)
{
    int i;
    null_check(context);
    if (!context->runtime)
        return;
    if (context->singleton->all_variables->length < VAR_MAX)
        return;

    struct variable *v;
    printf("\n>%" PRIu16 " - garbage collect\n", current_thread_id());

    unmark_all(context);

    // mark named variables
    struct program_state *state;
    for (i=0; (state = (struct program_state*)stack_peek(context->program_stack, i)); i++) {
        mark_map(state->named_variables, true);
    }

    // mark variables in operand stack
    for (i=0; (v = (struct variable*)stack_peek(context->operand_stack, i)); i++) {
        variable_mark(v);
    }

    variable_mark(context->sys);

    // sweep
    struct array *vars = context->singleton->all_variables;
    for (i=0; i<vars->length; i++) {
        struct variable *v = (struct variable*)array_get(vars, i);
        if (v->visited == VISITED_NOT) {
            variable_del(context, v);
            array_remove(context->singleton->all_variables, i--, 1);
        }
    }

    unmark_all(context);
}

// display /////////////////////////////////////////////////////////////////

#ifdef DEBUG

const struct number_string opcodes[] = {
    {VM_NIL,    "NIL"},
    {VM_INT,    "INT"},
    {VM_BUL,    "BUL"},
    {VM_FLT,    "FLT"},
    {VM_STR,    "STR"},
    {VM_VAR,    "VAR"},
    {VM_FNC,    "FNC"},
    {VM_SRC,    "SRC"},
    {VM_LST,    "LST"},
    {VM_DST,    "DST"},
    {VM_KVP,    "KVP"},
    {VM_GET,    "GET"},
    {VM_PUT,    "PUT"},
    {VM_ADD,    "ADD"},
    {VM_SUB,    "SUB"},
    {VM_MUL,    "MUL"},
    {VM_DIV,    "DIV"},
    {VM_MOD,    "MOD"},
    {VM_AND,    "AND"},
    {VM_ORR,    "ORR"},
    {VM_NOT,    "NOT"},
    {VM_NEG,    "NEG"},
    {VM_EQU,    "EQU"},
    {VM_NEQ,    "NEQ"},
    {VM_GTN,    "GTN"},
    {VM_LTN,    "LTN"},
    {VM_GRQ,    "GRQ"},
    {VM_LEQ,    "LEQ"},
    {VM_IFF,    "IFF"},
    {VM_JMP,    "JMP"},
    {VM_CAL,    "CAL"},
    {VM_MET,    "MET"},
    {VM_RET,    "RET"},
    {VM_ITR,    "ITR"},
    {VM_COM,    "COM"},
    {VM_TRY,    "TRY"},
};

const char* indentation(struct context *context)
{
    null_check(context);
    static char str[100];
    int tab = 0;
    while (tab < context->indent)
        str[tab++] = '\t';
    str[tab] = 0;
    return (const char*)str;
}

static void display_program_counter(struct context *context, const struct byte_array *program)
{
    null_check(context);
#ifdef __ANDROID__
    DEBUGSPRINT(context->pcbuf, "%s>%" PRIu16 " - %2d:%3d ",
#else
    DEBUGSPRINT(context->pcbuf, "%s>%" PRIu16 " - %2ld:%3d ",
#endif
            indentation(context),
            current_thread_id(),
            program->current-program->data,
            *program->current);
}

void display_program(struct byte_array *program)
{
    struct context *context = context_new(NULL, false, false);

    INDENT
    DEBUGPRINT("%sprogram bytes:\n", indentation(context));

    INDENT
    for (int i=0; i<program->length; i++)
        DEBUGPRINT("%s%2d:%3d\n", indentation(context), i, program->data[i]);
    UNDENT

    DEBUGPRINT("%sprogram instructions:\n", indentation(context));
    byte_array_reset(program);
    display_code(context, program);
    context_del(context);

    UNDENT
    UNDENT
}

void display_code(struct context *context, struct byte_array *code)
{
    null_check(context);
    bool was_running = context->runtime;
    context->runtime = false;

    INDENT
    run(context, code, false, NULL);
    UNDENT

    context->runtime = was_running;
}

#else // not DEBUG

const char* indentation(struct context *context) { return ""; }
void display_code(struct context *context, struct byte_array *code) {}

#endif // DEBUG

// global interpreter lock

void gil_lock(struct context *context, const char *who)
{
    pthread_mutex_lock(&context->singleton->gil);
    // DEBUGPRINT("\n%s>%" PRIu16 " lock %s\n", indentation(context), current_thread_id(), who);
}

void gil_unlock(struct context *context, const char *who)
{
    pthread_mutex_unlock(&context->singleton->gil);
    // DEBUGPRINT("\n%s>%" PRIu16 " unlock %s\n", indentation(context), current_thread_id(), who);
}

// instruction implementations /////////////////////////////////////////////

struct variable *src(struct context *context, enum Opcode op, struct byte_array *program)
{
    int32_t size = serial_decode_int(program);
    DEBUGSPRINT(context->pcbuf, "%s%s %d\n", context->pcbuf, NUM_TO_STRING(opcodes, op), size);
    if (!context->runtime)
        return NULL;
    struct variable *v = variable_new_src(context, size);
    stack_push(context->operand_stack, v);
    return v;
}

void vm_call_src(struct context *context, struct variable *func)
{
    struct program_state *state = (struct program_state*)stack_peek(context->program_stack, 0);
    if (state == NULL)
        state = program_state_new(context, NULL);
    struct variable *s = (struct variable*)stack_peek(context->operand_stack, 0);

    if (func->closure)
        array_insert(s->list, 1, func->closure); // first argument

    state->args = variable_copy(context, s);

    INDENT

    // call the function
    switch (func->type) {
        case VAR_FNC:
            run(context, func->str, func->map, false);
            break;
        case VAR_CFNC: {
            struct variable *v = func->cfnc(context);
            if (v == NULL)
                v = variable_new_src(context, 0);
            else if (v->type != VAR_SRC) { // convert to VAR_SRC variable
                stack_push(context->operand_stack, v);
                v = variable_new_src(context, 1);
            }

            stack_push(context->operand_stack, v); // push the result
        } break;
        case VAR_NIL:
            DEBUGPRINT("nil func\n");
            break;
        default:
            vm_exit_message(context, "not a function");
            break;
    }

    state->args = NULL;

    UNDENT
}

void vm_call(struct context *context, struct variable *func, struct variable *arg, ...)
{
    // add variables from vararg
    if (arg)
    {
        va_list argp;
        va_start(argp, arg);

        struct variable *s = (struct variable*)stack_peek(context->operand_stack, 0);
        if (s && s->type == VAR_SRC)
            s = (struct variable*)stack_pop(context->operand_stack);
        else
            s = variable_new_src(context, 0);

        for (; arg; arg = va_arg(argp, struct variable*))
            array_add(s->list, arg);

        va_end(argp);
        variable_push(context, s);
    }

    vm_call_src(context, func);
}

void func_call(struct context *context, enum Opcode op,
               struct byte_array *program, struct variable *indexable)
{
    struct variable *func = context->runtime ? (struct variable*)variable_pop(context): NULL;

    struct variable *s = src(context, op, program);
    if (!context->runtime)
        return;

    if (indexable)
        array_insert(s->list, 0, indexable); // self

    vm_call_src(context, func);

    struct variable *result = (struct variable*)stack_peek(context->operand_stack, 0);
    bool resulted = (result && result->type == VAR_SRC);

    if (!resulted) { // need a result for an expression, so pretend it returned nil
        struct variable *v = variable_new_src(context, 0);
        array_add(v->list, variable_new_nil(context));
        stack_push(context->operand_stack, v);
    }
}

static void method(struct context *context, struct byte_array *program)
{
    struct variable *indexable=NULL, *index;
    if (context->runtime) {
        indexable = variable_pop(context);
        index = variable_pop(context);
        struct variable *value = lookup(context, indexable, index);
        variable_push(context, value);
    }
    func_call(context, VM_MET, program, indexable);
}

static void push_list(struct context *context, struct byte_array *program)
{
    int32_t num_items = serial_decode_int(program);
    DEBUGSPRINT(context->pcbuf, "%sLST %d%s", context->pcbuf, num_items, !context->runtime ? "\n":"");
    if (!context->runtime)
        return;

    struct variable *list = variable_new_list(context, NULL);
    while (num_items--) {
        struct variable* v = variable_pop(context);
        enum VarType vt = v->type;
        if (vt == VAR_KVP)
            variable_map_insert(context, list, v->kvp.key, v->kvp.val);
        else if (vt != VAR_NIL)
            array_insert(list->list, 0, v);
    }
#ifdef DEBUG
    char buf[VV_SIZE];
    DEBUGSPRINT(context->pcbuf, "%s: %s\n", context->pcbuf, variable_value_str(context, list, buf));
#endif
    variable_push(context, list);
}

static void push_kvp(struct context *context, struct byte_array *program)
{
    DEBUGSPRINT(context->pcbuf, "%sKVP %s", context->pcbuf, !context->runtime?"\n":"");
    if (!context->runtime)
        return;
    struct variable* val = variable_pop(context);
    struct variable* key = variable_pop(context);
    struct variable *v = variable_new_kvp(context, key, val);
#ifdef DEBUG
    char buf[VV_SIZE];
    DEBUGSPRINT(context->pcbuf, "%s: %s\n", context->pcbuf, variable_value_str(context, v, buf));
#endif
    variable_push(context, v);
}

// run /////////////////////////////////////////////////////////////////////

// get the indexed item and push on operand stack
struct variable *lookup(struct context *context, struct variable *indexable, struct variable *index)
{
#ifdef DEBUG
    char buf[VV_SIZE];
    DEBUGSPRINT(context->pcbuf, "%s%s ", context->pcbuf, variable_value_str(context, index, buf));
#endif

    struct variable *item = NULL;

    if (index->type == VAR_INT) {
        switch (indexable->type) {
            case VAR_STR:
            case VAR_FNC:
            case VAR_BYT: {
                char *str = (char*)malloc(2);
                sprintf(str, "%c", indexable->str->data[index->integer]);
                struct variable *str2 = variable_new_str_chars(context, str);
                return str2;
            } break;
            case VAR_LST:
                if (index->integer < indexable->list->length)
                    item = (struct variable*)array_get(indexable->list, index->integer);
                break;
            default:
                exit_message("bad lookup type");
                return NULL;
        }
    }
    if ((NULL == item) && (NULL != indexable->map))
        item = map_get(indexable->map, index);
    if ((NULL == item) && (index->type == VAR_STR))
        item = builtin_method(context, indexable, index);
    if (NULL == item )
        item = variable_new_nil(context);
    return item;
}

static void list_get(struct context *context)
{
    DEBUGSPRINT(context->pcbuf, "%sGET %s", context->pcbuf, !context->runtime?"\n":"");
    if (!context->runtime)
        return;
    struct variable *indexable, *index;
    indexable = variable_pop(context);
    index = variable_pop(context);
    struct variable *value = lookup(context, indexable, index);
    variable_push(context, value);
    DEBUGSPRINT(context->pcbuf, "%s\n", context->pcbuf);
}

static int32_t jump(struct context *context, struct byte_array *program)
{
    null_check(program);
    uint8_t *start = program->current;
    int32_t offset = serial_decode_int(program);
    DEBUGSPRINT(context->pcbuf, "%sJMP %d\n", context->pcbuf, offset);
    if (!context->runtime)
        return 0;

    if (offset < 0) // skip over current VM_JMP instruction when going backward
        offset -= (program->current - start) + 1;
    return offset;// - (program->current - start);
}

bool test_operand(struct context *context)
{
    struct variable* v = variable_pop(context);
    bool indeed = false;
    switch (v->type) {
        case VAR_NIL:   indeed = false;                     break;
        case VAR_BOOL:  indeed = v->boolean;                break;
        case VAR_INT:   indeed = v->integer;                break;
        case VAR_FLT:   indeed = v->floater;                break;
        default:        indeed = true;                      break;
    }
    return indeed;
}

static int32_t iff(struct context *context, struct byte_array *program)
{
    null_check(program);
    int32_t offset = serial_decode_int(program);
    DEBUGSPRINT(context->pcbuf, "%sIF %d\n", context->pcbuf, offset);
    if (!context->runtime)
        return 0;
    return test_operand(context) ? 0 : (VOID_INT)offset;
}

static void push_nil(struct context *context)
{
    struct variable* var = variable_new_nil(context);
    DEBUGSPRINT(context->pcbuf, "%sNIL\n", context->pcbuf);
    if (!context->runtime)
        return;
    variable_push(context, var);
}

static void push_int(struct context *context, struct byte_array *program)
{
    null_check(program);
    int32_t num = serial_decode_int(program);
    DEBUGSPRINT(context->pcbuf, "%sINT %d\n", context->pcbuf, num);
    if (!context->runtime)
        return;
    struct variable* var = variable_new_int(context, num);
    variable_push(context, var);
}

static void push_bool(struct context *context, struct byte_array *program)
{
    null_check(program);
    int32_t num = serial_decode_int(program);
    DEBUGSPRINT(context->pcbuf, "%sBOOL %d\n", context->pcbuf, num);
    if (!context->runtime)
        return;
    struct variable* var = variable_new_bool(context, num);
    variable_push(context, var);
}

static void push_float(struct context *context, struct byte_array *program)
{
    null_check(program);
    float num = serial_decode_float(program);
    DEBUGSPRINT(context->pcbuf, "%sFLT %f\n", context->pcbuf, num);
    if (!context->runtime)
        return;
    struct variable* var = variable_new_float(context, num);
    variable_push(context, var);
}

struct variable *find_var(struct context *context, struct variable *key)
{
    null_check(key);

    const struct program_state *state = (const struct program_state*)stack_peek(context->program_stack, 0);
    if (NULL == state)
        return NULL;
    struct map *var_map = state->named_variables;
    struct variable *v = (struct variable*)map_get(var_map, key);

    if ((NULL == v) && !strncmp(RESERVED_SYS, (const char*)key->str->data, strlen(RESERVED_SYS))) {
        v = context->sys;
    }

    if ((NULL == v) && context->singleton->callback)
        v = variable_map_get(context, context->singleton->callback, key);

    return v;
}

static void push_var(struct context *context, struct byte_array *program)
{
    struct byte_array* name = serial_decode_string(program);
#ifdef DEBUG
//    if (!context->runtime) {
        char *str = byte_array_to_string(name);
        DEBUGSPRINT(context->pcbuf, "%sVAR %s", context->pcbuf, str);
        free(str);
//    } else {
//        DEBUGSPRINT(context->pcbuf, "%sVAR ", context->pcbuf);
//    }
    if (!context->runtime)
        return;
#endif // DEBUG
    struct variable *key = variable_new_str(context, name);
    struct variable *v = find_var(context, key);
    variable_push(context, v);
    byte_array_del(name);
    DEBUGSPRINT(context->pcbuf, "%s\n", context->pcbuf);
}

static void push_str(struct context *context, struct byte_array *program)
{
    struct byte_array* str = serial_decode_string(program);
#ifdef DEBUG
    char *str2 = byte_array_to_string(str);
    DEBUGSPRINT(context->pcbuf, "%sSTR %s\n", context->pcbuf, str2);
    free(str2);
    if (!context->runtime)
        return;
#endif // DEBUG
    struct variable* v = variable_new_str(context, str);
    byte_array_del(str);
    variable_push(context, v);
}

static void push_fnc(struct context *context, struct byte_array *program)
{
    uint32_t num_closures = serial_decode_int(program);
    struct variable *closures = NULL;

    for (int i=0; i<num_closures; i++) {
        struct byte_array *name = serial_decode_string(program);
        struct variable *key = variable_new_str(context, name);
        byte_array_del(name);
        if (context->runtime) {
            if (closures == NULL)
                closures = variable_new_list(context, NULL);
            struct variable *c = find_var(context, key);
            //c = variable_copy(context, c);
            variable_map_insert(context, closures, key, c);
        }
    }

    struct byte_array *body = serial_decode_string(program);

    DEBUGSPRINT(context->pcbuf, "%sFNC %u,%u\n", context->pcbuf, num_closures, body->length);
    //display_code(context, body);

    if (context->runtime) {
        struct variable *f = variable_new_fnc(context, body, closures);
        variable_push(context, f);
    }
    byte_array_del(body);
}

void set_named_variable(struct context *context,
                        struct program_state *state,
                        struct byte_array *name,
                        struct variable *value)
{
    //DEBUGPRINT(" set_named_variable: %p\n", state);
    if (state == NULL)
        state = (struct program_state*)stack_peek(context->program_stack, 0);
    struct map *var_map = state->named_variables;
    struct variable *name2 = variable_new_str(context, name);
    map_insert(var_map, name2, value);

    //DEBUGPRINT("SET %s to %s\n", byte_array_to_string(name), variable_value_str(context, value));
    // DEBUGPRINT(" SET %s at %p in {p:%p, s:%p, m:%p}\n", byte_array_to_string(name), to_var, context->program_stack, state, var_map);
}

static struct variable *get_value(struct context *context, enum Opcode op)
{
    bool interim = op == VM_STX || op == VM_PTX;
    struct variable *value = stack_peek(context->operand_stack, 0);
    null_check(value);

    if (value->type == VAR_SRC) {
        struct array *values = value->list;
        struct map *kvps = value->map;
        bool listvar = values->length > values->current - values->data;
        if (listvar)
            value = (struct variable*)*values->current++;
        else if (NULL != kvps) {
            value = variable_new_list(context, NULL);
            value->map = map_copy(context, kvps);
        } else
            value = variable_new_nil(context);
    }
    else if (!interim)
        variable_pop(context);

    return value;
}

static void set(struct context *context,
                enum Opcode op,
                struct program_state *state,
                struct byte_array *program)
{
    struct byte_array *name = serial_decode_string(program);    // destination variable name
    if (!context->runtime) {
        //        VM_DEBUGPRINT("%s %s\n", op==VM_SET?"SET":"STX", byte_array_to_string(name));
#ifdef DEBUG
        char *str = byte_array_to_string(name);
        DEBUGSPRINT(context->pcbuf, "%s%s %s\n", context->pcbuf, op==VM_SET?"SET":"STX", str);
        free(str);
        if (!context->runtime)
            return;
#endif // DEBUG
    }

    struct variable *value = get_value(context, op);

#ifdef DEBUG
    char *str = byte_array_to_string(name);
    char buf[VV_SIZE];
    DEBUGSPRINT(context->pcbuf, "%s%s %s to %s\n",
            context->pcbuf,
            op==VM_SET ? "SET" : "STX",
            str,
            variable_value_str(context, value, buf));
    free(str);
    if (!context->runtime)
        return;
#endif // DEBUG

    set_named_variable(context, state, name, value); // set the variable to the value
    byte_array_del(name);
}

static void dst(struct context *context) // drop unused assignment right-hand-side values
{
    DEBUGSPRINT(context->pcbuf, "%sDST%s", context->pcbuf, !context->runtime ? " (not runtime)":"");
    if (!context->runtime)
        return;
    if (stack_empty(context->operand_stack)) {
        DEBUGSPRINT(context->pcbuf, "%s %p mt\n", context->pcbuf, context->operand_stack);
        return;
    }

    struct variable *v = (struct variable*)stack_peek(context->operand_stack, 0);
    if (v->type == VAR_SRC) // unused result
        stack_pop(context->operand_stack);
    else
        DEBUGSPRINT(context->pcbuf, "%s (%s)", context->pcbuf, var_type_str(v->type));
    DEBUGSPRINT(context->pcbuf, "%s\n", context->pcbuf);

    garbage_collect(context);
}

static void list_put(struct context *context, enum Opcode op)
{
    DEBUGSPRINT(context->pcbuf, "%sPUT %s", context->pcbuf, !context->runtime?"\n":"");
    if (!context->runtime)
        return;
    struct variable* recipient = variable_pop(context);
    struct variable* key = variable_pop(context);
    struct variable *value = get_value(context, op);
    DEBUGSPRINT(context->pcbuf, "%s\n", context->pcbuf);

    switch (key->type) {
        case VAR_INT:
            switch (recipient->type) {
                case VAR_LST:
                    array_set(recipient->list, key->integer, value);
                    break;
                case VAR_STR:
                case VAR_BYT:
                    byte_array_set(recipient->str, key->integer, value->integer);
                    break;
                default:
                    vm_exit_message(context, "indexing non-indexable");
            } break;
        case VAR_STR:
            variable_map_insert(context, recipient, key, value);
            break;
        default:
            vm_exit_message(context, "bad index type");
            break;
    }
}

static struct variable *binary_op_int(struct context *context,
                                      enum Opcode op,
                                      const struct variable *u,
                                      const struct variable *v)
{
    int32_t m = u->type == VAR_INT ? u->integer : u->boolean;
    int32_t n = v->type == VAR_INT ? v->integer : v->boolean;
    int32_t i;
    switch (op) {
        case VM_MUL:    i = m * n;    break;
        case VM_DIV:    i = m / n;    break;
        case VM_ADD:    i = m + n;    break;
        case VM_SUB:    i = m - n;    break;
        case VM_EQU:    i = m == n;   break;
        case VM_NEQ:    i = m != n;   break;
        case VM_GTN:    i = m > n;    break;
        case VM_LTN:    i = m < n;    break;
        case VM_GRQ:    i = m >= n;   break;
        case VM_LEQ:    i = m <= n;   break;
        case VM_BND:    i = m & n;    break;
        case VM_BOR:    i = m | n;    break;
        case VM_MOD:    i = m % n;    break;
        case VM_XOR:    i = m ^ n;    break;
        case VM_RSF:    i = m >> n;   break;
        case VM_LSF:    i = m << n;   break;

        default:
            return (struct variable*)vm_exit_message(context, "bad math int operator");
    }
    return variable_new_int(context, i);
}

static struct variable *binary_op_flt(struct context *context,
                                      enum Opcode op,
                                      const struct variable *u,
                                      const struct variable *v)
{
    float m = u->floater;
    float n;
    switch (v->type) {
        case VAR_FLT:   n = v->floater;         break;
        case VAR_INT:   n = (float)v->integer;  break;
        default:
            exit_message("can't compare with float");
	    return NULL;
    }
    float f = 0;
    switch (op) {
        case VM_MUL:    f = m * n;                                  break;
        case VM_DIV:    f = m / n;                                  break;
        case VM_ADD:    f = m + n;                                  break;
        case VM_SUB:    f = m - n;                                  break;
        case VM_NEQ:    f = m != n;                                 break;
        case VM_GTN:    return variable_new_int(context, m > n);
        case VM_LTN:    return variable_new_int(context, m < n);
        case VM_GRQ:    return variable_new_int(context, m >= n);
        case VM_LEQ:    return variable_new_int(context, m <= n);
        default:
            return (struct variable*)vm_exit_message(context, "bad math float operator");
    }
    return variable_new_float(context, f);
}

static bool is_num(enum VarType vt) {
    return vt == VAR_INT || vt == VAR_FLT;
}

static struct variable *binary_op_str(struct context *context,
                                      enum Opcode op,
                                      struct variable *u,
                                      struct variable *v)
{
    struct variable *w = NULL;
    enum VarType ut = u->type, vt = v->type;
    struct byte_array *ustr = ut == VAR_STR ? u->str : variable_value(context, u);
    struct byte_array *vstr = vt == VAR_STR ? v->str : variable_value(context, v);

    switch (op) {
        case VM_ADD: {
            struct byte_array *wstr = byte_array_concatenate(2, ustr, vstr);
            w = variable_new_str(context, wstr);
            byte_array_del(wstr);
        } break;
        case VM_EQU:
            w = variable_new_int(context, byte_array_equals(ustr, vstr));
            break;
        case VM_SUB: {
            struct byte_array *nada = byte_array_from_string("");
            struct byte_array *wstr = byte_array_replace_all(ustr, vstr, nada);
            w = variable_new_str(context, wstr);
        } break;
        default:
            w = (struct variable*)vm_exit_message(context, "unknown string operation");
            break;
    }

    if (ut != VAR_STR)
        byte_array_del(ustr);
    if (vt != VAR_STR)
        byte_array_del(vstr);
    return w;
}

static struct variable *binary_op_map(struct context *context,
                                      enum Opcode op,
                                      struct variable *u,
                                      struct variable *v)
{
    assert_message(u->type == VAR_KVP && v->type == VAR_KVP && op == VM_ADD, "bad map op");
    struct array *list = array_new_size(2);
    array_add(list, u);
    array_add(list, v);
    return variable_new_list(context, list);
}

static void variable_purge(struct context *context, struct variable *v, struct variable *p)
{
    struct variable *position = variable_find(context, v, p, NULL);
    if (position->type != VAR_NIL)
        array_remove(v->list, position->integer, 1);
    else if (NULL != v->map)
        map_remove(v->map, p);
}

static struct variable *binary_op_lst(struct context *context,
                                      enum Opcode op,
                                      struct variable *u,
                                      struct variable *v)
{
    vm_assert(context, u->type==VAR_LST || v->type==VAR_LST, "list op with non-lists");
    enum VarType vt = v->type;

    struct variable *w = variable_copy(context, u);

    switch (op) {
        case VM_ADD:
            if (vt == VAR_LST) {
                for (int i=0; i<v->list->length; i++)
                    array_add(w->list, array_get(v->list, i));
            } else if (vt == VAR_KVP) {
                variable_map_insert(context, w, v->kvp.key, v->kvp.val);
            } else if (vt != VAR_NIL) {
                array_add(w->list, (void*)v);
            }
            w->map = map_union(w->map, v->map);
            break;
        case VM_SUB:
            if (vt == VAR_LST) {
                for (int i=0; i<v->list->length; i++) {
                    struct variable *item = (struct variable*)array_get(v->list, i);
                    variable_purge(context, w, item);
                }
            } else {
                variable_purge(context, w, v);
            }
            w->map = map_minus(w->map, v->map);
            break;
        default:
            return (struct variable*)vm_exit_message(context, "unknown list operation");
    }

    return w;
}

static struct variable *binary_op_nil(struct context *context,
                                      enum Opcode op,
                                      const struct variable *u,
                                      const struct variable *v)
{
    vm_assert(context, u->type==VAR_NIL || v->type==VAR_NIL, "nil op with non-nils");
    if (v->type == VAR_NIL && u->type != VAR_NIL)
        return binary_op_nil(context, op, v, u); // 1st var should be nil

    switch (op) {
        case VM_EQU:    return variable_new_bool(context, v->type == u->type);
        case VM_NEQ:    return variable_new_bool(context, v->type != u->type);
        case VM_ADD:
        case VM_SUB:    return variable_copy(context, v);
        case VM_MUL:    return variable_new_nil(context);
        case VM_LTN:
        case VM_LEQ:    return variable_new_bool(context, true);
        case VM_GTN:
        case VM_GRQ:    return variable_new_bool(context, false);
        default:
            return vm_exit_message(context, "unknown binary nil op");
    }
}

static int32_t boolean_op(struct context *context, struct byte_array *program, enum Opcode op)
{
    null_check(program);
    int32_t short_circuit = serial_decode_int(program);

    DEBUGSPRINT(context->pcbuf, "%s%s %d\n", context->pcbuf, NUM_TO_STRING(opcodes, op), short_circuit);
    if (!context->runtime)
        return 0;
    struct variable *v = variable_pop(context);
    null_check(v);
    bool indeed_quite_so;
    switch (v->type) {
        case VAR_BOOL:  indeed_quite_so = v->boolean;   break;
        case VAR_FLT:   indeed_quite_so = v->floater;   break;
        case VAR_INT:   indeed_quite_so = v->integer;   break;
        case VAR_NIL:   indeed_quite_so = false;        break;
        default:        indeed_quite_so = true;         break;
    }
    if (indeed_quite_so ^ (op == VM_AND)) {
        stack_push(context->operand_stack, v);
        return short_circuit;
    }
    return 0;
}

static void binary_op(struct context *context, enum Opcode op)
{
    if (!context->runtime) {
        DEBUGSPRINT(context->pcbuf, "%s%s\n", context->pcbuf, NUM_TO_STRING(opcodes, op));
        return;
    }

    struct variable *v = variable_pop(context);
    struct variable *u = variable_pop(context);

    enum VarType ut = (enum VarType)u->type;
    enum VarType vt = (enum VarType)v->type;
    struct variable *w = NULL;

    if ((op == VM_EQU) || (op == VM_NEQ)) {
        bool same = variable_compare(context, u, v) ^ (op == VM_NEQ);
        w = variable_new_bool(context, same);
    } else if (ut == VAR_LST) {
        w = binary_op_lst(context, op, u, v);
    } else if (ut == VAR_NIL || vt == VAR_NIL) {
        w = binary_op_nil(context, op, u, v);
    } else if (vt == VAR_KVP && ut == VAR_KVP) {
        w = binary_op_map(context, op, u, v);
    } else if (ut == VAR_STR || vt == VAR_STR) {
        w = binary_op_str(context, op, u, v);
    } else {
        bool floater = (ut == VAR_FLT && is_num(vt)) || (vt == VAR_FLT && is_num(ut));
        bool inter = (ut==VAR_INT || ut==VAR_BOOL) && (vt==VAR_INT || vt==VAR_BOOL);

        if (floater)                                w = binary_op_flt(context, op, u, v);
        else if (inter)                             w = binary_op_int(context, op, u, v);
        else
            vm_exit_message(context, "unknown binary op");
    }

    variable_push(context, w);
#ifdef DEBUG
    char bufu[VV_SIZE], bufv[VV_SIZE], bufw[VV_SIZE];
    DEBUGSPRINT(context->pcbuf, "%s%s(%s,%s) = %s\n",
            context->pcbuf,
               NUM_TO_STRING(opcodes, op),
               variable_value_str(context, u, bufu),
               variable_value_str(context, v, bufv),
               variable_value_str(context, w, bufw));
#endif
}

static void unary_op(struct context *context, enum Opcode op)
{
    if (!context->runtime) {
        DEBUGSPRINT(context->pcbuf, "%s%s\n", context->pcbuf, NUM_TO_STRING(opcodes, op));
        return;
    }

    struct variable *v = (struct variable*)variable_pop(context);
    struct variable *result = NULL;

    switch (v->type) {
        case VAR_NIL:
        {
            switch (op) {
                case VM_NEG:    result = variable_new_nil(context);              break;
                case VM_NOT:    result = variable_new_bool(context, true);       break;
                default:        vm_exit_message(context, "bad math operator");   break;
            }
        } break;
        case VAR_INT: {
            int32_t n = v->integer;
            switch (op) {
                case VM_NEG:    result = variable_new_int(context, -n);          break;
                case VM_NOT:    result = variable_new_bool(context, !n);         break;
                case VM_INV:    result = variable_new_int(context, ~n);          break;
                default:        vm_exit_message(context, "bad math operator");   break;
            }
        } break;
        case VAR_FLT: {
            float n = v->floater;
            switch (op) {
                case VM_NEG:    result = variable_new_float(context, -n);        break;
                case VM_NOT:    result = variable_new_bool(context, !n);         break;
                default:        vm_exit_message(context, "bad math operator");   break;
            }
        } break;
        case VAR_BOOL:
            switch (op) {
                case VM_NOT:    result = variable_new_bool(context, !v->boolean);break;
                default:        vm_exit_message(context, "bad math operator");   break;
            }
            break;
        default:
            if (op == VM_NOT)
                result = variable_new_bool(context, false);
            else
                vm_exit_message(context, "bad math type");
            break;
    }

    variable_push(context, result);
#ifdef DEBUG
    char buf1[VV_SIZE], buf2[VV_SIZE];
    DEBUGSPRINT(context->pcbuf, "%s%s(%s) = %s\n",
            context->pcbuf,
            NUM_TO_STRING(opcodes, op),
            variable_value_str(context, v, buf1),
            variable_value_str(context, result, buf2));
#endif
}

// FOR who IN what WHERE where DO how
static bool iterate(struct context *context,
                    enum Opcode op,
                    struct program_state *state,
                    struct byte_array *program)
{
    bool returned = false;

    struct byte_array *who = serial_decode_string(program);
    struct byte_array *where = serial_decode_string(program);
    struct byte_array *how = serial_decode_string(program);

#ifdef DEBUG
    char *str = byte_array_to_string(who);
    DEBUGSPRINT(context->pcbuf, "%s%s %s\n",
            context->pcbuf,
            NUM_TO_STRING(opcodes, op),
            str);
    free(str);
    if (!context->runtime) {
        if (where && where->length) {
            DEBUGSPRINT(context->pcbuf, "%s%s\tWHERE %d\n", context->pcbuf, indentation(context), where->length);
            //display_code(context, where);
        }
        DEBUGSPRINT(context->pcbuf, "%s%s\tDO %d\n", context->pcbuf, indentation(context), how->length);
        //display_code(context, how);
        goto done;
    }
#endif

    bool comprehending = (op == VM_COM);
    struct variable *result = comprehending ? variable_new_list(context, NULL) : NULL;

    struct variable *what = variable_pop(context);
    assert_message(what->type == VAR_LST, "iterating over non-list");
    struct array *list = what->list;
    if (!list->length && what->map)
        list = map_keys(what->map);
    uint32_t len = list->length;

    for (int i=0; i<len; i++) {

        INDENT;
        struct variable *that = (struct variable*)array_get(list, i);
        set_named_variable(context, state, who, that);

        byte_array_reset(where);
        byte_array_reset(how);
        if (where && where->length)
            run(context, where, NULL, true);
        if ((where == NULL) || !where->length || test_operand(context)) {

            DEBUGPRINT("\n");
            INDENT;
            if (run(context, how, NULL, true)) { // true if run hit VM_RET
                returned = true;
                UNDENT; UNDENT;
                goto done;
            }

            if (comprehending) {
                struct variable *item = (struct variable*)stack_pop(context->operand_stack);
                if (item->type == VAR_KVP)
                    variable_map_insert(context, result, item->kvp.key, item->kvp.val);
                else
                    array_add(result->list, item);
            }
            UNDENT;
        }
        UNDENT;
    }

    if (comprehending)
        stack_push(context->operand_stack, result);

done:
    byte_array_del(who);
    byte_array_del(where);
    byte_array_del(how);
    return returned;
}

static inline bool vm_trycatch(struct context *context, struct byte_array *program)
{
    bool returned = false;
    struct byte_array *trial = serial_decode_string(program);
    DEBUGSPRINT(context->pcbuf, "%sTRY %d\n", context->pcbuf, trial->length);
    //display_code(context, trial);
    struct byte_array *name = serial_decode_string(program);
    struct byte_array *catcher = serial_decode_string(program);
#ifdef DEBUG
    char *str = byte_array_to_string(name);
    DEBUGSPRINT(context->pcbuf, "%s%sCATCH %s %d\n", context->pcbuf, indentation(context), str, catcher->length);
    free(str);
#endif
    //display_code(context, catcher);
    if (!context->runtime)
        goto done;

    run(context, trial, NULL, true);
    if (context->error) {
        set_named_variable(context, NULL, name, context->error);
        context->error = NULL;
        returned = run(context, catcher, NULL, true);
    }
done:
    byte_array_del(name);
    byte_array_del(trial);
    byte_array_del(catcher);
    return returned;
}

static inline bool ret(struct context *context, struct byte_array *program)
{
    src(context, VM_RET, program);
    return context->runtime;
}

static inline bool tro(struct context *context)
{
    DEBUGSPRINT(context->pcbuf, "%sTHROW\n", context->pcbuf);
    if (!context->runtime)
        return false;
    context->error = (struct variable*)stack_pop(context->operand_stack);
    return true;
}

bool run(struct context *context,
         struct byte_array *program,
         struct map *env,
         bool in_state)
{
    bool local_state = false;
    null_check(context);
    null_check(program);
    program = byte_array_copy(program);
    program->current = program->data;
    struct program_state *state = NULL;
    enum Opcode inst = VM_NIL;

    if (context->runtime)
    {
        if (in_state)
        {
            if (state == NULL)
                state = (struct program_state*)stack_peek(context->program_stack, 0);
            env = state->named_variables; // use the caller's variable set in the new state
        }
        else // new state on program stack
        {
            local_state = true;
            state = program_state_new(context, env);
        }
    }

    while (program->current < program->data + program->length)
    {
        if (context->singleton->tick++ > GIL_SWITCH)
        {
            context->singleton->tick = 0;
            gil_unlock(context, "run");
            gil_lock(context, "run");
        }

        inst = (enum Opcode)*program->current;

#ifdef DEBUG
        context->pcbuf[0] = 0;
        display_program_counter(context, program);
#endif
        program->current++; // increment past the instruction
        int32_t pc_offset = 0;

        switch (inst) {
            case VM_COM:
            case VM_ITR:    if (iterate(context, inst, state, program)) goto done;  break;
            case VM_RET:    if (ret(context, program))                  goto done;  break;
            case VM_TRO:    if (tro(context))                           goto done;  break;
            case VM_TRY:    if (vm_trycatch(context, program))          goto done;  break;
            case VM_MUL:
            case VM_EQU:
            case VM_DIV:
            case VM_ADD:
            case VM_SUB:
            case VM_NEQ:
            case VM_GTN:
            case VM_LTN:
            case VM_GRQ:
            case VM_LEQ:
            case VM_BND:
            case VM_BOR:
            case VM_MOD:
            case VM_XOR:
            case VM_INV:
            case VM_RSF:
            case VM_LSF:    binary_op(context, inst);                       break;
            case VM_ORR:
            case VM_AND:    pc_offset = boolean_op(context, program, inst); break;
            case VM_NEG:
            case VM_NOT:    unary_op(context, inst);                        break;
            case VM_SRC:    src(context, inst, program);                    break;
            case VM_DST:    dst(context);                                   break;
            case VM_STX:
            case VM_SET:    set(context, inst, state, program);             break;
            case VM_JMP:    pc_offset = jump(context, program);             break;
            case VM_IFF:    pc_offset = iff(context, program);              break;
            case VM_CAL:    func_call(context, inst, program, NULL);        break;
            case VM_LST:    push_list(context, program);                    break;
            case VM_KVP:    push_kvp(context, program);                     break;
            case VM_NIL:    push_nil(context);                              break;
            case VM_INT:    push_int(context, program);                     break;
            case VM_FLT:    push_float(context, program);                   break;
            case VM_BUL:    push_bool(context, program);                    break;
            case VM_STR:    push_str(context, program);                     break;
            case VM_VAR:    push_var(context, program);                     break;
            case VM_FNC:    push_fnc(context, program);                     break;
            case VM_GET:    list_get(context);                              break;
            case VM_PTX:
            case VM_PUT:    list_put(context, inst);                        break;
            case VM_MET:    method(context, program);                       break;
            default:
                vm_exit_message(context, ERROR_OPCODE);
                break;
        }
        program->current += pc_offset;
        DEBUGPRINT("%s", context->pcbuf);
        
    } // while

    byte_array_del(program);
    program = NULL;

    if (!context->runtime)
        return false;
done:
    if (!in_state)
        stack_pop(context->program_stack);
    if (local_state)
        program_state_del(context, state);
    if (NULL != program)
        byte_array_del(program);
    garbage_collect(context);
    return inst == VM_RET;
}

void execute_with(struct context *context, struct byte_array *program, bool in_state)
{
    DEBUGPRINT("execute:\n");
    
    null_check(program);
    program = byte_array_copy(program);
    byte_array_reset(program);

#ifdef DEBUG
    context->indent = 1;
#endif
    if (!setjmp(trying))
        run(context, program, NULL, in_state);

    if (context->error)
        DEBUGPRINT("error: %s\n", context->error->str->data);
    if (!stack_empty(context->operand_stack))
        DEBUGPRINT("warning: operand stack not empty\n");
    gil_unlock(context, "execute");
    byte_array_del(program);
}

void execute(struct byte_array *program)
{
    struct context *context = context_new(NULL, true, true);
    execute_with(context, program, false);
    context_del(context);
}