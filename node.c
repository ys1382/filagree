// node.c - socket client and server

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <pthread.h>
#include "util.h"
#include "vm.h"
#include "serial.h"
#include "sys.h"

#define MAXLINE 1024

enum Event {
    CONNECTED,
    DISCONNECTED,
    MESSAGED,
    SENT,
    ERROR,
};

struct number_string node_events[] = {
    {CONNECTED,     "connected"},
    {DISCONNECTED,  "disconnected"},
    {MESSAGED,      "messaged"},
    {SENT,          "sent"},
    {ERROR,         "error"},
};

struct node_thread {
    struct context *context;
    struct variable *listener;
	struct sockaddr_in servaddr;
    struct byte_array *buf;
    enum Event event;
    void *(*start_routine)(void *);
    int fd;
};

static bool int_compare(const void *a, const void *b, void *context)
{
    int32_t i = (VOID_INT)a;
    int32_t j = (VOID_INT)b;
    return  i == j;
}

static int32_t int_hash(const void* x, void *context) {
    return (int32_t)(VOID_INT)x;
}

void *int_copy(const void *x, void *context) { return (void*)x; }

void int_del(const void *x, void *context) {}

struct node_thread *thread_new(struct context *context,
                               struct variable *listener,
                               void *(*start_routine)(void *),
                               int fd)
{
    struct node_thread *ta = (struct node_thread *)malloc(sizeof(struct node_thread));
    ta->context = context_new(true, true, true, context);
    ta->listener = listener; // != NULL ? variable_copy(ta->context, listener) : NULL;
    ta->fd = fd;
    ta->start_routine = start_routine;
    //DEBUGPRINT("thread_new on %p\n", context);
    return ta;
}

void thread_wait_for(pthread_t *thread)
{
    int ptj;
    DEBUGPRINT("\tpthread_join on thread %p\n", thread);
    if ((ptj = pthread_join(*thread, NULL)))
        printf("could not pthread_join %p, error %d\n", thread, ptj);
}

void node_callback(struct node_thread *ta, struct variable *message)
{
    if (ta->listener == NULL)
        return;

    pthread_mutex_lock(&ta->context->singleton->gil);
    
    const char *key = NUM_TO_STRING(node_events, ta->event);
    struct byte_array *key2 = byte_array_from_string(key);
    struct variable *key3 = variable_new_str(ta->context, key2);
    struct variable *callback = variable_map_get(ta->context, ta->listener, key3);
    struct variable *id = variable_new_int(ta->context, ta->fd);
    if (callback != NULL)
        vm_call(ta->context, callback, ta->listener, id, message);

    variable_del(ta->context, key3);

    pthread_mutex_unlock(&ta->context->singleton->gil);
}

// callback to client when connection is opened (or fails)
void *connected(void *arg)
{
    struct node_thread *ta = (struct node_thread *)arg;
    ta->event = CONNECTED;
    node_callback(arg, NULL);
    return NULL;
}

// callback to client or server when message arrives on socket
void *incoming(void *arg)
{
    struct node_thread *ta = (struct node_thread *)arg;

    pthread_mutex_lock(&ta->context->singleton->gil);
    struct variable *message = variable_deserialize(ta->context, ta->buf);
    pthread_mutex_unlock(&ta->context->singleton->gil);

    node_callback(ta, message);
	return NULL;
}

// for use with pthread_join in context_del, so that the context's variables are
// not all freed before the thread is done

void *thread_wrapper(void *param)
{
    struct node_thread *ta = (struct node_thread *)param;
    struct context_shared *s = ta->context->singleton;
    s->num_threads++;

    ta->start_routine(ta);

    pthread_cond_signal(&s->thread_cond);
    pthread_mutex_unlock(&s->gil);
    s->num_threads--;
    return NULL;
}

void add_thread(struct node_thread *ta, int sockfd)
{
    pthread_t *tid = malloc(sizeof(pthread_t));
    if (tid == NULL) {
        perror("malloc");
        return;
    }
    if (pthread_create(tid, NULL, &thread_wrapper, (void*)ta)) {
        perror("pthread_create");
        return;
    }

    DEBUGPRINT("add_thread %p to fd %d\n", *tid, sockfd);
}

// listens for inbound connections
void *sys_socket_listen2(void *arg)
{
    struct node_thread *ta0 = (struct node_thread*)arg;
    struct variable *listener = ta0->listener;

	int					i, maxi, maxfd, connfd, sockfd;
	int					nready, client[FD_SETSIZE];
	fd_set				rset, allset;
	char				buf[MAXLINE];
	socklen_t			clilen;
	struct sockaddr_in	cliaddr;

	maxfd = ta0->fd;			// initialize
	maxi = -1;					// index into client[] array
	for (i = 0; i < FD_SETSIZE; i++)
		client[i] = -1;			// -1 indicates available entry
	FD_ZERO(&allset);
	FD_SET(ta0->fd, &allset);

	for (;;) {

		rset = allset;
		nready = select(maxfd+1, &rset, NULL, NULL, NULL);

		if (FD_ISSET(ta0->fd, &rset))   // new client connection
        {
			clilen = sizeof(cliaddr);
			connfd = accept(ta0->fd, (struct sockaddr*) &cliaddr, &clilen);
            if (connfd == -1)
            {
                perror("accept");
                continue;
            }

			for (i = 0; i < FD_SETSIZE; i++)
            {
				if (client[i] < 0) {
					client[i] = connfd;	// save descriptor
					break;
				}
            }

			if (i == FD_SETSIZE)
				exit_message("too many clients");

			FD_SET(connfd, &allset);	// add new descriptor to set
			if (connfd > maxfd)
				maxfd = connfd;			// for select
			if (i > maxi)
				maxi = i;				// max index in client[] array

            struct node_thread *ta = thread_new(ta0->context, listener, connected, connfd);
            add_thread(ta, connfd);

			if (--nready <= 0)
				continue;				// no more readable descriptors
		}

        // check all clients for data
		for (i = 0; i <= maxi; i++)
        {
			ssize_t	n;
            if ( (sockfd = client[i]) < 0)
				continue;

			if (FD_ISSET(sockfd, &rset))
            {
				if ((n = read(sockfd, buf, MAXLINE)) == 0)
                {
                    // connection closed by client
					FD_CLR(sockfd, &allset);
					client[i] = -1;

				} else {

                    struct node_thread *ta = thread_new(ta0->context, listener, incoming, sockfd);
                    ta->buf = byte_array_new_size(n);
                    ta->buf->length = n;
                    memcpy((void*)ta->buf->data, buf, n);
                    ta->event = MESSAGED; // "messaged";
                    DEBUGPRINT("incoming1 %d\n", sockfd);
                    add_thread(ta, 0);
                }
				
                if (--nready <= 0) // no more readable descriptors
					break;
			}
		}
	}
}

// server listens for clients opening connections
struct variable *sys_socket_listen(struct context *context)
{
    struct variable *arguments = (struct variable*)stack_pop(context->operand_stack);
    struct variable *listener = param_var(context, arguments, 2);
    struct node_thread *ta = thread_new(context, listener, &sys_socket_listen2, 0);
    int serverport = param_int(arguments, 1);

	struct sockaddr_in servaddr;

	if ((ta->fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        return NULL;
    }

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family      = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port        = htons(serverport);

	if (bind(ta->fd, (struct sockaddr*) &servaddr, sizeof(servaddr))) {
        perror("bind");
        return NULL;
    }
	if (listen(ta->fd, SOMAXCONN)) {
        perror("listen");
        return NULL;
    }

    add_thread(ta, ta->fd);

    return NULL;
}

void *sys_connect2(void *arg)
{
    int result = 0;
    struct node_thread *ta = (struct node_thread *)arg;

    // Blocking connect to socket file descriptor
	if (connect(ta->fd, (struct sockaddr *)&ta->servaddr, sizeof(ta->servaddr)))
    {
        ta->event = ERROR;
        printf("error %d\n", errno);
        result = errno;
        struct variable *result2 = variable_new_int(ta->context, result);
        ta->event = ERROR;
        node_callback(ta, result2);

    } else {

        connected(ta);   // callback

        for (;;) {

            char buf[MAXLINE];
            ssize_t n = read(ta->fd, buf, sizeof(buf)); // read from the socket
            switch (n) {
                case 0:
                    return NULL;
                case -1:
                    break;
                default:
                    ta->buf = byte_array_new_size(n);
                    ta->buf->length = n;
                    memcpy((void*)ta->buf->data, buf, n);
                    ta->event = MESSAGED;
                    DEBUGPRINT("incoming2 %d\n", ta->fd);
                    ta->start_routine = incoming;
                    add_thread(ta, 0);
                    break;
            }
        }
    }

    return NULL;
}

// client opens a socket with server
struct variable *sys_connect(struct context *context)
{
    struct variable *arguments = (struct variable*)stack_pop(context->operand_stack);
    struct variable *listener   = param_var(context, arguments, 3);
    struct node_thread *ta = thread_new(context, listener, &sys_connect2, 0);
    char *serveraddr = param_str(arguments, 1);
    int serverport = param_int(arguments, 2);

	// create socket file descriptor
	ta->fd = socket(AF_INET, SOCK_STREAM, 0);

	bzero(&ta->servaddr, sizeof(ta->servaddr));
	ta->servaddr.sin_family = AF_INET;
	ta->servaddr.sin_port = htons(serverport);
	inet_pton(AF_INET, serveraddr, &ta->servaddr.sin_addr);

    DEBUGPRINT("connect\n");
    add_thread(ta, ta->fd);

    return NULL;
}

void *sys_send2(void *arg)
{
    struct node_thread *ta = (struct node_thread *)arg;

    if (write(ta->fd, ta->buf->data, ta->buf->length) != ta->buf->length) {
        DEBUGPRINT("write error\n");
        struct variable *problem = variable_new_str(ta->context, byte_array_from_string("write error"));
        node_callback(ta, problem);
    } else {
        DEBUGPRINT("sent to %d\n", ta->fd);
        node_callback(ta, NULL); // sent
    }
    return NULL;
}

// client or server send a message on a socket
struct variable *sys_send(struct context *context)
{
    struct variable *arguments = (struct variable*)stack_pop(context->operand_stack);
    int fd = param_int(arguments, 1);
    assert_message(fd < 10, "bad fd");
    struct variable *listener = param_var(context, arguments, 3);

    struct node_thread *ta = thread_new(context, listener, &sys_send2, fd);

    struct variable *v = param_var(ta->context, arguments, 2);
    ta->buf = variable_serialize(ta->context, NULL, v);
    ta->event = SENT;

    // debug double-check
    byte_array_reset(ta->buf);
    struct variable *message = variable_deserialize(ta->context, ta->buf);
    null_check(message);

    DEBUGPRINT("listen\n");
    add_thread(ta, 0);

    return NULL;
}

// close socket[s]
struct variable *sys_disconnect(struct context *context)
{
    struct variable *arguments = (struct variable*)stack_pop(context->operand_stack);
    //struct variable *listener = param_var(context, arguments, arguments->list->length-1);
    int fd = param_int(arguments, 1);
    //disconnect_fd(context, fd, listener);
    if (close(fd))
        perror("close");
    return NULL;
}
