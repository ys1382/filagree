#import <Foundation/Foundation.h>

#include "struct.h"
#include "util.h"
#include "variable.h"
#include "vm.h"
#include "hal.h"

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
