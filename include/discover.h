/**
 * @file      discover.h
 * @brief     Discovery of other instances
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

#ifndef __DISCOVER_H__
#define __DISCOVER_H__

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(__WINDOWS__) && (defined(WIN32) || defined(WIN64) || defined(_MSC_VER) || defined(_WIN32))
#define __WINDOWS__
#endif

#ifdef __WINDOWS__

/* When compiling for windows, we specify a specific calling convention to avoid issues where we are being called from a project with a different default calling convention.  For windows you have 3 define options:

DISCOVER_HIDE_SYMBOLS - Define this in the case where you don't want to ever dllexport symbols
DISCOVER_EXPORT_SYMBOLS - Define this on library build when you want to dllexport symbols (default)
DISCOVER_IMPORT_SYMBOLS - Define this if you want to dllimport symbol

For *nix builds that support visibility attribute, you can define similar behavior by

setting default visibility to hidden by adding
-fvisibility=hidden (for gcc)
or
-xldscope=hidden (for sun cc)
to CFLAGS

then using the DISCOVER_API_VISIBILITY flag to "export" the same symbols the way DISCOVER_EXPORT_SYMBOLS does

*/

#define DISCOVER_CDECL __cdecl
#define DISCOVER_STDCALL

/* export symbols by default, this is necessary for copy pasting the C and header file */
#if !defined(DISCOVER_HIDE_SYMBOLS) && !defined(DISCOVER_IMPORT_SYMBOLS) && !defined(DISCOVER_EXPORT_SYMBOLS)
#define DISCOVER_EXPORT_SYMBOLS
#endif

#if defined(DISCOVER_HIDE_SYMBOLS)
#define DISCOVER_PUBLIC(type) type DISCOVER_STDCALL
#elif defined(DISCOVER_EXPORT_SYMBOLS)
#define DISCOVER_PUBLIC(type) __declspec(dllexport) type DISCOVER_STDCALL
#elif defined(DISCOVER_IMPORT_SYMBOLS)
#define DISCOVER_PUBLIC(type) __declspec(dllimport) type DISCOVER_STDCALL
#endif
#else /* !__WINDOWS__ */
#define DISCOVER_CDECL
#define DISCOVER_STDCALL

#if (defined(__GNUC__) || defined(__SUNPRO_CC) || defined(__SUNPRO_C)) && defined(DISCOVER_API_VISIBILITY)
#define DISCOVER_PUBLIC(type) __attribute__((visibility("default"))) type
#else
#define DISCOVER_PUBLIC(type) type
#endif
#endif

/******************************************************************************/
/* Includes                                                                   */
/******************************************************************************/

#include <stdint.h>
#include <stdbool.h>
#ifdef __WINDOWS__
#include <windows.h>
#else
#include <semaphore.h>
#endif
#include <cJSON.h>

/******************************************************************************/
/* Definitions                                                                */
/******************************************************************************/

/* Discover nodes */
typedef struct discover_node_s {
    struct discover_node_s *prev;      /* Previous node */
    struct discover_node_s *next;      /* Next node */
    char                   *pid;       /* Process UUID of the node */
    char                   *iid;       /* Instance UUID of the node */
    char                   *hostname;  /* Hostname of the node */
    char                   *address;   /* Address of the node */
    uint16_t                port;      /* Port of the node */
    time_t                  last_seen; /* Last time the node has been seen */
    struct {
        bool   is_master;          /* true if the node is master, false otherwise */
        bool   is_master_eligible; /* true if the node is master eligible, false otherwise */
        double weight;             /* Weight of the node */
        char  *address;            /* Address on which the node bound */
        cJSON *advertisement;      /* Advestisement object */
    } data;
} discover_node_t;

/* Axon topic subscription */
struct discover_s;
typedef struct discover_channel_s {
    struct discover_channel_s *next;                            /* Next channel */
    char                      *event;                           /* Event of the channel */
    void *(*fct)(struct discover_s *, char *, cJSON *, void *); /* Callback function invoked when event is received */
    void *user;                                                 /* User data passed to the callback */
} discover_channel_t;

/* Discover instance */
typedef struct sock_s sock_t;
typedef struct discover_s {
    struct {
        int           hello_interval; /* How often to broadcast a hello packet in milliseconds */
        int           check_interval; /* How often to to check for missing nodes in milliseconds */
        int           node_timeout;   /* Consider a node dead if not seen in this many milliseconds */
        int           master_timeout; /* Consider a master node dead if not seen in this many milliseconds */
        char         *address;        /* Address to bind to */
        uint16_t      port;           /* Port on which to bind and communicate with other node-discover processes */
        char         *broadcast;      /* Broadcast address if using broadcast */
        char         *multicast;      /* Multicast address if using multicast - If net set, broadcast or unicast is used */
        unsigned char multicast_ttl;  /* Multicast TTL for when using multicast */
        char *
            unicast; /* Comma separated string of Unicast addresses of known nodes - It is advised to specify the address of the local interface when using unicast and expecting local discovery to work*/
        char  *key;  /* Encryption key if your broadcast packets should be encrypted */
        int    masters_required; /* The count of master processes that should always be available */
        double weight;           /* A number used to determine the preference for a specific process to become master - Higher numbers win */
        bool   client;           /* When true operate in client only mode (don't broadcast existence of node, just listen and discover) */
        bool   reuse_addr;       /* Allow multiple processes on the same host to bind to the same address and port */
        bool   ignore_process;  /* If set to false, will not ignore messages from other Discover instances within the same process (on non-reserved channels) */
        bool   ignore_instance; /* If set to false, will not ignore messages from self (on non-reserved channels) */
        cJSON *advertisement;   /* The initial advertisement which is sent with each hello packet */
        char  *hostname;        /* Override the OS hostname with a custom value */
#ifdef __WINDOWS__
        HANDLE sem; /* Semaphore used to protect options */
#else
        sem_t sem; /* Semaphore used to protect options */
#endif
    } options;
    sock_t   *sock;               /* Sock instance */
    pthread_t thread_check;       /* Check thread handle */
    pthread_t thread_hello;       /* Hello thread handle */
    char     *pid;                /* Process UUID */
    char     *iid;                /* Instance UUID */
    bool      is_master;          /* true if master, false otherwise */
    bool      is_master_eligible; /* true if master eligible, false otherwise */
    struct {
        discover_node_t *first; /* First node of the daisy chain */
        discover_node_t *last;  /* Last node of the daisy chain */
#ifdef __WINDOWS__
        HANDLE sem; /* Semaphore used to protect daisy chain */
#else
        sem_t sem; /* Semaphore used to protect daisy chain */
#endif
    } nodes;
    struct {
        discover_channel_t *first; /* Event channel daisy chain */
#ifdef __WINDOWS__
        HANDLE sem; /* Semaphore used to protect daisy chain */
#else
        sem_t sem; /* Semaphore used to protect daisy chain */
#endif
    } channels;
    struct {
        struct {
            void *(*fct)(struct discover_s *, discover_node_t *, void *); /* Callback function invoked when hello message is received */
            void *user;                                                   /* User data passed to the callback */
        } hello_received;
        struct {
            void *(*fct)(struct discover_s *, void *); /* Callback function invoked when hello message is emitted */
            void *user;                                /* User data passed to the callback */
        } hello_emitted;
        struct {
            void *(*fct)(struct discover_s *, void *); /* Callback function invoked when I promote myself a master */
            void *user;                                /* User data passed to the callback */
        } promotion;
        struct {
            void *(*fct)(struct discover_s *, void *); /* Callback function invoked when I demote myself */
            void *user;                                /* User data passed to the callback */
        } demotion;
        struct {
            void *(*fct)(struct discover_s *, void *); /* Callback function invoked when check has been executed */
            void *user;                                /* User data passed to the callback */
        } check;
        struct {
            void *(*fct)(struct discover_s *, discover_node_t *, void *); /* Callback function invoked when a node is added */
            void *user;                                                   /* User data passed to the callback */
        } added;
        struct {
            void *(*fct)(struct discover_s *, discover_node_t *, void *); /* Callback function invoked when a new master is promoted */
            void *user;                                                   /* User data passed to the callback */
        } master;
        struct {
            void *(*fct)(struct discover_s *, discover_node_t *, void *); /* Callback function invoked when a node is removed */
            void *user;                                                   /* User data passed to the callback */
        } removed;
        struct {
            void *(*fct)(struct discover_s *, char *, void *); /* Callback function invoked when an error occurs */
            void *user;                                        /* User data passed to the callback */
        } error;
    } cb;
} discover_t;

/******************************************************************************/
/* Prototypes                                                                 */
/******************************************************************************/

/**
 * @brief Function used to create discover instance
 * @return Discover instance if the function succeeded, NULL otherwise
 */
DISCOVER_PUBLIC(discover_t *) discover_create(void);

/**
 * @brief Set discover options
 * @param discover Discover instance
 * @param option Option by name
 * @param value New value of the option
 * @return 0 if the function succeeded, -1 otherwise
 */
DISCOVER_PUBLIC(int) discover_set_option(discover_t *discover, char *option, void *value);

/**
 * @brief Start discovering
 * @param discover Discover instance
 * @return 0 if the function succeeded, -1 otherwise
 */
DISCOVER_PUBLIC(int) discover_start(discover_t *discover);

/**
 * @brief Register callbacks
 * @param discover Discover instance
 * @param topic Topic
 * @param fct Callback funtion
 * @param user User data
 * @return 0 if the function succeeded, -1 otherwise
 */
DISCOVER_PUBLIC(int) discover_on(discover_t *discover, char *topic, void *fct, void *user);

/**
 * @brief Function used to set advertisement
 * @param discover Discover instance
 * @param advertisement Advestisement object
 * @return 0 if the function succeeded, -1 otherwise
 */
DISCOVER_PUBLIC(int) discover_advertise(discover_t *discover, cJSON *advertisement);

/**
 * @brief Promote the instance to master
 * @param discover Discover instance
 * @return 0 if the function succeeded, -1 otherwise
 */
DISCOVER_PUBLIC(int) discover_promote(discover_t *discover);

/**
 * @brief Demote the instance from being a master
 * @param discover Discover instance
 * @param permanent true to specify that this should not automatically become master again, false otherwise
 * @return 0 if the function succeeded, -1 otherwise
 */
DISCOVER_PUBLIC(int) discover_demote(discover_t *discover, bool permanent);

/**
 * @brief Subscribe to wanted event
 * @param discover Discover instance
 * @param event Event
 * @param fct Callback funtion
 * @param user User data
 * @return 0 if the function succeeded, -1 otherwise
 */
DISCOVER_PUBLIC(int) discover_join(discover_t *discover, char *event, void *fct, void *user);

/**
 * @brief Leave channel
 * @param discover Discover instance
 * @param event Event
 * @return 0 if the function succeeded, -1 otherwise
 */
DISCOVER_PUBLIC(int) discover_leave(discover_t *discover, char *event);

/**
 * @brief Function used to send event data
 * @param discover Discover instance
 * @param event Event
 * @param data Data to send
 * @return 0 if the function succeeded, -1 otherwise
 */
DISCOVER_PUBLIC(int) discover_send(discover_t *discover, char *event, cJSON *data);

/**
 * @brief Release discover instance
 * @param discover Discover instance
 */
DISCOVER_PUBLIC(void) discover_release(discover_t *discover);

#ifdef __cplusplus
}
#endif

#endif /* __DISCOVER_H__ */
