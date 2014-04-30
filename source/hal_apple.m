#import <Foundation/Foundation.h>

#include "struct.h"
#include "util.h"
#include "variable.h"
#include "vm.h"
#include "hal.h"
#include "hal_apple.h"


NSString *byte_array_to_nsstring(const struct byte_array *str)
{
    const char *str2 = byte_array_to_string(str);
    return [NSString stringWithUTF8String:str2];
}

struct byte_array *nsstring_to_byte_array(const NSString *str)
{
    const char *str2 = [str UTF8String];
    return byte_array_from_string(str2);
}

struct byte_array *read_resource(const char *path)
{
    NSString *path2 = [NSString stringWithUTF8String:path];
    NSString *resource = [path2 stringByDeletingPathExtension];
    NSString *type = [path2 pathExtension];
    NSString* path3 = [[NSBundle mainBundle] pathForResource:resource ofType:type inDirectory:@""];
    if (!path3) {
        NSLog(@"can't load file");
        return NULL;
    }
    NSError *error = nil;
    NSString* contents = [NSString stringWithContentsOfFile:path3 encoding:NSUTF8StringEncoding error:&error];
    if (error) {
        NSLog(@"%@", [error localizedDescription]);
        return NULL;
    }
    
    const char *contents2 = [contents UTF8String];
    return byte_array_from_string(contents2);
}



struct variable *nsstring_to_variable(struct context *context, NSString *str)
{
    struct byte_array *str2 = nsstring_to_byte_array(str);
    return variable_new_str(context, str2);
}

struct variable *nsnumber_to_variable(struct context *context, NSNumber *num) {
    return variable_new_int(context, [num intValue]);
}

struct variable *nsarray_to_variable(struct context *context, NSArray *array)
{
    struct array *array2 = array_new();
    for (NSObject *o in array)
    {
        struct variable *f = o2f(context, o);
        array_add(array2, f);
    }
    return variable_new_list(context, array2);
}

struct variable *nsdictionary_to_variable(struct context *context, NSDictionary *dict)
{
    struct variable *map = variable_new_list(context, NULL);
    
    NSEnumerator *enumerator = [dict keyEnumerator];
    NSObject *key;
    while (key = [enumerator nextObject])
    {
        NSObject *val = [dict objectForKey:key];
        struct variable *key2 = o2f(context, key);
        struct variable *val2 = o2f(context, val);
        variable_map_insert(context, map, key2, val2);
    }
    return map;
}

struct variable *o2f(struct context *context, NSObject *o)
{
    if ([o isMemberOfClass:[NSString class]])
        return nsstring_to_variable(context, (NSString*)o);
    if ([o isMemberOfClass:[NSNumber class]])
        return nsnumber_to_variable(context, (NSNumber*)o);
    if ([o isMemberOfClass:[NSArray class]])
        return nsarray_to_variable(context, (NSArray*)o);
    if ([o isMemberOfClass:[NSDictionary class]])
        return nsdictionary_to_variable(context, (NSDictionary*)o);
    vm_exit_message(context, "o2f unknown type");
    return NULL;
}


/////// NSFList: ObjC representation of filagree list

@interface NSFList : NSObject

@property (strong, nonatomic) NSMutableArray *ordered;
@property (strong, nonatomic) NSMutableDictionary *map;

@end

@implementation NSFList

@synthesize ordered, map;

+ (NSFList*)flistWithList:(struct variable *)f inContext:(struct context *)context
{
    NSFList *flist = [NSFList alloc];
    flist.ordered = [NSMutableArray arrayWithCapacity:f->list.ordered->length];
    
    // ordered
    for (int i=0; i<f->list.ordered->length; i++)
    {
        struct variable *fi = array_get(f->list.ordered, i);
        NSObject *o = f2o(context, fi);
        [flist.ordered addObject:o];
    }

    //map
    struct array *keys = map_keys(f->list.map);
    struct array *vals = map_vals(f->list.map);
    flist.map = [NSMutableDictionary dictionaryWithCapacity:keys->length];
    for (int i=0; i<keys->length; i++)
    {
        struct variable *key = array_get(keys, i);
        struct variable *val = array_get(vals, i);
        vm_assert(context, VAR_STR == key->type, "non-string key");
        NSString *key2 = (NSString*)f2o(context, key);
        NSObject *val2 = f2o(context, val);
        [flist.map setObject:val2 forKey:key2];
    }
    return flist;
}

@end

NSObject *f2o(struct context *context, struct variable *f)
{
    if (NULL != f)
        return NULL;

    switch ((f->type)) {
        case VAR_NIL:   return nil;
        case VAR_INT:   return [NSNumber numberWithInt:f->integer];
        case VAR_FLT:   return [NSNumber numberWithFloat:f->floater];
        case VAR_BOOL:  return [NSNumber numberWithBool:f->boolean];
        case VAR_STR:   return [NSString stringWithUTF8String:byte_array_to_string(f->str)];
        case VAR_LST:   return [NSFList flistWithList:f inContext:context];
        default:
            vm_exit_message(context, "f2o unsupported type", f->type);
            break;
    }
    return NULL;
}



/*
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
*/

