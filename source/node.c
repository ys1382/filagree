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

enum THREAD_USE
{
    THREAD_SEND,
    THREAD_RECV,
    THREAD_NUM_USES
};

struct node_thread
{
    struct context *context;
    struct variable *listener;
	struct sockaddr_in servaddr;
    enum HAL_Event event;
    struct array *bufs;
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
    ta->bufs = NULL;
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


// each thread has an array of byte_array messages, to be sent/received in order
static inline struct array *socket_buf(struct node_thread *thread, enum THREAD_USE use) {
    return array_get(thread->bufs, use);
}


// runs the start_routine function in the new thread
void *thread_wrapper(void *param)
{
    struct node_thread *ta = (struct node_thread *)param;
    struct context_shared *s = ta->context->singleton;
    
    ta->start_routine(ta);
    
    gil_lock(ta->context, "thread_wrapper b");
    s->num_threads--;
    pthread_cond_signal(&s->thread_cond);
    gil_unlock(ta->context, "thread_wrapper b");
    
    return NULL;
}


// creates a thread to run the handler function in the thread_wrapper
void start_thread(struct node_thread *ta)
{
    pthread_t *tid = malloc(sizeof(pthread_t));
    if (NULL == tid)
    {
        perror("malloc");
        return;
    }
    
    struct context_shared *s = ta->context->singleton;
    s->num_threads++;
    
    if (pthread_create(tid, NULL, &thread_wrapper, (void*)ta))
    {
        perror("pthread_create");
        s->num_threads--;
        pthread_cond_signal(&s->thread_cond);
    }
}


// store message that arrived on socket sockfd
// one should lock gil before queue fiddling
void enqueue(struct context *context, int sockfd, enum THREAD_USE use,
             struct byte_array *bytes, struct variable *listener, void *(*start_routine)(void *))
{
    struct array *threads = context->singleton->threads;
    
    // get thread_list
    struct array *thread_list;
    if ((threads->length <= sockfd) || (NULL == (thread_list = array_get(threads, sockfd))))
    {
        thread_list = array_new_size(THREAD_NUM_USES);
        array_set(threads, sockfd, thread_list);
    }
    
    // get thread
    struct node_thread *thread = NULL;
    bool created = false;
    if ((thread_list->length <= use) || (NULL == (thread = array_get(thread_list, use))))
    {
        thread = thread_new(context, listener, start_routine, sockfd);
        thread->bufs = array_new_size(THREAD_NUM_USES);
        for (int i=0; i<THREAD_NUM_USES; i++)
            array_set(thread->bufs, i, array_new());
        thread->bufs->length = THREAD_NUM_USES;
        array_set(thread_list, use, thread);
        created = true;
    }

    // add buf to list
    struct array *bufs = socket_buf(thread, use);
    //printf(">%" PRIu16 " - added %d bytes to %p\n", current_thread_id(), bytes->length, bufs);
    array_add(bufs, bytes);

    if (created)
        start_thread(thread);
}


// get next message that arrived on socket sockfd, or NULL if there are none
struct byte_array *dequeue(struct node_thread *ta, enum THREAD_USE use)
{
    struct array *bufs = socket_buf(ta, use);
    if (0 == bufs->length)
        return NULL;
    struct byte_array *buf = array_get(bufs, 0);
    array_remove(bufs, 0, 1);
    
    return buf;
}

// remove thread from list of threads
void thread_del(struct node_thread *thread, enum THREAD_USE use)
{
    struct array *thread_list = array_get(thread->context->singleton->threads, thread->fd);
    array_set(thread_list, use, NULL);
}

// callback to client or server when message arrives on socket
void *incoming(void *arg)
{
    struct node_thread *ta = (struct node_thread *)arg;
    ta->event = MESSAGED;
    struct context *context = ta->context;
    gil_lock(context, "ncoming");

    struct byte_array *buf;
    while ((buf = dequeue(ta, THREAD_RECV))) // service all incoming messages
    {
        while (buf->current - buf->data < buf->length)
        {
            ta->message = variable_deserialize(context, buf);
            DEBUGPRINT("received %s\n", variable_value_str(context, ta->message));
            gil_unlock(context, "ncoming");
            node_callback(ta);
            gil_lock(context, "ncoming");
        }
    }

    thread_del(ta, THREAD_RECV);
    gil_unlock(context, "ncoming");
    return NULL;
}


// handles socket recv and disconnect
// returns true iff disconnect
bool socket_event(struct node_thread *ta0, struct variable *listener, int fd)
{
    ssize_t n;
    uint8_t buf[MAXLINE];
    struct byte_array *received = byte_array_new();

    do
    {
        n = read(fd, buf, MAXLINE); // read bytes from socket (blocking)

        if (n <= 0)  // disconnected
        {
            struct node_thread *ta = thread_new(ta0->context, listener, node_callback, fd);
            printf("disconnected\n");
            ta->event = DISCONNECTED;
            start_thread(ta);
            return true;
        }

        if (received->length + n > BYTE_ARRAY_MAX_LEN)  // too long
        {
            printf("socket message too long (%d), dropping\n", received->length);
            break;
        }

        if (n > 0) {
            struct byte_array *chunk = byte_array_new_data((int32_t)n, buf);
            byte_array_append(received, chunk);
        }
    }
    while (n == MAXLINE);

    //printf("received %d bytes\n", received->length);

    if (!received->length)
    {
        byte_array_del(received);
        return true;
    }

    // process message in another thread
    byte_array_reset(received);

    gil_lock(ta0->context, "socket_event");
    enqueue(ta0->context, fd, THREAD_RECV, received, listener, &incoming);
    gil_unlock(ta0->context, "socket_event");

    return false;
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
            start_thread(ta);

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
                //printf("\nsocket %d:%d closed by client\n", i, sockfd);
                if (socket_event(ta0, listener, sockfd)) // connection closed by client
                {
					FD_CLR(sockfd, &allset);
					client[i] = -1;
                }

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
    struct variable *listener = param_var(arguments, 2);
    struct node_thread *ta = thread_new(context, listener, &sys_socket_listen2, 0);
    int serverport = param_int(arguments, 1);
    printf("sys_socket_listen on port %d\n", serverport);

	struct sockaddr_in servaddr;

	if ((ta->fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("socket");
        return NULL;
    }

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family      = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port        = htons(serverport);

    int reuse = 1;
    if (setsockopt(ta->fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)))
    {
        printf("error on socket %d\n", ta->fd);
        perror("setsockopt");
        return NULL;
    }
	if (bind(ta->fd, (struct sockaddr*)&servaddr, sizeof(servaddr)))
    {
        perror("bind");
        return NULL;
    }
	if (listen(ta->fd, SOMAXCONN))
    {
        perror("listen");
        return NULL;
    }

    start_thread(ta);

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
    }
    else
    {
        ta->event = CONNECTED;
        node_callback(ta);
        for (;!socket_event(ta, ta->listener, ta->fd););
    }

    return NULL;
}

// client opens a socket with server
struct variable *sys_connect(struct context *context)
{
    struct variable *arguments = (struct variable*)stack_pop(context->operand_stack);
    struct variable *listener = param_var(arguments, 3);
    listener->gc_state = GC_SAFE;
    struct node_thread *ta = thread_new(context, listener, &sys_connect2, 0);
    char *serveraddr = param_str(arguments, 1);
    int serverport = param_int(arguments, 2);
    printf("sys_connect to port %d\n", serverport);

#ifdef __ANDROID__
    if (!strcmp(serveraddr, "127.0.0.1"))
        serveraddr = "10.0.2.2"; // emulator's host IP
#endif

	// create socket file descriptor
	ta->fd = socket(AF_INET, SOCK_STREAM, 0);

	bzero(&ta->servaddr, sizeof(ta->servaddr));
	ta->servaddr.sin_family = AF_INET;
	ta->servaddr.sin_port = htons(serverport);
	inet_pton(AF_INET, serveraddr, &ta->servaddr.sin_addr);

    DEBUGPRINT("connect\n");
    start_thread(ta);

    return NULL;
}

void *sys_send2(void *arg)
{
    struct node_thread *ta = (struct node_thread *)arg;

    struct byte_array *buf;
    while ((buf = dequeue(ta, THREAD_SEND))) // send all outgoing messages for this socket
    {
        if (write(ta->fd, buf->data, buf->length) != buf->length)
            perror("write");
        else
            DEBUGPRINT(">%" PRIu16 " - sent %d bytes to fd %d\n", current_thread_id(), buf->length, ta->fd);
    }
    thread_del(ta, THREAD_SEND);
    return NULL;
}

// client or server send a message on a socket
struct variable *sys_send(struct context *context)
{
    struct variable *arguments = (struct variable*)stack_pop(context->operand_stack);
    struct variable *fd = param_var(arguments, 1);
    vm_assert(context, fd->type == VAR_INT && fd->integer >= 0, "bad fd");
    struct variable *v = param_var(arguments, 2);
    struct variable *listener = param_var(arguments, 3);

    struct byte_array *sending = variable_serialize(context, NULL, v);
    enqueue(context, fd->integer, THREAD_SEND, sending, listener, &sys_send2);

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
