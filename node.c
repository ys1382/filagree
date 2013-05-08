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


struct thread_argument {
    struct context *context;
    struct variable *listener;
    int fd;
    const char *event;
    char *serveraddr;
    char buf[MAXLINE];
    ssize_t length;
    uint16_t serverport;
};

struct thread_argument *thread_new(struct context *context, find_c_var *find)
{
    struct thread_argument *ta = (struct thread_argument *)malloc(sizeof(struct thread_argument));
    ta->context = (context != NULL) ? context : context_new(true, true, find);
    return ta;
}

static struct map *server_listeners = NULL; // maps port to listener


static bool int_compare(const void *a, const void *b, void *context)
{
    int32_t i = (VOID_INT)a;
    int32_t j = (VOID_INT)b;
    return i == j;
}

static int32_t int_hash(const void* x, void *context) {
    return (int32_t)(VOID_INT)x;
}

void *int_copy(const void *x, void *context) { return (void*)x; }

void int_del(const void *x, void *context) {}

void callback(struct thread_argument *ta, struct variable *message)
{
    struct byte_array *key = byte_array_from_string(ta->event);
    struct variable *key2 = variable_new_str(ta->context, key);
    struct variable *callback = variable_map_get(ta->context, ta->listener, key2);
    if ((callback != NULL) && (callback->type != VM_NIL)) {
        struct variable *id = variable_new_int(ta->context, ta->fd);
        vm_call(ta->context, callback, ta->listener, id, message);
    } else {
        char buf[1000];
        DEBUGPRINT("could not find %s in %s", ta->event, variable_value_str(ta->context, ta->listener, buf));
    }
    variable_del(ta->context, key2);
}

// callback to client when connection is opened (or fails)
void *connected(void *arg)
{
    struct thread_argument *ta = (struct thread_argument *)arg;
    ta->event = "connected";
    callback(arg, NULL);
    return NULL;
}

// callback to client or server when message arrives on socket
void *incoming(void *arg)
{
    struct thread_argument *ta = (struct thread_argument *)arg;
    struct byte_array *raw_message = byte_array_new_size(ta->length);
    raw_message->data = raw_message->current = (uint8_t*)ta->buf;
    raw_message->length = ta->length;
    struct variable *message = variable_deserialize(ta->context, raw_message);

    callback(ta, message);

	return NULL;
}

// listens for incoming connections
void *sys_listen2(void *arg)
{
    struct thread_argument *ta0 = (struct thread_argument*)arg;
    struct variable *listener = ta0->listener;
    int serverport = ta0->serverport;

    if (server_listeners == NULL)
        server_listeners = map_new_ex(NULL, &int_compare, &int_hash, &int_copy, &int_del);

    map_insert(server_listeners, (void*)(VOID_INT)serverport, listener);


	int					i, maxi, maxfd, listenfd, connfd, sockfd;
	int					nready, client[FD_SETSIZE];
	fd_set				rset, allset;
	char				buf[MAXLINE];
	socklen_t			clilen;
	struct sockaddr_in	cliaddr, servaddr;

	if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        printf("could not create socket\n");
        return NULL;;
    }

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family      = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port        = htons(serverport);

	if (bind(listenfd, (struct sockaddr*) &servaddr, sizeof(servaddr))) {
        printf("could not bind\n");
        return NULL;
    }
	if (listen(listenfd, SOMAXCONN)) {
        printf("could not listen\n");
        return NULL;
    }

	maxfd = listenfd;			// initialize
	maxi = -1;					// index into client[] array
	for (i = 0; i < FD_SETSIZE; i++)
		client[i] = -1;			// -1 indicates available entry
	FD_ZERO(&allset);
	FD_SET(listenfd, &allset);

	for (
        ;;) {

        printf("listen on %d\n", ta0->serverport);

		rset = allset;		// structure assignment
		nready = select(maxfd+1, &rset, NULL, NULL, NULL);

		if (FD_ISSET(listenfd, &rset)) {	// new client connection
			clilen = sizeof(cliaddr);
			connfd = accept(listenfd, (struct sockaddr*) &cliaddr, &clilen);

			for (i = 0; i < FD_SETSIZE; i++) {
				if (client[i] < 0) {
					client[i] = connfd;	// save descriptor
					break;
				}
            }

			if (i == FD_SETSIZE)
				exit(1); //exit_message("too many clients");

			FD_SET(connfd, &allset);	// add new descriptor to set
			if (connfd > maxfd)
				maxfd = connfd;			// for select
			if (i > maxi)
				maxi = i;				// max index in client[] array

            struct thread_argument *ta = thread_new(NULL, ta0->context->find);
            ta->listener = listener;
            ta->fd = connfd;
            pthread_t child;
            pthread_create(&child, NULL, connected, ta);

			if (--nready <= 0)
				continue;				// no more readable descriptors
		}

		for (i = 0; i <= maxi; i++) {	// check all clients for data

			ssize_t	n;

            if ( (sockfd = client[i]) < 0)
				continue;
			if (FD_ISSET(sockfd, &rset)) {
				if ( (n = read(sockfd, buf, MAXLINE)) == 0) {
                    // connection closed by client
					close(sockfd);
					FD_CLR(sockfd, &allset);
					client[i] = -1;
				} else {

                    struct thread_argument *ta = thread_new(NULL, ta0->context->find);
                    ta->listener = listener;
                    ta->fd = connfd;
                    memcpy((void*)ta->buf, buf, n);
                    ta->length = n;
                    ta->event = "messaged";
                    pthread_t child;
                    pthread_create(&child, NULL, incoming, ta);
                }
				
                if (--nready <= 0)
					break;				// no more readable descriptors
			}
		}
	}
}

// server listens for clients opening connections
struct variable *sys_listen(struct context *context)
{
    struct variable *arguments = (struct variable*)stack_pop(context->operand_stack);
    struct thread_argument *ta = thread_new(NULL, context->find);
    ta->serverport = param_int(arguments, 1);
    ta->listener = param_var(ta->context, arguments, 2);

    pthread_t child;
    pthread_create(&child, NULL, sys_listen2, ta);

    return NULL;
}

void *sys_connect2(void *arg)
{
    int result = 0;
    struct thread_argument *ta = (struct thread_argument *)arg;

    sleep(1);

    int sockfd;
	struct sockaddr_in servaddr;

	// Create Socket file descriptor
	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(ta->serverport);
	inet_pton(AF_INET, ta->serveraddr, &servaddr.sin_addr);

    printf("connect to %s:%d\n", ta->serveraddr, ta->serverport);

	// Blocking Connect to socket file descriptor
	if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)))
    {
        ta->event = "error";
        printf("error %d\n", errno);
        result = errno;
        struct variable *result2 = variable_new_int(ta->context, result);
        callback(ta, result2);

    } else {

        ta->fd = sockfd;
        connected(ta);

        for (;;) {

            char buf[MAXLINE];
            int n = recv(sockfd, buf, sizeof(buf), 0);
            if (n == -1)
            {
                perror("recv");
                close(sockfd);
                struct variable *result2 = variable_new_int(ta->context, errno);
                callback(ta, result2);

            } else {

                memcpy((void*)ta->buf, buf, n);
                ta->length = n;
                ta->event = "messaged";
                pthread_t child;
                pthread_create(&child, NULL, incoming, ta);

            }
        }
    }

    return NULL;
}

// client opens a socket with server
struct variable *sys_connect(struct context *context)
{
    struct variable *arguments = (struct variable*)stack_pop(context->operand_stack);

    struct thread_argument *ta = thread_new(NULL, context->find);
    ta->serveraddr = param_str(arguments, 1);
    ta->serverport = param_int(arguments, 2);
    ta->listener   = param_var(ta->context, arguments, 3);

    pthread_t child;
    pthread_create(&child, NULL, sys_connect2, (void*)ta);

    return NULL;
}

void *sys_send2(void *arg)
{
    struct thread_argument *ta = (struct thread_argument *)arg;

    if (write(ta->fd, ta->buf, strlen(ta->buf)) != strlen(ta->buf)) {
        struct variable *problem = variable_new_str(ta->context, byte_array_from_string("write error"));
        callback(ta, problem);
    } else {
        DEBUGPRINT("wrote to socket \n");
    }
    return NULL;
}

// client or server send a message on a socket
struct variable *sys_send(struct context *context)
{
    struct variable *arguments = (struct variable*)stack_pop(context->operand_stack);
    struct thread_argument *ta = thread_new(NULL, context->find);
    ta->fd = param_int(arguments, 1);

    struct variable *v = param_var(ta->context, arguments, 2);
    struct byte_array *v2 = variable_serialize(ta->context, NULL, v, true);
    memcpy(ta->buf, v2->data, v2->length);
    ta->length = v2->length;
    ta->listener = param_var(ta->context, arguments, 3);
    ta->event = "sent";

    pthread_t child;
    pthread_create(&child, NULL, sys_send2, (void*)ta);

    return NULL;
}

void *sys_disconnect2(void *arg)
{
    struct thread_argument *ta = (struct thread_argument *)arg;

    if (close(ta->fd)) {
        struct variable *problem = variable_new_str(ta->context, byte_array_from_string("close error"));
        callback(ta, problem);
    }
    return NULL;
}

// close socket[s]
struct variable *sys_disconnect(struct context *context)
{
    struct variable *arguments = (struct variable*)stack_pop(context->operand_stack);

    struct thread_argument *ta = thread_new(NULL, context->find);
    ta->listener = param_var(ta->context, arguments, 1);
    ta->event = "close";

    if (arguments->list->length < 2)
    {
        struct variable *sockets = (struct variable *)array_get(arguments->list, 1);
        assert_message(sockets->type == VAR_LST, "non list of sockets");
        for (int i=0; i<sockets->list->length; i++)
        {
            struct thread_argument *tc = (struct thread_argument *)array_get(sockets->list, i);
            ta->fd = tc->fd; //close(tc->fd);
        }
    }
    else
    {
        ta->fd = param_int(arguments, 1);
    }

    pthread_t child;
    pthread_create(&child, NULL, sys_disconnect2, (void*)ta);

    return NULL;
}
