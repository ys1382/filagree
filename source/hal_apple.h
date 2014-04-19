#ifndef HAL_APPLE_H
#define HAL_APPLE_H

NSString *byte_array_to_nsstring(const struct byte_array *str);
struct byte_array *nsstring_to_byte_array(const NSString *str);
struct byte_array *read_resource(const char *path);

#endif
