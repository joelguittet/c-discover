/**
 * @file      basic-advertise.c
 * @brief     Discover basic-advertise example in C
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
#if !defined(WIN32) && !defined(WIN64) && !defined(_MSC_VER) && !defined(_WIN32)
#include <unistd.h>
#endif
#include <assert.h>
#include <signal.h>

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

    discover_t *discover;

    /* Initialize sig handler */
    signal(SIGINT, sig_handler);

    /* Create discover instance */
    if (NULL == (discover = discover_create())) {
        printf("unable to create discover instance\n");
        exit(EXIT_FAILURE);
    }

    /* Definition of added/removed/error callbacks */
    discover_on(discover, "added", &callback_added, NULL);
    discover_on(discover, "removed", &callback_removed, NULL);
    discover_on(discover, "error", &callback_error, NULL);

    /* Definition of advertisement */
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "testing", "hello world!");
    discover_advertise(discover, json);
    cJSON_Delete(json);

    /* Start instance */
    if (0 != discover_start(discover)) {
        printf("unable to start discover instance\n");
        exit(EXIT_FAILURE);
    }

    printf("basic discover started\n");

    /* Wait before terminating the program */
    while (false == terminate) {
        sleep(1);
    }

    /* Release memory */
    discover_release(discover);

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
    (void)user;

    printf("New node added to the network:\n");
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
    (void)user;

    printf("Node removed from the network:\n");
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
    (void)user;

    printf("An error occured:\n");
    printf("%s\n", err);
    printf("\n");
}
