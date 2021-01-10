/**
 * @file test-multicast.c
 * @brief Discover test-multicast example in C
 */


/******************************************************************************/
/* Includes                                                                   */
/******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <signal.h>

#include "discover.h"


/******************************************************************************/
/* Variables                                                                  */
/******************************************************************************/

static bool terminate = false;                                      /* Flag used to terminate the application */


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
int main(int argc, char** argv) {

  discover_t *discover;
  
  /* Initialize sig handler */
  signal(SIGINT, sig_handler);
  
  /* Create discover instance */
  if (NULL == (discover = discover_create())) {
    printf("unable to create discover instance\n");
    exit(EXIT_FAILURE);
  }
  
  /* Set options */
  char *multicast = "224.0.2.1";
  discover_set_option(discover, "multicast", (void *)multicast);
  unsigned char multicast_ttl = 1;
  discover_set_option(discover, "multicastTTL", (void *)&multicast_ttl);
  
  /* Definition of added/removed/error callbacks */
  discover_on(discover, "added", &callback_added, NULL);
  discover_on(discover, "removed", &callback_removed, NULL);
  discover_on(discover, "error", &callback_error, NULL);

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
static void sig_handler(int signo) {

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
static void callback_added(discover_t *discover, discover_node_t *node, void *user) {

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
static void callback_removed(discover_t *discover, discover_node_t *node, void *user) {

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
static void callback_error(discover_t *discover, char *err, void *user) {

  (void)discover;
  assert(NULL != err);
  (void)user;

  printf("An error occured:\n");
  printf("%s\n", err);
  printf("\n");
}
