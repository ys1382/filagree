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
#include "hal.h"

#define MAXLINE 1024

struct node_thread {
    struct context *context;
    struct variable *listener;
	struct sockaddr_in servaddr;
    struct byte_array *buf;
    enum HAL_Event event;
    struct variable *message;
    void *(*start_routine)(void *);
    int fd;
};

uint16_t current_thread_id() {
    return ((unsigned int)(VOID_INT)pthread_self() >> 12) & 0xFFF;
}

struct node_thread *thread_new(struct context *context,
                               struct variable *listener,
                               void *(*start_routine)(void *),
                               int fd)
{
    struct node_thread *ta = (struct node_thread *)malloc(sizeof(struct node_thread));
    ta->context = context_new(context, true, true);
    ta->listener = listener;
    if (NULL != listener)
        listener->gc_state = GC_SAFE;

    ta->fd = fd;
    ta->message = NULL;
    ta->buf = NULL;
    ta->start_routine = start_routine;
    return ta;
}

void *node_callback(void *arg)
{
    struct node_thread *ta = (struct node_thread *)arg;
    if (NULL == ta->listener)
        return NULL;
    gil_lock(ta->context, "node_callback");

    struct variable *key = event_string(ta->context, ta->event);
    struct variable *callback = variable_map_get(ta->context, ta->listener, key);
    struct variable *id = variable_new_int(ta->context, ta->fd);
    if (NULL != callback)
        vm_call(ta->context, callback, ta->listener, id, ta->message, NULL);

    gil_unlock(ta->context, "node_callback");
    return NULL;
}

// callback to client or server when message arrives on socket
void *incoming(void *arg)
{
    struct node_thread *ta = (struct node_thread *)arg;

    gil_lock(ta->context, "ncoming");
    ta->message = variable_deserialize(ta->context, ta->buf);

#ifdef DEBUG
//    char buf[1000];
//    DEBUGPRINT("received %s\n", variable_value_str(ta->context, ta->message, buf));
#endif

    gil_unlock(ta->context, "ncoming");

    node_callback(ta);
	return NULL;
}

void *thread_wrapper(void *param)
{
    struct node_thread *ta = (struct node_thread *)param;
    struct context_shared *s = ta->context->singleton;

    gil_lock(ta->context, "thread_wrapper");
    s->num_threads++;
    gil_unlock(ta->context, "thread_wrapper");

    ta->start_routine(ta);

    gil_lock(ta->context, "thread_wrapper b");
    s->num_threads--;
    pthread_cond_signal(&s->thread_cond);
    gil_unlock(ta->context, "thread_wrapper b");

    return NULL;
}

void add_thread(struct node_thread *ta, int sockfd)
{
    pthread_t *tid = malloc(sizeof(pthread_t));
    if (NULL == tid) {
        perror("malloc");
        return;
    }
    if (pthread_create(tid, NULL, &thread_wrapper, (void*)ta)) {
        perror("pthread_create");
        return;
    }
}

void messaged(struct node_thread *ta0, struct variable *listener, int sockfd, uint8_t *buf, ssize_t n)
{
    struct node_thread *ta = thread_new(ta0->context, listener, incoming, sockfd);
    ta->buf = byte_array_new_size((uint32_t)n);
    ta->buf->length = (uint32_t)n;
    memcpy((void*)ta->buf->data, buf, n);
    ta->event = MESSAGED;
    DEBUGPRINT("\n>%" PRIu16 " - msgd %p\n", current_thread_id(), ta->context);
    add_thread(ta, 0);
}

// listens for inbound connections
void *sys_socket_listen2(void *arg)
{
    struct node_thread *ta0 = (struct node_thread*)arg;
    struct variable *listener = ta0->listener;
    listener->gc_state = GC_SAFE;

	int					i, maxi, maxfd, connfd, sockfd;
	int					nready, client[FD_SETSIZE];
	fd_set				rset, allset;
	uint8_t				buf[MAXLINE];
	socklen_t			clilen;
	struct sockaddr_in	cliaddr;

	maxfd = ta0->fd;			// initialize
	maxi = -1;					// index into client[] array
	for (i = 0; i < FD_SETSIZE; i++)
		client[i] = -1;			// -1 indicates available entry
	FD_ZERO(&allset);
	FD_SET(ta0->fd, &allset);

	for (;;)
    {
        DEBUGPRINT("server listen on socket\n");

		rset = allset;
		nready = select(maxfd+1, &rset, NULL, NULL, NULL);

        DEBUGPRINT("client connected\n");

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
				if (client[i] < 0)
                {
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

            struct node_thread *ta = thread_new(ta0->context, listener, node_callback, connfd);
            ta->event = CONNECTED;
            add_thread(ta, connfd);

			if (--nready <= 0)
				continue;				// no more readable descriptors
		}

        // check all clients for data
		for (i = 0; i <= maxi; i++)
        {
            if ( (sockfd = client[i]) < 0)
				continue;

			if (FD_ISSET(sockfd, &rset))
            {
                ssize_t n;
				if ((n = read(sockfd, buf, MAXLINE)) <= 0)
                {
                    // connection closed by client
					FD_CLR(sockfd, &allset);
					client[i] = -1;

                    struct node_thread *ta = thread_new(ta0->context, listener, node_callback, sockfd);
                    ta->event = DISCONNECTED;
                    add_thread(ta, 0);

				} else
                    DEBUGPRINT("\nmessagd1\n");
                    messaged(ta0, listener, sockfd, buf, n);

                if (--nready <= 0) // no more readable descriptors
					break;
			}
		}
	}
    return NULL;
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

    int reuse = 1;
    setsockopt(ta->fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int));
	if (bind(ta->fd, (struct sockaddr*)&servaddr, sizeof(servaddr))) {
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
    struct node_thread *ta = (struct node_thread *)arg;

    // Blocking connect to socket file descriptor
	if (connect(ta->fd, (struct sockaddr *)&ta->servaddr, sizeof(ta->servaddr)))
    {
        perror("connect");
        return NULL;

    } else {

        ta->event = CONNECTED;
        node_callback(ta);

        for (;;)
        {
            uint8_t buf[MAXLINE];
            ssize_t n = read(ta->fd, buf, sizeof(buf)); // read from the socket
            if (n <= 0)
                    return NULL;
            printf("\nmessaged2\n");
            messaged(ta, ta->listener, ta->fd, buf, n);
        }
    }

    return NULL;
}

// client opens a socket with server
struct variable *sys_connect(struct context *context)
{
    struct variable *arguments = (struct variable*)stack_pop(context->operand_stack);
    struct variable *listener = param_var(context, arguments, 3);
    listener->gc_state = GC_SAFE;
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
        printf("write error\n");
    } else {
        printf("sent to %d\n", ta->fd);
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

    add_thread(ta, 0);

    return NULL;
}

// close socket
struct variable *sys_disconnect(struct context *context)
{
    struct variable *arguments = (struct variable*)stack_pop(context->operand_stack);
    int fd = param_int(arguments, 1);
    if (close(fd))
        perror("close");
    return NULL;
}
