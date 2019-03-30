/* struct.c
 *
 * implements array, byte_array, lifo and dic
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>
#include <ctype.h>

#include "vm.h"
#include "struct.h"
#include "util.h"

#define ERROR_BYTE_ARRAY_LEN    "byte array too long"
#define GROWTH_FACTOR           2

// array ///////////////////////////////////////////////////////////////////

struct array *array_new() {
    struct array *a = array_new_size(0);
    //DEBUGPRINT("array_new %p->%p\n", a, a->data);
    return a;
}

struct array *array_new_size(uint32_t size) {
    struct array *a = (struct array*)malloc(sizeof(struct array));
    a->data = a->current = (void**)malloc(size * sizeof(void**));
    a->length = 0;
    a->size = size;
    for (int i=0; i<size; i++)
            a->data[i] = NULL;
    return a;
}

void array_del(struct array *a) {
    //DEBUGPRINT("array_del %p->%p\n", a, a->data);
    free(a->data);
    free(a);
}

uint32_t list_resize(uint32_t size, uint32_t length) {
    uint32_t oldsize = size;
    if (length > size)
        size = length * GROWTH_FACTOR;
    if (length < size / (GROWTH_FACTOR * GROWTH_FACTOR))
        size = size / GROWTH_FACTOR;
    return size != oldsize ? size : 0;
}

void array_resize(struct array *a, uint32_t size) {
    if (!(size = list_resize(a->size, size))) // didn't resize
        return;
    uint32_t delta = (uint32_t)(a->current - a->data);
    a->data = (void**)realloc(a->data, size * sizeof(void*));
    null_check(a->data);

    //DEBUGPRINT("array_resize %p->%p -- %d to %d\n", a, a->data, a->size, size);
    if (size > a->size)
        memset(&a->data[a->size], 0, (size - a->size) * sizeof(void*));

    a->size = size;
    a->current = a->data + delta;
    return;
}

uint32_t array_add(struct array *a, void *datum) {
    array_resize(a, a->length + 1);
    a->data[a->length++] = datum;
    return a->length-1;
}

void array_insert(struct array *a, uint32_t index, void *datum) {
    array_resize(a, a->length + 1);
    uint32_t i;
    for (i=a->length; i>index; i--)
        a->data[i] = a->data[i-1];
    a->data[i] = datum;
    a->length++;
}

void* array_get(const struct array *a, uint32_t index) {
    assert_message(a && index < a->length, ERROR_INDEX);
    //DEBUGPRINT("array_get %d = %x\n", index, a->data[index]);
    return a->data[index];
}

void array_set(struct array *a, uint32_t index, void* datum) {
    null_check(a);
    uint32_t minlen = index + 1;
    array_resize(a, minlen);
    //DEBUGPRINT("array_set %d %x\n", index, datum);
    a->data[index] = datum;
    if (a->length <= minlen)
        a->length = minlen;
}

void list_remove(void *data, uint32_t *end, uint32_t start, int32_t length, size_t width) {
    assert_message(data || !length, "list can't remove");
    null_check(end);
    length = length < 0 ? *end - start : length;
    assert_message(!length || (start < *end && start+length <= *end), "index out of bounds");

    memmove((uint8_t*)data+start*width, (uint8_t*)data+(start+length)*width, (*end-start-length)*width);
    *end -= (uint32_t)length;
}

void array_remove(struct array *a, uint32_t start, int32_t length) {
    list_remove(a->data, &a->length, start, length, sizeof(void*));
    array_resize(a, a->length);
    //DEBUGPRINT("array_remove %p->%p\n", a, a->data);
}

struct array *array_copy(const struct array* original) {
    if (original == NULL) {
        return NULL;
    }
    struct array* copy = array_new_size(original->size); // (struct array*)malloc(sizeof(struct array));
    //copy->data = (void**)malloc(original->length * sizeof(void**));
    memcpy(copy->data, original->data, original->length * sizeof(void*));
    copy->length = original->length;
    copy->current = original->current;
    copy->size = original->size;
    //DEBUGPRINT("array_copy %p->%p\n", copy, copy->data);
    return copy;
}

struct array *array_part(struct array *within, uint32_t start, uint32_t length) {
    struct array *p = array_copy(within);
    array_remove(p, start+length, within->length-start-length);
    array_remove(p, 0, start);
    array_resize(p, p->length);
    return p;
}

void array_append(struct array *a, const struct array* b) {
    null_check(a);
    null_check(b);
    uint32_t alen = a->length;
    uint32_t newlen = alen + b->length;
    array_resize(a, newlen);
    memcpy(&a->data[alen], b->data, b->length * sizeof(void*));
    a->length = newlen;
    a->current += b->length;
}

// byte_array ///////////////////////////////////////////////////////////////

struct byte_array *byte_array_new() {
    return byte_array_new_size(0);
}

void byte_array_del(struct byte_array* ba) {
    //DEBUGPRINT("byte_array_del %p->%p\n", ba, ba->data);
    if (NULL != ba->data) {
        free(ba->data);
    }
    free(ba);
}

struct byte_array *byte_array_new_size(uint32_t size) {
    struct byte_array* ba = (struct byte_array*)malloc(sizeof(struct byte_array));
    ba->data = ba->current = (uint8_t*)malloc(size);
    ba->length = 0;
    ba->size = size;
    //DEBUGPRINT("byte_array_new_size %p->%p\n", ba, ba->data);
    return ba;
}

struct byte_array *byte_array_new_data(uint32_t size, uint8_t *data) {
    struct byte_array *ba = byte_array_new();
    ba->size = ba->length = size;
    ba->data = ba->current = data;
    return ba;
}

void byte_array_resize(struct byte_array* ba, uint32_t size) {
    if (!(size = list_resize(ba->size, size))) { // didn't resize
        return;
    }
    assert_message(ba->current >= ba->data, "byte_array corrupt");
    uint32_t delta = (uint32_t)(ba->current - ba->data);
    ba->data = (uint8_t*)realloc(ba->data, size);
    assert_message(ba->data, "could not reallocate data");
    ba->current = ba->data + delta;
    ba->size = size;
    //DEBUGPRINT("byte_array_resize %p->%p\n", ba, ba->data);
}

bool byte_array_equals(const struct byte_array *a, const struct byte_array* b) {
    if (a==b) {
        return true;
    } else if ((a == NULL) != (b == NULL)) { // one is null and the other is not
        return false;
    } else if (a->length != b->length) {
        return false;
    }
    return !memcmp(a->data, b->data, a->length * sizeof(uint8_t));
}

struct byte_array *byte_array_copy(const struct byte_array* original) {
    if (original == NULL) {
        return NULL;
    }
    struct byte_array* copy = byte_array_new_size(original->size);
    memcpy(copy->data, original->data, original->length);
    copy->length = original->length;
    copy->current = copy->data + (original->current - original->data);
    return copy;
}

void byte_array_set(struct byte_array *within, uint32_t index, uint8_t byte) {
    null_check(within);
    assert_message(index < within->length, "out of bounds");
    within->data[index] = byte;
}

uint8_t byte_array_get(const struct byte_array *within, uint32_t index) {
    null_check(within);
    assert_message(index < within->length, "out of bounds");
    return within->data[index];
}

void byte_array_append(struct byte_array *a, const struct byte_array* b) {
    null_check(a);
    null_check(b);
    uint32_t offset = a->length;
    uint32_t newlen = a->length + b->length;
    byte_array_resize(a, newlen);
    memcpy(&a->data[offset], b->data, b->length);
    a->length = newlen;
    a->current = a->data + newlen;
}

void byte_array_remove(struct byte_array *self, uint32_t start, int32_t length) {
    list_remove(self->data, &self->length, start, length, sizeof(uint8_t));
    byte_array_resize(self, self->length);
    //DEBUGPRINT("byte_array_remove %p->%p\n", self, self->data);
}

struct byte_array *byte_array_part(struct byte_array *within, uint32_t start, uint32_t length) {
    struct byte_array *p = byte_array_copy(within);
    byte_array_remove(p, start+length, within->length-start-length);
    byte_array_remove(p, 0, start);
    byte_array_resize(p, p->length);
    return p;
}

struct byte_array *byte_array_from_string(const char* str) {
    int len = (int)strlen(str);
    struct byte_array* ba = byte_array_new_size(len);
    memcpy(ba->data, str, len);
    ba->length = len;
    //DEBUGPRINT("byte_array_from_string %s %p->%p\n", str, ba, ba->data);
    return ba;
}

char* byte_array_to_string(const struct byte_array* ba) {
    int len = ba->length;
    char* s = (char*)malloc(len+1);
    memcpy(s, ba->data, len);
    s[len] = 0;
    //DEBUGPRINT("byte_array_to_string %p\n", s);
    return s;
}

inline void byte_array_reset(struct byte_array* ba) {
    ba->current = ba->data;
}

struct byte_array *byte_array_concatenate(int n, const struct byte_array* ba, ...) {
    struct byte_array* result = byte_array_copy(ba);

    va_list argp;
    for (va_start(argp, ba); --n;) {
        struct byte_array* parameter = va_arg(argp, struct byte_array* );
        if (parameter == NULL)
            continue;
        assert_message(result->length + parameter->length < BYTE_ARRAY_MAX_LEN, ERROR_BYTE_ARRAY_LEN);
        byte_array_append(result, parameter);
    }

    va_end(argp);

    if (result) {
        result->current = result->data + result->length;
    }
    return result;
}

struct byte_array *byte_array_add_byte(struct byte_array *a, uint8_t b) {
    a->length++;
    byte_array_resize(a, a->length);
    a->current = a->data + a->length;
    a->data[a->length-1] = b;
    return a;
}

void byte_array_print(char* into, size_t size, const struct byte_array* ba) {
    if (size < 2) {
        return;
    }
    sprintf(into, "0x");
    for (int i=0; i<ba->length && i < size-1; i++) {
        sprintf(into+(i+1)*2, "%02X", ba->data[i]);
    }
}

int32_t byte_array_find(struct byte_array *within, struct byte_array *sought, int32_t start) {
    null_check(within);
    null_check(sought);

    int32_t ws = within->length;
    int32_t ss = sought->length;

    bool reverse = (start < 0);
    int32_t not_found = reverse ? (start + ws + 1) : -1;

    if ((start + ss) >= (int32_t)within->length) {
        return not_found;
    }

    uint8_t *wd = within->data;
    uint8_t *sd = sought->data;

    if (!reverse) { // forward search
        for (int32_t i=start; i<=ws-ss; i++)
            if (!memcmp(wd + i, sd, ss))
                return i;

    } else { // reverse search
        for (int32_t i=ws-ss+start+2; i>=0; i--) {
            if (!memcmp(wd + i, sd, ss)) {
                return i;
            }
        }
    }
    return not_found;
}

struct byte_array *byte_array_replace_all(struct byte_array *original, struct byte_array *a, struct byte_array *b) {
    /*DEBUGPRINT("replace in %s: %s -> %s\n",
               byte_array_to_string(original),
               byte_array_to_string(a),
               byte_array_to_string(b));*/

    struct byte_array *replaced = byte_array_copy(original);
    int32_t where = 0;

    for (;;) { // replace all

        if ((where = byte_array_find(replaced, a, where)) < 0) {
            break;
        }
        struct byte_array *replaced2 = byte_array_replace(replaced, b, where++, a->length);
        byte_array_del(replaced);
        replaced = replaced2;
    }
    return replaced;
}

struct byte_array *byte_array_replace(struct byte_array *within, struct byte_array *replacement, uint32_t start, int32_t length) {
    null_check(within);
    null_check(replacement);
    uint32_t ws = within->length;
    assert_message(start < ws, "index out of bounds");
    if (length < 0) {
        length = ws - start;
    }

    int32_t new_length = within->length - length + replacement->length;
    struct byte_array *replaced = byte_array_new_size(new_length);
    replaced->length = new_length;

    memcpy(replaced->data, within->data, start);
    memcpy(replaced->data + start, replacement->data, replacement->length);
    memcpy(replaced->data + start + replacement->length,
           within->data + start + length,
           within->length - start - length);

    //DEBUGPRINT("byte_array_replace %p->%p\n", replaced, replaced->data);
    return replaced;
}

// adds formatted string to end of byte_array, so sprintf + strcat
void byte_array_format(struct byte_array *ba, bool append, const char *format, ...) {
    bool resize;

    do {
        size_t capacity = ba->size;
        uint8_t *at = ba->data;
        if (append) {
            capacity -= ba->length;
            at = ba->current;
        }

        va_list args;
        va_start(args, format);

        int written = vsnprintf((char*)at, capacity, format, args) + 1;
        if ((resize = (written > capacity) && (ba->size < VV_SIZE))) {
            byte_array_resize(ba, (uint32_t)(ba->size + written - capacity));
        } else {
            ba->length = (uint32_t)(ba->length + MIN(written-1, capacity));
            ba->current = ba->data + ba->length;
        }

        va_end(args);

    } while (resize);
}

// stack ////////////////////////////////////////////////////////////////////

struct stack *stack_new() {
    return (struct stack*)calloc(sizeof(struct stack), 1);
}

void stack_del(struct stack *s) {
    struct stack_node *sn = s->head;
    while (sn) {
        struct stack_node *oldnode = sn;
        sn = sn->next;
        free(oldnode);
    }
    free(s);
}

struct stack_node* stack_node_new() {
    return (struct stack_node*)calloc(sizeof(struct stack_node), 1);
}

uint32_t stack_depth(struct stack *stack) {
    uint32_t i;
    struct stack_node *sn = stack->head;
    for (i=0; sn; i++, sn=sn->next);
    return i;
}

void stack_push(struct stack *stack, void* data) {
    null_check(data);
    if (stack->head == NULL) {
        stack->head = stack->tail = stack_node_new();
    } else {
        struct stack_node* old_head = stack->head;
        stack->head = stack_node_new();
        null_check(stack->head);
        stack->head->next = old_head;
    }
    stack->head->data = data;
    //DEBUGPRINT("stack_push %x to %x:%d\n", data, stack, stack_depth(stack));
}

void* stack_pop(struct stack *stack) {
    if (stack->head == NULL) {
        return NULL;
    }
    void* data = stack->head->data;
    struct stack_node *oldnode = stack->head;
    stack->head = stack->head->next;
    if (stack->head == NULL) {
        stack->tail = NULL;
    }
    if (NULL != oldnode) {
        free(oldnode);
    }
    null_check(data);
    //DEBUGPRINT("stack_pop %x from %x:%d\n", data, stack, stack_depth(stack));
    return data;
}

void* stack_peek(const struct stack *stack, uint8_t index) {
    null_check(stack);
    struct stack_node *p = stack->head;
    for (; index && p; index--, p=p->next);
    return p ? p->data : NULL;
}

bool stack_empty(const struct stack *stack) {
    null_check(stack);
    return stack->head == NULL;
}

// dic /////////////////////////////////////////////////////////////////////

static int32_t default_hashor(const void *x, void *context) {
    const struct variable *key = (const struct variable*)x;
    switch (key->type) {
        case VAR_INT:
            return key->integer;
        case VAR_FNC:
        case VAR_BYT:
        case VAR_STR: {
            const struct byte_array *name = key->str;
            int32_t hash = 0;
            int i = 0;
            for (i = 0; i<name->length; i++)
                hash += name->data[i];
            return hash;
        }
        case VAR_NIL:
            return 0;
            break;
        default:
            exit_message("not handled hash type");
            return 1;
    }
}

static bool default_comparator(const void *a, const void *b, void *context) {
    return variable_compare((struct context *)context, (struct variable *)a, (struct variable *)b);
}

static void *default_copyor(const void *key, void *context) {
    struct variable *v = (struct variable *)key;
    struct variable *u = variable_copy((struct context *)context, v);
    variable_old(u);
    return u;
}

static void default_rm(const void *key, void *context) {}

struct dic* dic_new_ex(void *context, dic_compare *mc, dic_hash *mh, dic_copyor *my, dic_rm *md) {
    struct dic *m;
    if ((m =(struct dic*)malloc(sizeof(struct dic))) == NULL) {
        return NULL;
    }
    m->context = context;
    m->size = 16;
    m->hash_func = mh ? mh : &default_hashor;
    m->comparator = mc ? mc : &default_comparator;
    m->deletor = md ? md : & default_rm;
    m->copyor = my ? my : &default_copyor;

    if ((m->nodes = (struct hash_node**)calloc(m->size, sizeof(struct hash_node*))) == NULL) {
        free(m);
        return NULL;
    }

    //DEBUGPRINT("dic_new %p\n", m);
    return m;
}

struct dic* dic_new(void *context) {
    return dic_new_ex(context, NULL, NULL, NULL, NULL);
}

void dic_del(struct dic *m) {
    //DEBUGPRINT("dic_del %p\n", m);
    struct hash_node *node, *oldnode;

    for (size_t n = 0; n<m->size; ++n) {
        node = m->nodes[n];
        while (node) {
            m->deletor(node->key, m->context);
            oldnode = node;
            node = node->next;
            free(oldnode);
        }
    }
    free(m->nodes);
    free(m);
}

bool dic_key_equals(const struct dic *m1, const void *key1, const void *key2) {
    return m1->comparator(key1, key2, m1->context);
}

int dic_insert(struct dic *m, const void *key, void *data) {
    struct hash_node *node;
    int32_t hash = m->hash_func(key, m->context) % m->size;

    node = m->nodes[hash];
    while (node) {
        if (dic_key_equals(m, node->key, key)) {
            node->data = data;
            return 0;
        }
        node = node->next;
    }

    if ((node = (struct hash_node*)malloc(sizeof(struct hash_node))) == NULL) {
        return -1;
    }
    if ((node->key = m->copyor(key, m->context)) == NULL) {
        free(node);
        return -1;
    }
    node->data = data;
    node->next = m->nodes[hash];
    m->nodes[hash] = node;

    return 0;
}

struct array* dic_keys(const struct dic *m) {
    null_check(m);
    struct array *a = array_new();
    for (int i=0; i<m->size; i++)
        for (const struct hash_node* n = m->nodes[i]; n; n=n->next)
            if (n->data)
                array_add(a, n->key);
    //DEBUGPRINT("dic_keys %p\n", a);
    return a;
}

struct array* dic_vals(const struct dic *m) {
    struct array *a = array_new();
    for (int i=0; i<m->size; i++)
        for (const struct hash_node* n = m->nodes[i]; n; n=n->next)
            if (n->data)
                array_add(a, n->data);
    return a;
}

int dic_remove(struct dic *m, const void *key) {
    struct hash_node *node, *prevnode = NULL;
    size_t hash = m->hash_func(key, m->context)%m->size;

    node = m->nodes[hash];
    while(node) {
        if (dic_key_equals(m, node->key, key)) {
            m->deletor(node->key, m->context);
            if (prevnode) {
                prevnode->next = node->next;
            } else {
                m->nodes[hash] = node->next;
            }
            free(node);
            return 0;
        }
        prevnode = node;
        node = node->next;
    }
    return -1;
}

bool dic_has(const struct dic *m, const void *key) {
    struct hash_node *node;
    size_t hash = m->hash_func(key, m->context) % m->size;
    node = m->nodes[hash];
    while (node) {
        if (dic_key_equals(m, node->key, key))
            return true;
        node = node->next;
    }
    return false;
}

void *dic_get(const struct dic *m, const void *key) {
    if (NULL == m) {
        return NULL;
    }
    struct hash_node *node;
    size_t hash = m->hash_func(key, m->context) % m->size;
    node = m->nodes[hash];
    while (node) {
        if (dic_key_equals(m, node->key, key))
            return node->data;
        node = node->next;
    }
    return NULL;
}

int dic_resize(struct dic *m, size_t size) {
    //DEBUGPRINT("dic_resize\n");
    struct dic newtbl;
    size_t n;
    struct hash_node *node,*next;

    newtbl.size = size;
    newtbl.hash_func = m->hash_func;

    if ((newtbl.nodes = (struct hash_node**)calloc(size, sizeof(struct hash_node*))) == NULL) {
        return -1;
    }

    for (n = 0; n<m->size; ++n) {
        for (node = m->nodes[n]; node; node = next) {
            next = node->next;
            dic_insert(&newtbl, node->key, node->data);
            dic_remove(m, node->key);
        }
    }

    free(m->nodes);
    m->size = newtbl.size;
    m->nodes = newtbl.nodes;

    return 0;
}

// a - b
struct dic *dic_minus(struct dic *a, const struct dic *b) {
    if ((a == NULL) || (b == NULL)) {
        return a;
    }
    struct array *keys = dic_keys(b);
    for (int i=0; i<keys->length; i++) {
        const void *key = array_get(keys, i);
        if (dic_has(a, key)) {
            dic_remove(a, key);
        }
    }
    array_del(keys);
    return a;
}

// a + b; in case of intersection, a wins
struct dic *dic_union(struct dic *a, const struct dic *b) {
    if (b == NULL) {
        return a;
    }
    if (a == NULL) {
        return dic_copy(b->context, b);
    }
    struct array *keys = dic_keys(b);
    for (int i=0; i<keys->length; i++) {
        const void *key = array_get(keys, i);
        if (!dic_has(a, key)) {
            void *key2 = b->copyor(key, a->context);
            void *value = dic_get(b, key);
            void *value2 = b->copyor(value, a->context);
            dic_insert(a, key2, value2);
        }
    }
    array_del(keys);
    return a;
}

struct dic *dic_copy(void *context, const struct dic *original) {
    if (original == NULL) {
        return NULL;
    }
    struct dic *copy;
    copy = dic_new_ex(context,
                      original->comparator,
                      original->hash_func,
                      original->copyor,
                      original->deletor);
    return dic_union(copy, original);
}
