#include <string.h>
#include "vm.h"
#include "util.h"
#include "struct.h"
#include "serial.h"
#include "variable.h"
#include "node.h"

extern void mark_map(struct map *map, bool mark);
static void variable_value2(struct context *context, struct variable* v, struct byte_array *buf);
static struct variable *variable_deserialize2(struct context *context, struct byte_array *bits);

#define ERROR_VAR_TYPE  "type error"

const struct number_string var_types[] = {
    {VAR_NIL,   "nil"},
    {VAR_INT,   "integer"},
    {VAR_FLT,   "float"},
    {VAR_STR,   "string"},
    {VAR_KVP,   "key-value-pair"},
    {VAR_LST,   "list"},
    {VAR_FNC,   "function"},
    {VAR_ERR,   "error"},
    {VAR_SRC,   "source"},
    {VAR_BOOL,  "boolean"},
    {VAR_CFNC,  "c-function"},
    {VAR_VOID,  "void"},
};

const char *var_type_str(enum VarType vt)
{
    return NUM_TO_STRING(var_types, vt);
}

struct variable* variable_new(struct context *context, enum VarType type)
{
    struct variable* v = (struct variable*)malloc(sizeof(struct variable));
    //DEBUGPRINT("\n>%" PRIu16 " - variable_new %p %s\n", current_thread_id(), v, var_type_str(type));

    v->type = type;
    v->mark = 0;
    v->ptr = NULL;
    v->visited = VISITED_NOT;
    v->gc_state = GC_NEW;

    struct array *oldarray = context->singleton->all_variables;

    array_add(context->singleton->all_variables, v);

    struct array *newarray = context->singleton->all_variables;
    if (oldarray != newarray)
        DEBUGPRINT("all_variables %p -> %p\n", oldarray, newarray);

    //DEBUGPRINT("variable_new %s %p\n", var_type_str(type), v);
    return v;
}

struct variable *variable_new_void(struct context *context, void *p)
{
    struct variable *v = variable_new(context, VAR_VOID);
    v->ptr = p;
    return v;
}

struct variable* variable_new_nil(struct context *context) {
    return variable_new(context, VAR_NIL);
}

struct variable* variable_new_int(struct context *context, int32_t i)
{
    struct variable *v = variable_new(context, VAR_INT);
    v->integer = i;
    return v;
}

struct variable* variable_new_err(struct context *context, const char* message)
{
    struct variable *v = variable_new(context, VAR_ERR);
    v->str = byte_array_from_string(message);
    return v;
}

struct variable* variable_new_bool(struct context *context, bool b)
{
    struct variable *v = variable_new(context, VAR_BOOL);
    v->boolean = b;
    return v;
}

void variable_old(struct variable *v)
{
    if (v->gc_state != GC_SAFE)
        v->gc_state = GC_OLD;
}

void variable_del(struct context *context, struct variable *v)
{
    if (v->gc_state != GC_OLD) { DEBUGPRINT("killing an orphan"); }
    //DEBUGPRINT("\n>%" PRIu16 " - variable_del %p %s\n", current_thread_id(), v, var_type_str(v->type));
    switch (v->type) {
        case VAR_CFNC:
        case VAR_NIL:
        case VAR_INT:
        case VAR_FLT:
        case VAR_KVP:
        case VAR_BOOL:
            break;
        case VAR_SRC:
        case VAR_LST:
            array_del(v->list.ordered);
            if (NULL != v->list.map)
                map_del(v->list.map);
            break;
        case VAR_STR:
        case VAR_FNC:
            byte_array_del(v->str);
            break;
        case VAR_VOID: // todo
            break;
        default:
            vm_exit_message(context, "bad var type");
            break;
    }

    free(v);
}

struct variable *variable_new_src(struct context *context, uint32_t size)
{
    struct variable *v = variable_new(context, VAR_SRC);
    v->list.ordered = array_new();
    v->list.map = NULL;

    while (size--) {
        struct variable *o = (struct variable*)stack_pop(context->operand_stack);
        if (o->type == VAR_SRC) {
            o->list.map = map_union(o->list.map, v->list.map);
            array_append(o->list.ordered, v->list.ordered);
            v = o;
        } else if (o->type == VAR_KVP) // then put o's kvp into v's map
            variable_map_insert(context, v, o->kvp.key, o->kvp.val);
        else
            array_insert(v->list.ordered, 0, o);
    }
    v->list.ordered->current = v->list.ordered->data;
    return v;
}

struct variable *variable_new_bytes(struct context *context, struct byte_array *bytes, uint32_t size)
{
    struct variable *v = variable_new(context, VAR_BYT);
    v->str = bytes ? bytes : byte_array_new_size(size);
    return v;
}

struct variable* variable_new_float(struct context *context, float f)
{
    //DEBUGPRINT("new float %f\n", f);
    struct variable *v = variable_new(context, VAR_FLT);
    v->floater = f;
    return v;
}

struct variable *variable_new_str(struct context *context, struct byte_array *str) {
    struct variable *v = variable_new(context, VAR_STR);
    v->str = str ? byte_array_copy(str) : byte_array_new();
    //DEBUGPRINT("variable_new_str %p->%s\n", v, byte_array_to_string(str));
    return v;
}

struct variable *variable_new_str_chars(struct context *context, const char *str)
{
    struct byte_array *str2 = byte_array_from_string(str);
    return variable_new_str(context, str2);
}

struct variable *variable_new_fnc(struct context *context, struct byte_array *body, struct variable *closure)
{
    //DEBUGPRINT("variable_new_fnc %s\n", buf);

    struct variable *v = variable_new(context, VAR_FNC);
    v->fnc.body = byte_array_copy(body);
    if (NULL != closure)
        v->fnc.closure = map_copy(context, closure->list.map);
    else
        v->fnc.closure = NULL;

    return v;
}

struct variable *variable_new_list(struct context *context, struct array *list)
{
    struct variable *v = variable_new(context, VAR_LST);
    v->list.ordered = array_new();
    //DEBUGPRINT("variable_new_list %p->%p\n", v, v->list.ordered);
    for (uint32_t i=0; list && (i<list->length); i++) {
        struct variable *u = (struct variable*)array_get(list, i);
        if (u->type == VAR_KVP) {
            v->list.map = map_union(v->list.map, u->list.map);
        } else
            array_set(v->list.ordered, v->list.ordered->length, u);
    }
    v->list.map = NULL;
    return v;
}

struct variable *variable_new_kvp(struct context *context, struct variable *key, struct variable *val)
{
    struct variable *v = variable_new(context, VAR_KVP);
    v->kvp.key = key;
    v->kvp.val = val;
    return v;
}

struct variable *variable_new_cfnc(struct context *context, callback2func *cfnc) {
    struct variable *v = variable_new(context, VAR_CFNC);
    v->cfnc.f = cfnc;
    v->cfnc.data = NULL;
    return v;
}

static void variable_value2(struct context *context, struct variable* v, struct byte_array *buf)
{
    assert_message(v && (v->visited < VISITED_LAST), "corrupt variable");
    null_check(v);
    struct map *map = NULL;

    enum VarType vt = (enum VarType)v->type;

    //printf("vt=%d\n", vt);

    if (v->visited ==VISITED_MORE) { // first visit of reused variable
        byte_array_format(buf, true, "&%d", v->mark);
        v->visited = VISITED_REPEAT;
    }
    else if (v->visited == VISITED_REPEAT) { // subsequent visit
        byte_array_format(buf, true, "*%d", v->mark);
        return;
    }

    switch (vt) {
        case VAR_NIL:    byte_array_format(buf, true, "nil");                               break;
        case VAR_INT:    byte_array_format(buf, true, "%d", v->integer);                    break;
        case VAR_BOOL:   byte_array_format(buf, true, "%s", v->boolean ? "true" : "false"); break;
        case VAR_FLT:    byte_array_format(buf, true, "%f", v->floater);                    break;
        case VAR_FNC:
            byte_array_format(buf, true, "f(%dB)", v->fnc.body->length);
            map = v->fnc.closure;
            break;
        case VAR_CFNC:   byte_array_format(buf, true, "c-fnc");                             break;
        case VAR_VOID:   byte_array_format(buf, true, "%p", v->ptr);                        break;
        case VAR_BYT:
            byte_array_print((char*)buf->current, buf->size - buf->length, v->str);
            break;
        case VAR_KVP:
            variable_value2(context, v->kvp.key, buf);
            byte_array_format(buf, true, ":");
            variable_value2(context, v->kvp.val, buf);
            break;
        case VAR_SRC:
        case VAR_LST: {
            struct array* list = v->list.ordered;
            byte_array_format(buf, true, "[");
            vm_null_check(context, list);
            for (int i=0; i<list->length; i++) {
                struct variable* element = (struct variable*)array_get(list, i);
                byte_array_format(buf, true, "%s", i ? "," : "");
                if (NULL != element)
                    variable_value2(context, element, buf);
            }
            map = v->list.map;
        } break;
        case VAR_STR: {
            byte_array_format(buf, true, "'");
            byte_array_append(buf, v->str);
            byte_array_format(buf, true, "'");
        } break;
        case VAR_ERR: {
            byte_array_append(buf, v->str);
        } break;
        default:
            vm_exit_message(context, ERROR_VAR_TYPE);
            break;
    }

    if (NULL != map)
    {
        struct array *keys = map_keys(map);
        struct array *vals = map_vals(map);

        for (int i=0; i<keys->length; i++)
        {
            if (v->list.ordered->length + i)
                byte_array_format(buf, true, ",");

            struct variable *key = (struct variable *)array_get(keys, i);
            struct variable *val = (struct variable *)array_get(vals, i);
            struct variable *kvp = variable_new_kvp(context, key, val);
            variable_value2(context, kvp, buf);
            kvp->gc_state = GC_OLD;

        } // for

        byte_array_format(buf, true, "]");

        array_del(keys);
        array_del(vals);

    } // map
    else if (vt == VAR_LST || vt == VAR_SRC)
        byte_array_format(buf, true, "]");
}

static void variable_mark2(struct variable *v, uint32_t *marker)
{
    if (VISITED_MORE == v->visited)
        return;
    if (VISITED_ONCE == v->visited) {
        v->visited = VISITED_MORE;
        return;
    }

    //DEBUGPRINT("\n>%" PRIu16 " - variable_mark %p %s\n", current_thread_id(), v, var_type_str(v->type));

    // first visit
    v->mark = ++(*marker);
    v->visited = VISITED_ONCE;

    if (VAR_LST == v->type)
    {
        for (int i=0; i<v->list.ordered->length; i++) {
            struct variable *v2 = (struct variable*)array_get(v->list.ordered, i);
            if (v2)
                variable_mark2(v2, marker);
        }
        mark_map(v->list.map, true);
    }
    else if (VAR_KVP == v->type)
    {
        variable_mark2((struct variable*)v->kvp.key, marker);
        variable_mark2((struct variable*)v->kvp.val, marker);
    }
    else if (VAR_FNC == v->type)
        mark_map(v->fnc.closure, true);

    //DEBUGPRINT("variable_mark2 %p->%p\n", v, v->map);
}

void variable_mark(struct variable *v)
{
    if (NULL == v)
        return;
    uint32_t marker = 0;
    variable_mark2(v, &marker);
}

void variable_unmark(struct variable *v)
{
    assert_message(v->type < VAR_LAST, "corrupt variable");
    if (VISITED_NOT == v->visited)
        return;

    //DEBUGPRINT("\n>%" PRIu16 " - variable_unmark %p %s\n", current_thread_id(), v, var_type_str(v->type));

    v->mark = 0;
    v->visited = VISITED_NOT;

    if (VAR_LST == v->type)
    {
        for (int i=0; i<v->list.ordered->length; i++)
        {
            struct variable* element = (struct variable*)array_get(v->list.ordered, i);
            if (NULL != element)
                variable_unmark(element);
        }
        mark_map(v->list.map, false);
    }
    else if (VAR_KVP == v->type)
    {
        variable_unmark((struct variable*)v->kvp.key);
        variable_unmark((struct variable*)v->kvp.val);
    }
    else if (VAR_FNC == v->type)
        mark_map(v->fnc.closure, false);
}

struct byte_array *variable_value(struct context *context, struct variable *v)
{
    struct byte_array *buf = byte_array_new();
    variable_unmark(v);
    variable_mark(v);
    variable_value2(context, v, buf);
    variable_unmark(v);
    return buf;
}

const char *variable_value_str(struct context *context, struct variable *v)
{
    struct byte_array *buf = variable_value(context, v);
    return byte_array_to_string(buf);
}

struct variable *variable_pop(struct context *context)
{
    struct variable *v = (struct variable*)stack_pop(context->operand_stack);
    null_check(v);
    if (v->type == VAR_SRC) {
        if (v->list.ordered->length)
            v = (struct variable*)array_get(v->list.ordered, 0);
        else
            v = variable_new_nil(context);
    }
    return v;
}

void inline variable_push(struct context *context, struct variable *v) {
    stack_push(context->operand_stack, v);
    v->gc_state = GC_OLD;
    //DEBUGPRINT("\n>%" PRIu16 " - variable_push %p %s\n", current_thread_id(), v, var_type_str(v->type));
}

void map_serialize(struct context *context, struct byte_array *bits, struct map *map)
{
    if (NULL == map)
    {
        serial_encode_int(bits, 0);
        return;
    }

    struct array *keys = map_keys(map);
    struct array *values = map_vals(map);
    serial_encode_int(bits, keys->length);
    for (int i=0; i<keys->length; i++) {
        const struct variable *key = (const struct variable*)array_get(keys, i);
        const struct variable *value = (const struct variable*)array_get(values, i);
        variable_serialize(context, bits, key);
        variable_serialize(context, bits, value);
    }
    array_del(keys);
    array_del(values);
}

struct byte_array *variable_serialize(struct context *context,
									  struct byte_array *bits,
                                      const struct variable *in)
{
	null_check(context);
    //DEBUGPRINT("\tserialize:%s\n", variable_value_str(context, (struct variable*)in));
    if (NULL == bits)
        bits = byte_array_new();
        serial_encode_int(bits, in->type);
    switch (in->type) {
        case VAR_INT:   serial_encode_int(bits, in->integer);   break;
        case VAR_FLT:   serial_encode_float(bits, in->floater); break;
        case VAR_BOOL:  serial_encode_int(bits, in->boolean);   break;
        case VAR_STR:   serial_encode_string(bits, in->str);    break;
        case VAR_FNC:
            serial_encode_string(bits, in->fnc.body);
            map_serialize(context, bits, in->fnc.closure);
            break;
        case VAR_LST: {
            uint32_t len = in->list.ordered->length;
            serial_encode_int(bits, len);
            for (int i=0; i<len; i++)
            {
                const struct variable *item = (const struct variable*)array_get(in->list.ordered, i);
                variable_serialize(context, bits, item);
            }
            map_serialize(context, bits, in->list.map);
        } break;
        case VAR_KVP:
            variable_serialize(context, bits, in->kvp.key);
            variable_serialize(context, bits, in->kvp.val);
            break;
        case VAR_NIL:
            break;
        default:
            vm_exit_message(context, "bad var type");
            break;
    }

    return bits;
}

struct map *map_deserialize(struct context *context, struct byte_array *bits)
{
    uint32_t map_length = serial_decode_int(bits);
    if (!map_length)
        return NULL;

    struct map *map = map_new(context);
    for (int i=0; i<map_length; i++)
    {
        struct variable *key = variable_deserialize2(context, bits);
        struct variable *val = variable_deserialize2(context, bits);
        map_insert(map, key, val);
    }
    return map;
}

struct variable *variable_deserialize(struct context *context, struct byte_array *bits)
{
    struct variable *v = variable_deserialize2(context, bits);
    v->gc_state = GC_NEW;
    return v;
}

static struct variable *variable_deserialize2(struct context *context, struct byte_array *bits)
{
	null_check(context);
    assert_message(bits->length, "zero length serialized variable");
    struct variable *result = NULL;
    struct byte_array *str = NULL;

    enum VarType vt = (enum VarType)serial_decode_int(bits);
    switch (vt) {
        case VAR_NIL:
            result = variable_new_nil(context);
            break;
        case VAR_BOOL:
            result = variable_new_bool(context, serial_decode_int(bits));
            break;
        case VAR_INT:
            result = variable_new_int(context, serial_decode_int(bits));
            break;
        case VAR_FLT:
            result = variable_new_float(context, serial_decode_float(bits));
            break;
        case VAR_FNC:
            str = serial_decode_string(bits);
            result = variable_new_fnc(context, str, NULL);
            result->fnc.closure = map_deserialize(context, bits);
            break;
        case VAR_STR:
            str = serial_decode_string(bits);
            result = variable_new_str(context, str);
            break;
        case VAR_BYT:
            str = serial_decode_string(bits);
            result =  variable_new_bytes(context, str, 0);
            break;
        case VAR_LST: {
            uint32_t len = serial_decode_int(bits);
            struct array *list = array_new_size(len);
            while (len--)
            {
                struct variable *vd = variable_deserialize2(context, bits);
                array_add(list, vd);
            }
            result = variable_new_list(context, list);
            array_del(list);
            result->list.map = map_deserialize(context, bits);
            break;
        case VAR_KVP: {
            struct variable *key = variable_deserialize2(context, bits);
            struct variable *val = variable_deserialize2(context, bits);
            result = variable_new_kvp(context, key, val);
            } break;
        }
        default:
            vm_exit_message(context, "bad var type");
    }

    if (NULL != str)
        byte_array_del(str);

    //DEBUGPRINT("\n>%" PRIu16 " - variable_deserialize2 %p %s\n", current_thread_id(), result, var_type_str(result->type));

    result->gc_state = GC_NEW;
    return result;
}

uint32_t variable_length(struct context *context, const struct variable *v)
{
    switch (v->type) {
        case VAR_LST: return v->list.ordered->length;
        case VAR_STR: return v->str->length;
        case VAR_INT: return v->integer;
        case VAR_NIL: return 0;
        default:
            vm_exit_message(context, "non-indexable length");
            return 0;
    }
}

struct variable *variable_part(struct context *context, struct variable *self, uint32_t start, int32_t length)
{
    null_check(self);

    if (length < 0) // count back from end of list/string
        length = self->list.ordered->length + length + 1 - start;
    if (length < 0) // end < start
        length = 0;

    struct variable *result = NULL;
    switch (self->type) {
        case VAR_STR: {
            if (start >= self->str->length)
                return variable_new_str(context, NULL);
            struct byte_array *str = byte_array_part(self->str, start, length);
            result = variable_new_str(context, str);
            byte_array_del(str);
            break;
        }
        case VAR_LST: {
            if (start > self->list.ordered->length)
                return variable_new_list(context, NULL);
            struct array *list = array_part(self->list.ordered, start, length);
            result = variable_new_list(context, list);
            array_del(list);
            break;
        }
        default:
            result = (struct variable*)exit_message("bad part type");
            break;
    }
    return result;
}

void variable_remove(struct variable *self, uint32_t start, int32_t length)
{
    null_check(self);
    switch (self->type) {
        case VAR_STR:
            byte_array_remove(self->str, start, length);
            break;
        case VAR_LST:
            array_remove(self->list.ordered, start, length);
            break;
        default:
            exit_message("bad remove type");
    }
}

struct variable *variable_concatenate(struct context *context, int n, const struct variable* v, ...)
{
    struct variable* result = variable_copy(context, v);

    va_list argp;
    for(va_start(argp, v); --n;) {
        struct variable* parameter = va_arg(argp, struct variable* );
        if (NULL == parameter)
            continue;
        switch (result->type) {
            case VAR_STR: byte_array_append(result->str, parameter->str); break;
            case VAR_LST: array_append(result->list.ordered, parameter->list.ordered);    break;
            default: return (struct variable*)exit_message("bad concat type");
        }
    }

    va_end(argp);
    return result;
}

static struct map *variable_map_insert2(struct context *context, struct map* map,
                                        struct variable *key, struct variable *val)
{
    if (NULL == map)
        map = map_new(context);

    // setting a value to nil means removing the key
    if (val->type == VAR_NIL)
        map_remove(map, key);
    else
    {
        key = variable_copy_value(context, key);
        val = variable_copy_value(context, val);
        map_insert(map, key, val);
    }

    key->gc_state = val->gc_state = GC_OLD;
    return map;
}

struct variable *variable_copy_value(struct context *context, struct variable *value)
{
    enum VarType vt = value->type;
    bool is_a_pointer = !(vt==VAR_INT || vt==VAR_FLT || vt==VAR_BOOL || vt==VAR_NIL);
    return is_a_pointer ? value : variable_copy(context, value);
}

void variable_map_insert(struct context *context, struct variable* v,
                         struct variable *key, struct variable *datum)
{
    if (key->type == VAR_NIL)
        return;
    if (v->type == VAR_LST || v->type == VAR_SRC)
        v->list.map = variable_map_insert2(context, v->list.map, key, datum);
    else if (v->type == VAR_FNC)
        v->fnc.closure = variable_map_insert2(context, v->list.map, key, datum);
    else exit_message("variable does not have map");
}

struct variable *variable_map_get(struct context *context, struct variable *v, struct variable *key)
{
    assert_message(v->type == VAR_LST, "only lists may have maps");
    struct variable *result = lookup(context, v, key);
    if (result->type == VAR_SRC)
        result = array_get(result->list.ordered, 0);
    return result;
}

static bool variable_compare_maps(struct context *context, const struct map *umap, const struct map *vmap)
{
    if ((NULL == umap) && (NULL == vmap))
        return true;
    if (NULL == umap)
        return variable_compare_maps(context, vmap, umap);
    struct array *keys = map_keys(umap);
    bool result = true;
    if (NULL == vmap)
        result = !keys->length;
    else for (int i=0; i<keys->length; i++) {
        struct variable *key = (struct variable*)array_get(keys, i);
        struct variable *uvalue = (struct variable*)map_get(umap, key);
        struct variable *vvalue = (struct variable*)map_get(vmap, key);
        if (!variable_compare(context, uvalue, vvalue)) {
            result = false;
            break;
        }
    }
    array_del(keys);
    return result;
}

int32_t variable_value_int(const struct variable *v)
{
    switch (v->type) {
        case VAR_INT:   return v->integer;
        case VAR_BOOL:  return  v->boolean;
        default: return 0;
    }
}

bool variable_compare(struct context *context, const struct variable *u, const struct variable *v)
{
    if ((NULL == u) != (NULL == v))
        return false;
    enum VarType ut = (enum VarType)u->type;
    enum VarType vt = (enum VarType)v->type;

    if ((ut == VAR_INT || ut == VAR_BOOL || ut == VAR_NIL) && (vt == VAR_INT || vt == VAR_BOOL || vt == VAR_NIL))
        return variable_value_int(u) == variable_value_int(v);
    
    if (ut != vt)
        return false;

    switch (ut) {
        case VAR_LST:
            if (u->list.ordered->length != v->list.ordered->length)
                return false;
            for (int i=0; i<u->list.ordered->length; i++) {
                struct variable *ui = (struct variable*)array_get(u->list.ordered, i);
                struct variable *vi = (struct variable*)array_get(v->list.ordered, i);
                if (!variable_compare(context, ui, vi))
                    return false;
            }
            return variable_compare_maps(context, u->list.map, v->list.map);

        case VAR_BOOL:  return (u->boolean == v->boolean);
        case VAR_INT:   return (u->integer == v->integer);
        case VAR_FLT:   return (u->floater == v->floater);
        case VAR_STR:   return (byte_array_equals(u->str, v->str));
        case VAR_NIL:   return true;
        default:
            return (bool)vm_exit_message(context, "bad comparison");
    }
}

struct variable* variable_set(struct context *context, struct variable *dst, const struct variable* src)
{
    vm_null_check(context, dst);
    vm_null_check(context, src);
    switch (src->type) {
        case VAR_NIL:                                                       break;
        case VAR_INT:   dst->integer = src->integer;                        break;
        case VAR_FLT:   dst->floater = src->floater;                        break;
        case VAR_FNC:
            dst->fnc.body = byte_array_copy(src->fnc.body);
            dst->fnc.closure = map_copy(context, src->fnc.closure);
            break;
        case VAR_BYT:
        case VAR_STR:   dst->str = byte_array_copy(src->str);               break;
        case VAR_SRC:
        case VAR_LST:
            dst->list.ordered = array_copy(src->list.ordered);
            dst->list.map = map_copy(context, src->list.map);
            break;
        case VAR_KVP:   dst->kvp = src->kvp;                                break;
        case VAR_CFNC:  dst->cfnc = src->cfnc;                              break;
        case VAR_BOOL:  dst->boolean = src->boolean;                        break;
        case VAR_VOID:  dst->ptr = src->ptr;                                break;
        default:
            vm_exit_message(context, "bad var type");
            break;
    }
    dst->type = src->type;
    return dst;
}

struct variable* variable_copy(struct context *context, const struct variable* v)
{
    vm_null_check(context, v);
    struct variable *u = variable_new(context, (enum VarType)v->type);
    variable_set(context, u, v);
    //DEBUGPRINT("variable_copy %p -> %p -> %p\n", context, v, u);
    return u;
}

struct variable *variable_find(struct context *context,
                               struct variable *self,
                               struct variable *sought,
                               struct variable *start)
{
    // search for substring
    if (self->type == VAR_STR && sought->type == VAR_STR)
    {
        assert_message(!start || start->type == VAR_INT, "non-integer index");
        int32_t beginning = start ? start->integer : 0;
        int32_t index = byte_array_find(self->str, sought->str, beginning);
        if (index >= 0)
            return variable_new_int(context, index);
    }
    else if (self->type == VAR_LST)
    {
        for (int i=0; i<self->list.ordered->length; i++)
        {
            struct variable *v = (struct variable*)array_get(self->list.ordered, i);
            if ((sought->type == VAR_INT && v->type == VAR_INT && v->integer == sought->integer) ||
                (sought->type == VAR_STR && v->type == VAR_STR && byte_array_equals(sought->str, v->str)))
                return variable_new_int(context, i);
        }
    }

    return variable_new_int(context, -1); // -1 means 'not found'
}
