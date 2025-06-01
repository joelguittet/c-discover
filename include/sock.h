/**
 * @file      sock.h
 * @brief     Creation and handling of the sockets
 *
 * MIT License
 *
 * Copyright joelguittet and c-discover contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef __SOCK_H__
#define __SOCK_H__

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************/
/* Includes                                                                   */
/******************************************************************************/

#include <stdint.h>
#include <stdbool.h>
#include <semaphore.h>

/******************************************************************************/
/* Definitions                                                                */
/******************************************************************************/

/* Sock worker structure */
struct sock_s;
typedef struct sock_worker_s {
    struct sock_s        *parent; /* Parent sock instance */
    struct sock_worker_s *prev;   /* Previous worker instance */
    struct sock_worker_s *next;   /* Next worker instance */
    pthread_t             thread; /* Thread handle of the worker */
    union {
        struct {
            int    socket; /* Listenner socket */
            fd_set fds;    /* Listenner FDs (myself) */
        } listenner;
        struct {
            char     ip[15 + 1]; /* Messenger IP address of the sender */
            uint16_t port;       /* Messenger port of the sender */
            void    *buffer;     /* Messenger buffer */
            size_t   size;       /* Messenger buffer size */
        } messenger;
        struct {
            void  *buffer; /* Sender buffer */
            size_t size;   /* Sender buffer size */
        } sender;
    } type;
} sock_worker_t;

/* Worker daisy chain structure */
typedef struct {
    sock_worker_t *first; /* First worker of the daisy chain */
    sock_worker_t *last;  /* Last worker of the daisy chain */
    sem_t          sem;   /* Semaphore used to protect daisy chain */
} sock_worker_list_t;

/* Sock instance structure */
typedef struct sock_s {
    struct {
        char         *address;       /* Address to bind to */
        uint16_t      port;          /* Port on which to bind and communicate with other node-discover processes */
        char         *broadcast;     /* Broadcast address if using broadcast */
        char         *multicast;     /* Multicast address if using multicast - If net set, broadcast or unicast is used */
        unsigned char multicast_ttl; /* Multicast TTL for when using multicast */
        char *
            unicast; /* Comma separated string of Unicast addresses of known nodes - It is advised to specify the address of the local interface when using unicast and expecting local discovery to work*/
        bool reuse_addr; /* Allow multiple processes on the same host to bind to the same address and port */
    } options;
    sock_worker_list_t listenners; /* List of listenners */
    sock_worker_list_t messengers; /* List of messengers */
    sock_worker_list_t senders;    /* List of senders */
    struct {
        fd_set fds; /* All clients sockets */
        sem_t  sem; /* Semaphore used to protect clients */
    } clients;
    struct {
        struct {
            void (*fct)(struct sock_s *, char *, uint16_t, void *, size_t, void *); /* Callback function invoked when message is received */
            void *user;                                                             /* User data passed to the callback */
        } message;
        struct {
            void (*fct)(struct sock_s *, char *, void *); /* Callback function invoked when an error occured*/
            void *user;                                   /* User data passed to the callback */
        } error;
    } cb;
} sock_t;

/******************************************************************************/
/* Prototypes                                                                 */
/******************************************************************************/

/**
 * @brief Function used to create a sock instance
 * @return Sock instance if the function succeeded, NULL otherwise
 */
sock_t *sock_create(void);

/**
 * @brief Bind a new socket to the wanted port, unicast configuration
 * @param sock Sock instance
 * @param address Address to bind to
 * @param port Port
 * @param reuse_addr Reuse address flag
 * @param unicast Unicast addresses, separated by a comma
 * @return 0 if the function succeeded, -1 otherwise
 */
int sock_bind_unicast(sock_t *sock, char *address, uint16_t port, bool reuse_addr, char *unicast);

/**
 * @brief Bind a new socket to the wanted port, unicast configuration
 * @param sock Sock instance
 * @param address Address to bind to
 * @param port Port
 * @param reuse_addr Reuse address flag
 * @param multicast Multicast address
 * @param multicast_ttl Multicast TTL value
 * @return 0 if the function succeeded, -1 otherwise
 */
int sock_bind_multicast(sock_t *sock, char *address, uint16_t port, bool reuse_addr, char *multicast, unsigned char multicast_ttl);

/**
 * @brief Bind a new socket to the wanted port, unicast configuration
 * @param sock Sock instance
 * @param address Address to bind to
 * @param port Port
 * @param reuse_addr Reuse address flag
 * @param broadcast Broadcast address
 * @return 0 if the function succeeded, -1 otherwise
 */
int sock_bind_broadcast(sock_t *sock, char *address, uint16_t port, bool reuse_addr, char *broadcast);

/**
 * @brief Register callbacks
 * @param sock Sock instance
 * @param topic Topic
 * @param fct Callback function
 * @param user User data
 * @return 0 if the function succeeded, -1 otherwise
 */
int sock_on(sock_t *sock, char *topic, void *fct, void *user);

/**
 * @brief Function used to send data
 * @param sock Sock instance
 * @param buffer Buffer to be sent
 * @param size Size of buffer to send
 * @return 0 if the function succeeded, -1 otherwise
 */
int sock_send(sock_t *sock, void *buffer, size_t size);

/**
 * @brief Release sock instance
 * @param sock Sock instance
 */
void sock_release(sock_t *sock);

#ifdef __cplusplus
}
#endif

#endif /* __SOCK_H__ */
