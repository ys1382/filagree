//
//  IMclient.m
//  

#import "IMclient.h"

#include "interpret.h"
#include "hal_apple.h"
#include "compile.h"


@implementation IMclient

static struct context *context = NULL;
static struct variable *filagree_object = NULL;
static IMclient *imc = NULL;


+ (id)shared
{
    if (NULL != imc)
        return imc;
    imc = [IMclient alloc];

    struct byte_array *ui = read_resource("ui.fg");
    struct byte_array *mesh = read_resource("mesh.fg");
    struct byte_array *client = read_resource("im_client.fg");
    struct byte_array *args = byte_array_from_string("id='IOS'");
    struct byte_array *script = byte_array_concatenate(4, ui, mesh, args, client);
    
    struct byte_array *program = build_string(script, NULL);
    context = context_new(NULL, true, true);
    execute_with(context, program, true);

    return imc;
}


- (void)signup:(NSString*)username withPassword:(NSString*)password
{
    NSString *code = [NSString stringWithFormat:@"client.signup('%@','%@')", username, password];
    interpret_string_with(context, nsstring_to_byte_array(code));
}

- (void)signin:(NSString*)username withPassword:(NSString*)password
{
    NSString *code = [NSString stringWithFormat:@"client.signin('%@','%@')", username, password];
    interpret_string_with(context, nsstring_to_byte_array(code));
}


struct variable *nsstring_to_variable(NSString *str)
{
    struct byte_array *str2 = nsstring_to_byte_array(str);
    return variable_new_str(context, str2);
}

struct variable *nsnumber_to_variable(NSNumber *num) {
    return variable_new_int(context, [num intValue]);
}

struct variable *nsarray_to_variable(NSArray *array)
{
    struct array *array2 = array_new();
    for (NSObject *o in array)
    {
        struct variable *f = o2f(o);
        array_add(array2, f);
    }
    return variable_new_list(context, array2);
}

struct variable *nsdictionary_to_variable(NSDictionary *dict)
{
    struct variable *map = variable_new_list(context, NULL);
    
    NSEnumerator *enumerator = [dict keyEnumerator];
    NSObject *key;
    while (key = [enumerator nextObject])
    {
        NSObject *val = [dict objectForKey:key];
        struct variable *key2 = o2f(key);
        struct variable *val2 = o2f(val);
        variable_map_insert(context, map, key2, val2);
    }
    return map;
}

struct variable *o2f(NSObject *o)
{
    if ([o isMemberOfClass:[NSString class]])
        return nsstring_to_variable((NSString*)o);
    if ([o isMemberOfClass:[NSNumber class]])
        return nsnumber_to_variable((NSNumber*)o);
    if ([o isMemberOfClass:[NSArray class]])
        return nsarray_to_variable((NSArray*)o);
    if ([o isMemberOfClass:[NSDictionary class]])
        return nsdictionary_to_variable((NSDictionary*)o);
    vm_exit_message(context, "o2f unknown type");
    return NULL;
}

- (void)forwardInvocation:(NSInvocation *)invocation
{
    NSString *name = NSStringFromSelector([invocation selector]);
    struct byte_array *name2 = nsstring_to_byte_array(name);
    struct variable *name3 = variable_new_str(context, name2);
    struct variable *func = lookup(context, filagree_object, name3);
    if (NULL == func)
        [super forwardInvocation:invocation];
    else {
        NSMethodSignature *sig = [invocation methodSignature];
        struct variable *src = variable_new_src(context, 0);
        struct array *args = src->list.ordered;
        for (int i=0; i< [sig numberOfArguments]; i++) {
            NSObject *arg = [[NSObject alloc] init];
            [invocation getArgument:(void*)arg atIndex:i];
            struct variable *arg2 = o2f(arg);
            array_insert(args, i, arg2);
        }
        vm_call(context, func, src, NULL);
    }
}


@end
