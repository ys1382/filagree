#ifndef VARIABLE_H
#define VARIABLE_H

#define VV_SIZE 10000

enum VarType
{
    VAR_NIL,    // nil
    VAR_INT,    // integer
    VAR_FLT,    // float
    VAR_STR,    // string
    VAR_FNC,    // function
    VAR_LST,    // list/map
    VAR_KVP,    // key-value pair
    VAR_SRC,    // assignment RHS
    VAR_ERR,    // error
    VAR_BYT,    // byte array
    VAR_BOOL,   // boolean
    VAR_VOID,   // void*
    VAR_CFNC,   // pointer to c function
    VAR_LAST,   // end of enums
};    

enum Visited // for graph traversal
{
    VISITED_NOT,    // we did not traverse to this variable yet
    VISITED_ONCE,   // we visited it once so far
    VISITED_MORE,   // we visited it more than once
    VISITED_REPEAT, // we've seen this before in the same structure
    VISITED_LAST    // end of enums
};

enum GCsafety // for garbage collection
{
    GC_NEW, // this variable was just created
    GC_OLD, // this variable is in some structure, so safe from GC until no longer needed
    GC_SAFE // don't GC this variable just because
};

typedef struct context *context_p; // forward declaration
typedef struct variable *(callback2func)(context_p context);

struct variable
{
    enum VarType type;
    enum Visited visited;
    uint32_t mark;
    enum GCsafety gc_state;

    union {
        struct byte_array* str;
        struct {
            
            struct byte_array* body;
            struct map *closure;
        } fnc;
        struct {
            struct array *ordered;
            struct map *map;
        } list;
        int32_t integer;
        float floater;
        bool boolean;
        void *ptr;
        struct {
            struct variable *key, *val;
        } kvp;
        struct {
            struct variable*(*f)(context_p);
            struct variable *data;
        } cfnc;
    };
};

struct variable* variable_new(struct context *context, enum VarType type);
void variable_del(struct context *context, struct variable *v);
struct byte_array* variable_value(struct context *context, struct variable* v);
const char *variable_value_str(struct context *context, struct variable *v);
int32_t variable_value_int(const struct variable *v);
float variable_value_flt(const struct variable *v);

struct byte_array *variable_serialize(struct context *context, struct byte_array *bits,
                                      const struct variable *in);
struct variable *variable_deserialize(struct context *context, struct byte_array *str);

void variable_old(struct variable *v);

struct variable* variable_new_bool(struct context *context, bool b);
struct variable *variable_new_err(struct context *context, const char* message);
struct variable *variable_new_cfnc(struct context *context, callback2func *cfnc);
struct variable *variable_new_int(struct context *context, int32_t i);
struct variable *variable_new_nil(struct context *context);
struct variable *variable_new_kvp(struct context *context, struct variable *key, struct variable *val);
struct variable *variable_new_float(struct context *context, float f);
struct variable *variable_new_str(struct context *context, struct byte_array *str);
struct variable *variable_new_str_chars(struct context *context, const char *str);
struct variable *variable_new_fnc(struct context *context,
                                  struct byte_array *body, struct variable *closures);
struct variable *variable_new_list(struct context *context, struct array *list);
struct variable *variable_new_src(struct context *context, uint32_t size);
struct variable *variable_new_bytes(struct context *context, struct byte_array *bytes, uint32_t size);
struct variable *variable_new_void(struct context *context, void *p);

struct variable *variable_copy(struct context *context, const struct variable *v);
struct variable *variable_pop(struct context *context);
uint32_t variable_length(struct context *context, const struct variable *v);
void variable_push(struct context *context, struct variable *v);
struct variable *variable_concatenate(struct context *context, int n, const struct variable* v, ...);
void variable_remove(struct variable *self, uint32_t start, int32_t length);
struct variable *variable_part(struct context *context, struct variable *self,
                               uint32_t start, int32_t length);
void variable_map_insert(struct context *context, struct variable* v,
                         struct variable *key, struct variable *data);
struct variable *variable_map_get(struct context *context, struct variable* v, struct variable *key);
bool variable_compare(struct context *context, const struct variable *u, const struct variable *v);
struct variable *variable_copy_value(struct context *context, struct variable *value);
void variable_mark(struct variable *v);
void variable_unmark(struct variable *v);

const char *var_type_str(enum VarType vt);

struct variable *variable_find(struct context *context,
                               struct variable *self,
                               struct variable *sought,
                               struct variable *start);

#endif // VARIABLE_H
