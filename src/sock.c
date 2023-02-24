/**
 * @file      sock.c
 * @brief     Creation and handling of the sockets
 *
 * MIT License
 *
 * Copyright (c) 2021-2023 joelguittet and c-discover contributors
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

/******************************************************************************/
/* Includes                                                                   */
/******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <semaphore.h>
#include <pthread.h>

#include "sock.h"

/******************************************************************************/
/* Prototypes                                                                 */
/******************************************************************************/

/**
 * @brief Sock thread used to handle reception of data
 * @param arg Sock listenner
 * @return Always returns NULL
 */
static void *sock_thread_listenner(void *arg);

/**
 * @brief Sock thread used to handle data received
 * @param arg Worker
 * @return Always returns NULL
 */
static void *sock_thread_messenger(void *arg);

/**
 * @brief Sock thread used to send data
 * @param arg Worker
 * @return Always returns NULL
 */
static void *sock_thread_sender(void *arg);

/**
 * @brief Start a new worker
 * @param sock Sock instance
 * @param list List of workers to which the new one should be added
 * @param worker Worker to start
 * @param start_routine Worker thread function
 * @return 0 if the function succeeded, -1 otherwise
 */
static int sock_start_worker(sock_t *sock, sock_worker_list_t *list, sock_worker_t *worker, void *(*start_routine)(void *));

/**
 * @brief Remove a worker
 * @param sock Sock instance
 * @param list List of workers to which the worker should be removed
 * @param worker Worker to remove
 * @return 0 if the function succeeded, -1 otherwise
 */
static int sock_remove_worker(sock_t *sock, sock_worker_list_t *list, sock_worker_t *worker);

/******************************************************************************/
/* Functions                                                                  */
/******************************************************************************/

/**
 * @brief Function used to create a sock instance
 * @return Sock instance if the function succeeded, NULL otherwise
 */
sock_t *
sock_create(void) {

    /* Create new sock instance */
    sock_t *sock = (sock_t *)malloc(sizeof(sock_t));
    if (NULL == sock) {
        /* Unable to allocate memory */
        return NULL;
    }
    memset(sock, 0, sizeof(sock_t));

    /* Initialize semaphore used to access listenners */
    sem_init(&sock->listenners.sem, 0, 1);

    /* Initialize semaphore used to access messengers */
    sem_init(&sock->messengers.sem, 0, 1);

    /* Initialize semaphore used to access senders */
    sem_init(&sock->senders.sem, 0, 1);

    /* Initialize clients FDs and semaphore */
    sem_init(&sock->clients.sem, 0, 1);
    FD_ZERO(&sock->clients.fds);

    return sock;
}

/**
 * @brief Bind a new socket to the wanted port, unicast configuration
 * @param sock Sock instance
 * @param address Address to bind to
 * @param port Port
 * @param reuse_addr Reuse address flag
 * @param unicast Unicast addresses, separated by a comma
 * @return 0 if the function succeeded, -1 otherwise
 */
int
sock_bind_unicast(sock_t *sock, char *address, uint16_t port, bool reuse_addr, char *unicast) {

    assert(NULL != sock);
    assert(NULL != address);
    assert(NULL != unicast);

    /* Create new listenner */
    sock_worker_t *worker = (sock_worker_t *)malloc(sizeof(sock_worker_t));
    if (NULL == worker) {
        /* Unable to allocate memory */
        return -1;
    }
    memset(worker, 0, sizeof(sock_worker_t));

    /* Store configuration, initialize FDs */
    if (NULL == (sock->options.address = strdup(address))) {
        /* Unable to allocate memory */
        free(worker);
        return -1;
    }
    sock->options.port       = port;
    sock->options.reuse_addr = reuse_addr;
    if (NULL == (sock->options.unicast = strdup(unicast))) {
        /* Unable to allocate memory */
        free(worker);
        return -1;
    }
    FD_ZERO(&worker->type.listenner.fds);

    /* Start listenner */
    if (0 != sock_start_worker(sock, &sock->listenners, worker, sock_thread_listenner)) {
        /* Unable to start the worker */
        free(worker);
        return -1;
    }

    return 0;
}

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
int
sock_bind_multicast(sock_t *sock, char *address, uint16_t port, bool reuse_addr, char *multicast, unsigned char multicast_ttl) {

    assert(NULL != sock);
    assert(NULL != address);
    assert(NULL != multicast);

    /* Create new listenner */
    sock_worker_t *worker = (sock_worker_t *)malloc(sizeof(sock_worker_t));
    if (NULL == worker) {
        /* Unable to allocate memory */
        return -1;
    }
    memset(worker, 0, sizeof(sock_worker_t));

    /* Store configuration, initialize FDs */
    if (NULL == (sock->options.address = strdup(address))) {
        /* Unable to allocate memory */
        free(worker);
        return -1;
    }
    sock->options.port       = port;
    sock->options.reuse_addr = reuse_addr;
    if (NULL == (sock->options.multicast = strdup(multicast))) {
        /* Unable to allocate memory */
        free(worker);
        return -1;
    }
    sock->options.multicast_ttl = multicast_ttl;
    FD_ZERO(&worker->type.listenner.fds);

    /* Start listenner */
    if (0 != sock_start_worker(sock, &sock->listenners, worker, sock_thread_listenner)) {
        /* Unable to start the worker */
        free(worker);
        return -1;
    }

    return 0;
}

/**
 * @brief Bind a new socket to the wanted port, unicast configuration
 * @param sock Sock instance
 * @param address Address to bind to
 * @param port Port
 * @param reuse_addr Reuse address flag
 * @param broadcast Broadcast address
 * @return 0 if the function succeeded, -1 otherwise
 */
int
sock_bind_broadcast(sock_t *sock, char *address, uint16_t port, bool reuse_addr, char *broadcast) {

    assert(NULL != sock);
    assert(NULL != address);
    assert(NULL != broadcast);

    /* Create new listenner */
    sock_worker_t *worker = (sock_worker_t *)malloc(sizeof(sock_worker_t));
    if (NULL == worker) {
        /* Unable to allocate memory */
        return -1;
    }
    memset(worker, 0, sizeof(sock_worker_t));

    /* Store configuration, initialize FDs */
    if (NULL == (sock->options.address = strdup(address))) {
        /* Unable to allocate memory */
        free(worker);
        return -1;
    }
    sock->options.port       = port;
    sock->options.reuse_addr = reuse_addr;
    if (NULL == (sock->options.broadcast = strdup(broadcast))) {
        /* Unable to allocate memory */
        free(worker);
        return -1;
    }
    FD_ZERO(&worker->type.listenner.fds);

    /* Start listenner */
    if (0 != sock_start_worker(sock, &sock->listenners, worker, sock_thread_listenner)) {
        /* Unable to start the worker */
        free(worker);
        return -1;
    }

    return 0;
}

/**
 * @brief Register callbacks
 * @param sock Sock instance
 * @param topic Topic
 * @param fct Callback function
 * @param user User data
 * @return 0 if the function succeeded, -1 otherwise
 */
int
sock_on(sock_t *sock, char *topic, void *fct, void *user) {

    assert(NULL != sock);
    assert(NULL != topic);

    /* Record callback depending of the topic */
    if (!strcmp(topic, "message")) {
        sock->cb.message.fct  = fct;
        sock->cb.message.user = user;
    } else if (!strcmp(topic, "error")) {
        sock->cb.error.fct  = fct;
        sock->cb.error.user = user;
    }

    return 0;
}

/**
 * @brief Function used to send data
 * @param sock Sock instance
 * @param buffer Buffer to be sent
 * @param size Size of buffer to send
 * @return 0 if the function succeeded, -1 otherwise
 */
int
sock_send(sock_t *sock, void *buffer, size_t size) {

    assert(NULL != sock);
    assert(NULL != buffer);

    /* Create new sender */
    sock_worker_t *worker = (sock_worker_t *)malloc(sizeof(sock_worker_t));
    if (NULL == worker) {
        /* Unable to allocate memory */
        return -1;
    }
    memset(worker, 0, sizeof(sock_worker_t));

    /* Store buffer and size */
    worker->type.sender.buffer = buffer;
    worker->type.sender.size   = size;

    /* Start sender */
    if (0 != sock_start_worker(sock, &sock->senders, worker, sock_thread_sender)) {
        /* Unable to start the worker */
        free(worker);
        return -1;
    }

    return 0;
}

/**
 * @brief Release sock instance
 * @param sock Sock instance
 */
void
sock_release(sock_t *sock) {

    /* Release sock instance */
    if (NULL != sock) {

        /* Release listenners */
        sem_wait(&sock->listenners.sem);
        sock_worker_t *worker = sock->listenners.first;
        while (NULL != worker) {
            sock_worker_t *tmp = worker;
            worker             = worker->next;
            pthread_cancel(tmp->thread);
            pthread_join(tmp->thread, NULL);
            for (int index = 0; index < FD_SETSIZE; index++) {
                if (FD_ISSET(index, &tmp->type.listenner.fds)) {
                    close(index);
                }
            }
            free(tmp);
        }
        sem_post(&sock->listenners.sem);
        sem_close(&sock->listenners.sem);

        /* Release messengers */
        sem_wait(&sock->messengers.sem);
        worker = sock->messengers.first;
        while (NULL != worker) {
            sock_worker_t *tmp = worker;
            worker             = worker->next;
            pthread_cancel(tmp->thread);
            pthread_join(tmp->thread, NULL);
            free(tmp->type.messenger.buffer);
            free(tmp);
        }
        sem_post(&sock->messengers.sem);
        sem_close(&sock->messengers.sem);

        /* Release senders */
        sem_wait(&sock->senders.sem);
        worker = sock->senders.first;
        while (NULL != worker) {
            sock_worker_t *tmp = worker;
            worker             = worker->next;
            pthread_cancel(tmp->thread);
            pthread_join(tmp->thread, NULL);
            free(tmp->type.sender.buffer);
            free(tmp);
        }
        sem_post(&sock->senders.sem);
        sem_close(&sock->senders.sem);

        /* Release clients semaphore */
        sem_close(&sock->clients.sem);

        /* Release options */
        if (NULL != sock->options.address) {
            free(sock->options.address);
        }
        if (NULL != sock->options.broadcast) {
            free(sock->options.broadcast);
        }
        if (NULL != sock->options.multicast) {
            free(sock->options.multicast);
        }
        if (NULL != sock->options.unicast) {
            free(sock->options.unicast);
        }

        /* Release sock instance */
        free(sock);
    }
}

/**
 * @brief Sock thread used to handle reception of data
 * @param arg Worker
 * @return Always returns NULL
 */
static void *
sock_thread_listenner(void *arg) {

    assert(NULL != arg);

    /* Retrieve worker */
    sock_worker_t *worker = (sock_worker_t *)arg;
    sock_t *       sock   = worker->parent;

    /* Create new SOCK_DGRAM socket */
    worker->type.listenner.socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (0 > worker->type.listenner.socket) {
        /* Unable to create socket */
        if (NULL != sock->cb.error.fct) {
            sock->cb.error.fct(sock, "sock: unable to create listenner socket", sock->cb.error.user);
        }
        goto END;
    }

    /* Add myself to the FDs */
    FD_SET(worker->type.listenner.socket, &worker->type.listenner.fds);
    sem_wait(&sock->clients.sem);
    FD_SET(worker->type.listenner.socket, &sock->clients.fds);
    sem_post(&sock->clients.sem);

    /* Set socket options */
    if (NULL != sock->options.broadcast) {
        int opt = 1;
        if (0 > setsockopt(worker->type.listenner.socket, SOL_SOCKET, SO_BROADCAST, (void *)&opt, sizeof(opt))) {
            /* Unable to set socket option */
            close(worker->type.listenner.socket);
            if (NULL != sock->cb.error.fct) {
                sock->cb.error.fct(sock, "sock: unable to set socket option SO_BROADCAST", sock->cb.error.user);
            }
            goto END;
        }
    }
    if (true == sock->options.reuse_addr) {
        int opt = 1;
        if (0 > setsockopt(worker->type.listenner.socket, SOL_SOCKET, SO_REUSEADDR, (void *)&opt, sizeof(opt))) {
            /* Unable to set socket option */
            close(worker->type.listenner.socket);
            if (NULL != sock->cb.error.fct) {
                sock->cb.error.fct(sock, "sock: unable to set socket option SO_REUSEADDR", sock->cb.error.user);
            }
            goto END;
        }
    }

    /* Bind socket */
    struct sockaddr_in addr_local;
    addr_local.sin_family      = AF_INET;
    addr_local.sin_addr.s_addr = inet_addr(sock->options.address);
    addr_local.sin_port        = htons(sock->options.port);
    if (0 > bind(worker->type.listenner.socket, (struct sockaddr *)&addr_local, sizeof(addr_local))) {
        /* Unable to bind socket */
        close(worker->type.listenner.socket);
        if (NULL != sock->cb.error.fct) {
            sock->cb.error.fct(sock, "sock: unable to bind socket", sock->cb.error.user);
        }
        goto END;
    }

    /* Set more socket options */
    if (NULL != sock->options.multicast) {
        struct ip_mreq mreq;
        mreq.imr_multiaddr.s_addr = inet_addr(sock->options.multicast);
        mreq.imr_interface.s_addr = htonl(INADDR_ANY);
        if (0 > setsockopt(worker->type.listenner.socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, (void *)&mreq, sizeof(mreq))) {
            /* Unable to set socket option */
            close(worker->type.listenner.socket);
            if (NULL != sock->cb.error.fct) {
                sock->cb.error.fct(sock, "sock: unable to set socket option IP_ADD_MEMBERSHIP", sock->cb.error.user);
            }
            goto END;
        }
        if (0 > setsockopt(worker->type.listenner.socket, IPPROTO_IP, IP_MULTICAST_TTL, (void *)&sock->options.multicast_ttl, sizeof(unsigned char))) {
            /* Unable to set socket option */
            close(worker->type.listenner.socket);
            if (NULL != sock->cb.error.fct) {
                sock->cb.error.fct(sock, "sock: unable to set socket option IP_MULTICAST_TTL", sock->cb.error.user);
            }
            goto END;
        }
    }

    /* Infinite loop */
    while (1) {

        /* Block until input arrives on one or more active sockets */
        fd_set         fds = worker->type.listenner.fds;
        struct timeval tv  = { 5, 0 };
        if (0 > select(FD_SETSIZE, &fds, NULL, NULL, &tv)) {
            /* Unable to select */
        }

        /* Handling of all the sockets with input pending */
        for (int index = 0; index < FD_SETSIZE; index++) {
            if (FD_ISSET(index, &fds)) {
                /* Data arriving on an already-connected socket */
                size_t size = 0;
                ioctl(index, FIONREAD, &size);
                if (0 < size) {
                    /* Create new messenger */
                    sock_worker_t *w = (sock_worker_t *)malloc(sizeof(sock_worker_t));
                    if (NULL != w) {
                        memset(w, 0, sizeof(sock_worker_t));
                        /* Store socket and size and create buffer */
                        w->type.messenger.size   = size;
                        w->type.messenger.buffer = malloc(size);
                        if (NULL != w->type.messenger.buffer) {
                            /* Read from socket */
                            struct sockaddr_in addr_remote;
                            socklen_t          size_remote = sizeof(addr_remote);
                            if (size == recvfrom(index, w->type.messenger.buffer, size, 0, (struct sockaddr *)&addr_remote, &size_remote)) {
                                /* Retrieve IP address and port of the sender */
                                inet_ntop(AF_INET, &addr_remote.sin_addr, w->type.messenger.ip, sizeof(w->type.messenger.ip));
                                w->type.messenger.port = ntohs(addr_remote.sin_port);
                                /* Start messenger */
                                if (0 != sock_start_worker(sock, &sock->messengers, w, sock_thread_messenger)) {
                                    /* Unable to start the worker */
                                    free(w->type.messenger.buffer);
                                    free(w);
                                }
                            }
                        } else {
                            /* Unable to allocate memory */
                            free(w);
                        }
                    }
                }
            }
        }
    }

    /* Close my own socket */
    close(worker->type.listenner.socket);

END:

    /* Remove worker from listenners */
    sock_remove_worker(sock, &sock->listenners, worker);

    /* Release memory */
    free(worker);

    return NULL;
}

/**
 * @brief Sock thread used to handle data received
 * @param arg Worker
 * @return Always returns NULL
 */
static void *
sock_thread_messenger(void *arg) {

    assert(NULL != arg);

    /* Retrieve worker */
    sock_worker_t *worker = (sock_worker_t *)arg;
    sock_t *       sock   = worker->parent;

    /* Check if message callback is define */
    if (NULL != sock->cb.message.fct) {

        /* Invoke message callback */
        sock->cb.message.fct(
            sock, worker->type.messenger.ip, worker->type.messenger.port, worker->type.messenger.buffer, worker->type.messenger.size, sock->cb.message.user);
    }

    /* Remove worker from messengers */
    sock_remove_worker(sock, &sock->messengers, worker);

    /* Release memory */
    free(worker->type.messenger.buffer);
    free(worker);

    return NULL;
}

/**
 * @brief Sock thread used to send data
 * @param arg Worker
 * @return Always returns NULL
 */
static void *
sock_thread_sender(void *arg) {

    assert(NULL != arg);

    /* Retrieve worker */
    sock_worker_t *worker = (sock_worker_t *)arg;
    sock_t *       sock   = worker->parent;

    /* Wait semaphore */
    sem_wait(&sock->clients.sem);

    /* Send data to all clients sockets depending of the configuration */
    if (NULL != sock->options.unicast) {

        /* Unicast */
        char *unicast = strdup(sock->options.unicast);
        if (NULL != unicast) {
            char *saveptr = NULL;
            char *pch     = strtok_r(unicast, ",", &saveptr);
            while (NULL != pch) {
                struct sockaddr_in addr;
                addr.sin_family      = AF_INET;
                addr.sin_port        = htons(sock->options.port);
                addr.sin_addr.s_addr = inet_addr(pch);
                for (int index = 0; index < FD_SETSIZE; index++) {
                    if (FD_ISSET(index, &sock->clients.fds)) {
                        if (worker->type.sender.size
                            != sendto(index, worker->type.sender.buffer, worker->type.sender.size, 0, (struct sockaddr *)&addr, sizeof(addr))) {
                            /* Unable to send data */
                        }
                    }
                }
                pch = strtok_r(NULL, ",", &saveptr);
            }
            free(unicast);
        }

    } else if (NULL != sock->options.multicast) {

        /* Multicast */
        struct sockaddr_in addr;
        addr.sin_family      = AF_INET;
        addr.sin_port        = htons(sock->options.port);
        addr.sin_addr.s_addr = inet_addr(sock->options.multicast);
        for (int index = 0; index < FD_SETSIZE; index++) {
            if (FD_ISSET(index, &sock->clients.fds)) {
                if (worker->type.sender.size
                    != sendto(index, worker->type.sender.buffer, worker->type.sender.size, 0, (struct sockaddr *)&addr, sizeof(addr))) {
                    /* Unable to send data */
                }
            }
        }

    } else if (NULL != sock->options.broadcast) {

        /* Broadcast */
        struct sockaddr_in addr;
        addr.sin_family      = AF_INET;
        addr.sin_port        = htons(sock->options.port);
        addr.sin_addr.s_addr = inet_addr(sock->options.broadcast);
        for (int index = 0; index < FD_SETSIZE; index++) {
            if (FD_ISSET(index, &sock->clients.fds)) {
                if (worker->type.sender.size
                    != sendto(index, worker->type.sender.buffer, worker->type.sender.size, 0, (struct sockaddr *)&addr, sizeof(addr))) {
                    /* Unable to send data */
                }
            }
        }
    }

    /* Release semaphore */
    sem_post(&sock->clients.sem);

    /* Remove worker from senders */
    sock_remove_worker(sock, &sock->senders, worker);

    /* Release memory */
    free(worker->type.sender.buffer);
    free(worker);

    return NULL;
}

/**
 * @brief Start a new worker
 * @param sock Sock instance
 * @param list List of workers to which the worker should be added
 * @param worker Worker to start
 * @param start_routine Worker thread function
 * @return 0 if the function succeeded, -1 otherwise
 */
static int
sock_start_worker(sock_t *sock, sock_worker_list_t *list, sock_worker_t *worker, void *(*start_routine)(void *)) {

    /* Wait semaphore */
    sem_wait(&list->sem);

    /* Store sock parent instance */
    worker->parent = sock;

    /* Initialize attributes of the thread */
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    /* Start thread */
    if (0 != pthread_create(&worker->thread, &attr, start_routine, (void *)worker)) {
        /* Unable to start the thread */
        sem_post(&list->sem);
        return -1;
    }

    /* Add worker to the daisy chain */
    if (NULL == list->last) {
        list->first = list->last = worker;
    } else {
        worker->prev     = list->last;
        list->last->next = worker;
        list->last       = worker;
    }

    /* Release semaphore */
    sem_post(&list->sem);

    return 0;
}

/**
 * @brief Remove a worker
 * @param sock Sock instance
 * @param list List of workers to which the worker should be removed
 * @param worker Worker to remove
 * @return 0 if the function succeeded, -1 otherwise
 */
static int
sock_remove_worker(sock_t *sock, sock_worker_list_t *list, sock_worker_t *worker) {

    (void)sock;

    /* Wait semaphore */
    sem_wait(&list->sem);

    /* Remove the worker from the daisy chain */
    if (NULL != worker->prev) {
        worker->prev->next = worker->next;
    } else {
        list->first = worker->next;
    }
    if (NULL != worker->next) {
        worker->next->prev = worker->prev;
    } else {
        list->last = worker->prev;
    }

    /* Release semaphore */
    sem_post(&list->sem);

    return 0;
}
