/* struct.h
 *
 * APIs for array, byte_array, [f|l]ifo and dic
 */

#ifndef STRUCT_H
#define STRUCT_H

#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdarg.h>

#define ERROR_INDEX	"index out of bounds"
#define ERROR_NULL "null pointer"
#define BYTE_ARRAY_MAX_LEN (1024*1024)

// array ///////////////////////////////////////////////////////////////////

struct array {
	void **data;
    void **current;
	uint32_t length;
    uint32_t size;
};

struct array *array_new(void);
void array_del(struct array *a);
struct array *array_new_size(uint32_t size);
uint32_t array_add(struct array *a , void *datum);
void array_insert(struct array *a, uint32_t index, void *datum);
void* array_get(const struct array *a, uint32_t index);
void array_set(struct array *a, uint32_t index, void *datum);
void array_remove(struct array *a, uint32_t start, int32_t length);
struct array *array_part(struct array *within, uint32_t start, uint32_t length);
void array_append(struct array *a, const struct array* b);
struct array *array_copy(const struct array* original);

// byte_array ///////////////////////////////////////////////////////////////

struct byte_array {
	uint8_t *data, *current;
	uint32_t length;
    uint32_t size;
};

struct byte_array *byte_array_new(void);
struct byte_array *byte_array_new_size(uint32_t size);
struct byte_array *byte_array_new_data(uint32_t size, uint8_t *data);
struct byte_array *byte_array_from_string(const char* str);
struct byte_array *byte_array_copy(const struct byte_array* original);
struct byte_array *byte_array_add_byte(struct byte_array *a, uint8_t b);
struct byte_array *byte_array_concatenate(int n, const struct byte_array* ba, ...);
struct byte_array *byte_array_part(struct byte_array *within, uint32_t start, uint32_t length);
struct byte_array *byte_array_replace_all(struct byte_array *original,
                                          struct byte_array *a, struct byte_array *b);
struct byte_array *byte_array_replace(struct byte_array *within, struct byte_array *replacement,
                                      uint32_t start, int32_t length);
void    byte_array_append(struct byte_array *a, const struct byte_array* b);
char*   byte_array_to_string(const struct byte_array* ba);
void    byte_array_del(struct byte_array* ba);
void    byte_array_reset(struct byte_array* ba);
bool    byte_array_equals(const struct byte_array *a, const struct byte_array* b);
void    byte_array_print(char* into, size_t size, const struct byte_array* ba);
int32_t byte_array_find(struct byte_array *within, struct byte_array *sought, int32_t start);
void    byte_array_remove(struct byte_array *within, uint32_t start, int32_t length);
void    byte_array_set(struct byte_array *within, uint32_t index, uint8_t byte);
uint8_t byte_array_get(const struct byte_array *within, uint32_t index);
void    byte_array_format(struct byte_array *ba, bool append, const char *format, ...);

// stack ////////////////////////////////////////////////////////////////////

struct stack_node {
	void* data;
	struct stack_node* next;
};

struct stack {
	struct stack_node* head;
	struct stack_node* tail;
};

struct stack* stack_new(void);
void stack_del(struct stack *s);
struct stack_node* stack_node_new(void);
void fifo_push(struct stack* fifo, void* data);
void stack_push(struct stack* stack, void* data);
void* stack_pop(struct stack* stack);
void* stack_peek(const struct stack* stack, uint8_t index);
bool stack_empty(const struct stack* stack);
uint32_t stack_depth(struct stack *stack);

// dic /////////////////////////////////////////////////////////////////////

struct hash_node {
	void *key;
	void *data;
	struct hash_node *next;
};

typedef bool (dic_compare)(const void *a, const void *b, void *context);
typedef int32_t (dic_hash)(const void *x, void *context);
typedef void *(dic_copyor)(const void *x, void *context);
typedef void (dic_rm)(const void *x, void *context);

struct dic {
    dic_compare *comparator;
	size_t size;
	struct hash_node **nodes;
    dic_hash *hash_func;
    dic_rm *deletor;
    dic_copyor *copyor;
    void *context;
};

struct dic* dic_new(void *context);
struct dic* dic_new_ex(void *context, dic_compare *mc, dic_hash *mh, dic_copyor *my, dic_rm *md);
void dic_del(struct dic* dic);
int dic_insert(struct dic* dic, const void *key, void *data);
int dic_remove(struct dic* dic, const void *key);
void *dic_get(const struct dic* dic, const void *key);
bool dic_has(const struct dic* dic, const void *key);
struct array* dic_keys(const struct dic* m);
struct array* dic_vals(const struct dic* m);
struct dic *dic_union(struct dic *a, const struct dic *b);
struct dic *dic_minus(struct dic *a, const struct dic *b);
struct dic *dic_copy(void *context, const struct dic *dic);

#endif // STRUCT_H
