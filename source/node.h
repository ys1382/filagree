#ifndef NODE_H
#define NODE_H

#include <pthread.h>
#include "vm.h"

enum HAL_Event {
    CONNECTED,
    DISCONNECTED,
    MESSAGED,
    SENT,
    FILED,
    ERROR,
};

struct variable *sys_socket_listen(struct context *context);
struct variable *sys_connect(struct context *context);
struct variable *sys_send(struct context *context);
struct variable *sys_disconnect(struct context *context);
uint16_t current_thread_id(void);

#endif // NODE_H
