#ifndef HAL_APPLE_H
#define HAL_APPLE_H

NSString *byte_array_to_nsstring(const struct byte_array *str);
struct byte_array *nsstring_to_byte_array(const NSString *str);
struct byte_array *read_resource(const char *path);
struct variable *o2f(struct context *context, NSObject *o);
NSObject *f2o(struct context *context, struct variable *f);

@interface NSFList : NSObject

@property (strong, nonatomic) NSMutableArray *ordered;
@property (strong, nonatomic) NSMutableDictionary *map;

@end

#endif
