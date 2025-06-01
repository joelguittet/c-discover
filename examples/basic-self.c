/**
 * @file      basic-self.c
 * @brief     Discover basic-self example in C
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

/******************************************************************************/
/* Includes                                                                   */
/******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <signal.h>
#include <pthread.h>

#include "discover.h"

/******************************************************************************/
/* Variables                                                                  */
/******************************************************************************/

static bool terminate = false; /* Flag used to terminate the application */

/******************************************************************************/
/* Prototypes                                                                 */
/******************************************************************************/

/**
 * @brief Signal hanlder
 * @param signo Signal number
 */
static void sig_handler(int signo);

/**
 * @brief Callback function invoked when instance is added
 * @param discover Discover instance
 * @param node Node
 * @param user User data
 */
static void callback_added(discover_t *discover, discover_node_t *node, void *user);

/**
 * @brief Callback function invoked when instance is removed
 * @param discover Discover instance
 * @param node Node
 * @param user User data
 */
static void callback_removed(discover_t *discover, discover_node_t *node, void *user);

/**
 * @brief Callback function invoked when an error occurs
 * @param discover Discover instance
 * @param err Error as string
 * @param user User data
 */
static void callback_error(discover_t *discover, char *err, void *user);

/**
 * @brief Callback function invoked when test message is received
 * @param discover Discover instance
 * @param event Event received
 * @param json JSON object received
 * @param user User data
 */
static void callback_test(discover_t *discover, char *event, cJSON *json, void *user);

/**
 * @brief Thread used to periodically send the test message
 * @param arg Discover instance
 * @return Always returns NULL
 */
static void *thread_test(void *arg);

/******************************************************************************/
/* Functions                                                                  */
/******************************************************************************/

/**
 * @brief Main function
 * @param argc Number of arguments
 * @param argv Arguments
 * @return Always returns 0
 */
int
main(int argc, char **argv) {

    discover_t *discover1;
    discover_t *discover2;

    /* Initialize sig handler */
    signal(SIGINT, sig_handler);

    /* Create discover instances */
    if (NULL == (discover1 = discover_create())) {
        printf("unable to create discover instance\n");
        exit(EXIT_FAILURE);
    }
    if (NULL == (discover2 = discover_create())) {
        printf("unable to create discover instance\n");
        exit(EXIT_FAILURE);
    }

    /* Set options */
    double weight1 = 11111;
    discover_set_option(discover1, "weight", (void *)&weight1);
    double weight2 = 22222;
    discover_set_option(discover2, "weight", (void *)&weight2);

    /* Definition of added/removed/error callbacks */
    discover_on(discover1, "added", &callback_added, "d1");
    discover_on(discover2, "added", &callback_added, "d2");
    discover_on(discover1, "removed", &callback_removed, "d1");
    discover_on(discover2, "removed", &callback_removed, "d2");
    discover_on(discover1, "error", &callback_error, "d1");
    discover_on(discover2, "error", &callback_error, "d2");

    /* Join "test" event */
    discover_join(discover1, "test", &callback_test, "d1");
    discover_join(discover2, "test", &callback_test, "d2");

    /* Start instances */
    if (0 != discover_start(discover1)) {
        printf("unable to start discover instance\n");
        exit(EXIT_FAILURE);
    }
    if (0 != discover_start(discover2)) {
        printf("unable to start discover instance\n");
        exit(EXIT_FAILURE);
    }

    printf("basic discover started\n");

    /* Start the threads used to send test event */
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_t thread1;
    pthread_create(&thread1, &attr, thread_test, (void *)discover1);
    pthread_t thread2;
    pthread_create(&thread2, &attr, thread_test, (void *)discover2);

    /* Wait before terminating the program */
    while (false == terminate) {
        sleep(1);
    }

    /* Stop threads */
    pthread_cancel(thread1);
    pthread_join(thread1, NULL);
    pthread_cancel(thread2);
    pthread_join(thread2, NULL);

    /* Release memory */
    discover_release(discover1);
    discover_release(discover2);

    return 0;
}

/**
 * @brief Signal hanlder
 * @param signo Signal number
 */
static void
sig_handler(int signo) {

    /* SIGINT handling */
    if (SIGINT == signo) {
        terminate = true;
    }
}

/**
 * @brief Callback function invoked when instance is added
 * @param discover Discover instance
 * @param node Node
 * @param user User data
 */
static void
callback_added(discover_t *discover, discover_node_t *node, void *user) {

    (void)discover;
    assert(NULL != node);
    assert(NULL != user);

    printf("%s: New node added to the network:\n", (char *)user);
    printf("  isMaster=%s\n", (true == node->data.is_master) ? "true" : "false");
    printf("  isMasterEligible=%s\n", (true == node->data.is_master_eligible) ? "true" : "false");
    printf("  weight=%0.10f\n", node->data.weight);
    printf("  address='%s'\n", node->address);
    printf("  lastSeen=%d\n", (int)node->last_seen);
    printf("  hostName='%s'\n", node->hostname);
    printf("  port=%d\n", node->port);
    printf("  iid='%s'\n", node->iid);
    printf("  pid='%s'\n", node->pid);
    printf("\n");
}

/**
 * @brief Callback function invoked when instance is removed
 * @param discover Discover instance
 * @param node Node
 * @param user User data
 */
static void
callback_removed(discover_t *discover, discover_node_t *node, void *user) {

    (void)discover;
    assert(NULL != node);
    assert(NULL != user);

    printf("%s: Node removed from the network:\n", (char *)user);
    printf("  isMaster=%s\n", (true == node->data.is_master) ? "true" : "false");
    printf("  isMasterEligible=%s\n", (true == node->data.is_master_eligible) ? "true" : "false");
    printf("  weight=%0.10f\n", node->data.weight);
    printf("  address='%s'\n", node->address);
    printf("  lastSeen=%d\n", (int)node->last_seen);
    printf("  hostName='%s'\n", node->hostname);
    printf("  port=%d\n", node->port);
    printf("  iid='%s'\n", node->iid);
    printf("  pid='%s'\n", node->pid);
    printf("\n");
}

/**
 * @brief Callback function invoked when an error occurs
 * @param discover Discover instance
 * @param err Error as string
 * @param user User data
 */
static void
callback_error(discover_t *discover, char *err, void *user) {

    (void)discover;
    assert(NULL != err);
    assert(NULL != user);

    printf("%s: An error occured:\n", (char *)user);
    printf("%s\n", err);
    printf("\n");
}

/**
 * @brief Callback function invoked when test message is received
 * @param discover Discover instance
 * @param event Event received
 * @param json JSON object received
 * @param user User data
 */
static void
callback_test(discover_t *discover, char *event, cJSON *json, void *user) {

    (void)discover;
    (void)event;
    assert(NULL != json);
    assert(NULL != user);

    printf("%s: Message:\n", (char *)user);
    char *str = cJSON_Print(json);
    if (NULL != str) {
        printf("%s\n", str);
        free(str);
    }
    printf("\n");
}

/**
 * @brief Thread used to periodically send the test message
 * @param arg Discover instance
 * @return Always returns NULL
 */
static void *
thread_test(void *arg) {

    /* Retrieve discover instance */
    discover_t *discover = (discover_t *)arg;

    /* Infinite loop */
    while (1) {

        /* Send message */
        cJSON *json = cJSON_CreateString("hello from the other instance");
        if (NULL != json) {
            discover_send(discover, "test", json);
            cJSON_Delete(json);
        }

        /* Wait before the next loop */
        sleep(1);
    }

    return NULL;
}
