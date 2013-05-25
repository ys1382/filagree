#ifndef NODE_H
#define NODE_H

#include <pthread.h>

struct variable *sys_socket_listen(struct context *context);
struct variable *sys_connect(struct context *context);
struct variable *sys_send(struct context *context);
struct variable *sys_disconnect(struct context *context);

typedef struct thread_argument *thread_p;
void thread_wait_for(pthread_t *thread);

#endif // NODE_H