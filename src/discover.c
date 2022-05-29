/**
 * @file discover.c
 * @brief Discovery of other instances
 */

/******************************************************************************/
/* Includes                                                                   */
/******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <pthread.h>
#include <regex.h>
#include <uuid4.h>
#include <cJSON.h>
#include <math.h>

#include "discover.h"
#include "sock.h"


/******************************************************************************/
/* Prototypes                                                                 */
/******************************************************************************/

/**
 * @brief Start hello thread
 * @param discover Discover instance
 * @return 0 if the function succeeded, -1 otherwise
 */
static int discover_start_hello(discover_t *discover);

/**
 * @brief Thread used to periodically send the hello message
 * @param arg Discover instance
 * @return Always returns NULL
 */
static void *discover_thread_hello(void *arg);

/**
 * @brief Start check thread
 * @param discover Discover instance
 * @return 0 if the function succeeded, -1 otherwise
 */
static int discover_start_check(discover_t *discover);

/**
 * @brief Thread used to periodically refresh the presence of other nodes
 * @param arg Discover instance
 * @return Always returns NULL
 */
static void *discover_thread_check(void *arg);

/**
 * @brief Callback function called to handle received data
 * @param sock Sock instance
 * @param ip IP address of the sender
 * @param port Port of the sender
 * @param buffer Data received
 * @param size Size of data received
 * @param user User data
 */
static void discover_message_cb(const sock_t *sock, const char *ip, uint16_t port, const void *buffer, size_t size, void *user);

/**
 * @brief Callback function called to handle error from sock instance
 * @param sock Sock instance
 * @param err Error as string
 * @param user User data
 */
static void discover_error_cb(const sock_t *sock, char *err, void *user);

/**
 * @brief Generate UUID V4
 * @param buf Buffer to store UUID
 * @return 0 if the function succeeded, -1 otherwise
 */
static int discover_generate_uuid(char **buf);


/******************************************************************************/
/* Functions                                                                  */
/******************************************************************************/

/**
 * @brief Function used to create discover instance
 * @return Discover instance if the function succeeded, NULL otherwise
 */
discover_t *discover_create(void) {

  /* Create discover instance */
  discover_t *discover = (discover_t *)malloc(sizeof(discover_t));
  if (NULL == discover) {
    /* Unable to allocate memory */
    return NULL;
  }
  memset(discover, 0, sizeof(discover_t));
  
  /* Create sock instance */
  if (NULL == (discover->sock = sock_create())) {
    /* Unable to allocate memory */
    free(discover);
    return NULL;
  }

  /* Set default options */
  discover->options.hello_interval = 1000;
  discover->options.check_interval = 2000;
  discover->options.node_timeout = 2000;
  discover->options.master_timeout = 2000;
  discover->options.address = strdup("0.0.0.0");
  if (NULL == discover->options.address) {
    /* Unable to allocate memory */
    sock_release(discover->sock);
    free(discover);
    return NULL;
  }
  discover->options.port = 12345;
  discover->options.broadcast = strdup("255.255.255.255");
  if (NULL == discover->options.broadcast) {
    /* Unable to allocate memory */
    sock_release(discover->sock);
    free(discover->options.address);
    free(discover);
    return NULL;
  }
  discover->options.multicast = NULL;
  discover->options.multicast_ttl = 1;
  discover->options.unicast = NULL;
  discover->options.key = NULL;
  discover->options.masters_required = 1;
  discover->options.client = false;
  discover->options.reuse_addr = true;
  discover->options.ignore_process = true;
  discover->options.ignore_instance = true;
  discover->options.advertisement = NULL;
  
  /* Get hostname */
  if (NULL == (discover->options.hostname = (char *)malloc(128 + 1))) {
    /* Unable to allocate memory */
    sock_release(discover->sock);
    free(discover->options.address);
    free(discover->options.broadcast);
    free(discover);
    return NULL;
  }
  memset(discover->options.hostname, 0, 128);
  if (0 > gethostname(discover->options.hostname, 128)) {
    /* Unable to get hostname */
    sock_release(discover->sock);
    free(discover->options.address);
    free(discover->options.broadcast);
    free(discover->options.hostname);
    free(discover);
    return NULL;
  }
  
  /* Compute weight */
  discover->options.weight = (double)time(NULL);
  while (1 < discover->options.weight) {
    discover->options.weight /= 10;
  }
  discover->options.weight *= -1;
  
  /* Generate Process UUID */
  if (0 > discover_generate_uuid(&discover->pid)) {
    /* Unable to generate Process UUID */
    sock_release(discover->sock);
    free(discover->options.address);
    free(discover->options.broadcast);
    free(discover->options.hostname);
    free(discover);
    return NULL;
  }
  
  /* Generate Instance UUID */
  if (0 > discover_generate_uuid(&discover->iid)) {
    /* Unable to generate Instance UUID */
    sock_release(discover->sock);
    free(discover->options.address);
    free(discover->options.broadcast);
    free(discover->options.hostname);
    free(discover->pid);
    free(discover);
    return NULL;
  }
  
  /* Eligible as master by default */
  discover->is_master_eligible = true;
  
  /* Initialize semaphore used to access nodes */
  sem_init(&discover->nodes.sem, 0, 1);

  /* Initialize semaphore used to access channels */
  sem_init(&discover->channels.sem, 0, 1);
  
  /* Initialize semaphore used to access options */
  sem_init(&discover->options.sem, 0, 1);
  
  /* Register message and error callbacks */
  sock_on(discover->sock, "message", &discover_message_cb, discover);
  sock_on(discover->sock, "error", &discover_error_cb, discover);
  
  return discover;
}

/**
 * @brief Set discover options
 * @param discover Discover instance
 * @param option Option by name
 * @param value New value of the option
 * @return 0 if the function succeeded, -1 otherwise
 */
int discover_set_option(discover_t *discover, const char *option, void *value) {
  
  assert(NULL != discover);
  assert(NULL != option);
  assert(NULL != value);
  
  int ret = -1;
  
  /* Wait options semaphore */
  sem_wait(&discover->options.sem);
  
  /* Treatment depending of the option */
  if (!strcmp("helloInterval", option)) {
    discover->options.hello_interval = *((int *)value);
    ret = 0;
  } else if (!strcmp("checkInterval", option)) {
    int tmp = *((int *)value);
    if (tmp <= discover->options.node_timeout) {
      discover->options.check_interval = tmp;
      ret = 0;
    }
  } else if (!strcmp("nodeTimeout", option)) {
    int tmp = *((int *)value);
    if ((tmp >= discover->options.check_interval) && (tmp <= discover->options.master_timeout)) {
      discover->options.node_timeout = tmp;
      ret = 0;
    }
  } else if (!strcmp("masterTimeout", option)) {
    int tmp = *((int *)value);
    if (tmp >= discover->options.node_timeout) {
      discover->options.master_timeout = tmp;
      ret = 0;
    }
  } else if (!strcmp("address", option)) {
    if (NULL != discover->options.address) {
      free(discover->options.address);
    }
    discover->options.address = strdup((char *)value);
    if (NULL != discover->options.address) {
      ret = 0;
    }
  } else if (!strcmp("port", option)) {
    discover->options.port = *((uint16_t *)value);
    ret = 0;
  } else if (!strcmp("broadcast", option)) {
    if (NULL != discover->options.broadcast) {
      free(discover->options.broadcast);
    }
    discover->options.broadcast = strdup((char *)value);
    if (NULL != discover->options.broadcast) {
      ret = 0;
    }
  } else if (!strcmp("multicast", option)) {
    if (NULL != discover->options.multicast) {
      free(discover->options.multicast);
    }
    discover->options.multicast = strdup((char *)value);
    if (NULL != discover->options.multicast) {
      ret = 0;
    }
  } else if (!strcmp("multicastTTL", option)) {
    discover->options.multicast_ttl = *((unsigned char *)value);
    ret = 0;
  } else if (!strcmp("unicast", option)) {
    if (NULL != discover->options.unicast) {
      free(discover->options.unicast);
    }
    discover->options.unicast = strdup((char *)value);
    if (NULL != discover->options.unicast) {
      ret = 0;
    }
  } else if (!strcmp("key", option)) {
    if (NULL != discover->options.key) {
      free(discover->options.key);
    }
    discover->options.key = strdup((char *)value);
    if (NULL != discover->options.key) {
      ret = 0;
    }
  } else if (!strcmp("mastersRequired", option)) {
    discover->options.masters_required = *((int *)value);
    ret = 0;
  } else if (!strcmp("weight", option)) {
    discover->options.weight = *((double *)value);
    ret = 0;
  } else if (!strcmp("client", option)) {
    discover->options.client = *((bool *)value);
    ret = 0;
  } else if (!strcmp("reuseAddr", option)) {
    discover->options.reuse_addr = *((bool *)value);
    ret = 0;
  } else if (!strcmp("ignoreProcess", option)) {
    discover->options.ignore_process = *((bool *)value);
    ret = 0;
  } else if (!strcmp("ignoreInstance", option)) {
    discover->options.ignore_instance = *((bool *)value);
    ret = 0;
  } else if (!strcmp("advertisement", option)) {
    if (NULL != discover->options.advertisement) {
      cJSON_Delete(discover->options.advertisement);
    }
    discover->options.advertisement = (NULL != value) ? cJSON_Duplicate((cJSON *)value, 1) : NULL;
    ret = 0;
  } else if (!strcmp("hostname", option)) {
    if (NULL != discover->options.hostname) {
      free(discover->options.hostname);
    }
    discover->options.hostname = strdup((char *)value);
    if (NULL != discover->options.hostname) {
      ret = 0;
    }
  }
  
  /* Release options semaphore */
  sem_post(&discover->options.sem);

  return ret;
}

/**
 * @brief Start discovering
 * @param discover Discover instance
 * @return 0 if the function succeeded, -1 otherwise
 */
int discover_start(discover_t *discover) {
  
  assert(NULL != discover);

  /* Wait options semaphore */
  sem_wait(&discover->options.sem);

  /* Bind socket */
  if (NULL != discover->options.unicast) {
    sock_bind_unicast(discover->sock, discover->options.address, discover->options.port, discover->options.reuse_addr, discover->options.unicast);
  } else if (NULL != discover->options.multicast) {
    sock_bind_multicast(discover->sock, discover->options.address, discover->options.port, discover->options.reuse_addr, discover->options.multicast, discover->options.multicast_ttl);
  } else {
    sock_bind_broadcast(discover->sock, discover->options.address, discover->options.port, discover->options.reuse_addr, discover->options.broadcast);
  }
  
  /* Start periodic "check" task */
  if (0 != discover_start_check(discover)) {
    /* Unable to start task */
    sem_post(&discover->options.sem);
    return -1;
  }
  
  /* Start periodic "hello" task depending of the option */
  if (false == discover->options.client) {
    if (0 != discover_start_hello(discover)) {
      /* Unable to start task */
      sem_post(&discover->options.sem);
      return -1;
    }
  }
  
  /* Release options semaphore */
  sem_post(&discover->options.sem);

  return 0;
}

/**
 * @brief Register callbacks
 * @param discover Discover instance
 * @param topic Topic
 * @param fct Callback funtion
 * @param user User data
 * @return 0 if the function succeeded, -1 otherwise
 */
int discover_on(discover_t *discover, const char *topic, void *fct, void *user) {
  
  assert(NULL != discover);
  assert(NULL != topic);

  /* Record callback depending of the topic */
  if (!strcmp(topic, "helloReceived")) {
    discover->cb.hello_received.fct = fct;
    discover->cb.hello_received.user = user;
  } else if (!strcmp(topic, "helloEmitted")) {
    discover->cb.hello_emitted.fct = fct;
    discover->cb.hello_emitted.user = user;
  } else if (!strcmp(topic, "promotion")) {
    discover->cb.promotion.fct = fct;
    discover->cb.promotion.user = user;
  } else if (!strcmp(topic, "demotion")) {
    discover->cb.demotion.fct = fct;
    discover->cb.demotion.user = user;
  } else if (!strcmp(topic, "check")) {
    discover->cb.check.fct = fct;
    discover->cb.check.user = user;
  } else if (!strcmp(topic, "added")) {
    discover->cb.added.fct = fct;
    discover->cb.added.user = user;
  } else if (!strcmp(topic, "master")) {
    discover->cb.master.fct = fct;
    discover->cb.master.user = user;
  } else if (!strcmp(topic, "removed")) {
    discover->cb.removed.fct = fct;
    discover->cb.removed.user = user;
  } else if (!strcmp(topic, "error")) {
    discover->cb.error.fct = fct;
    discover->cb.error.user = user;
  }
  
  return 0;
}

/**
 * @brief Function used to set advertisement
 * @param discover Discover instance
 * @param advertisement Advestisement object
 * @return 0 if the function succeeded, -1 otherwise
 */
int discover_advertise(discover_t *discover, const cJSON *advertisement) {
  
  assert(NULL != discover);
  
  /* Wait options semaphore */
  sem_wait(&discover->options.sem);
  
  /* Set advertisement */
  if (NULL != discover->options.advertisement) {
    cJSON_Delete(discover->options.advertisement);
  }
  discover->options.advertisement = (NULL != advertisement) ? cJSON_Duplicate(advertisement, 1) : NULL;
  
  /* Release options semaphore */
  sem_post(&discover->options.sem);
  
  return 0;
}

/**
 * @brief Promote the instance to master
 * @param discover Discover instance
 * @return 0 if the function succeeded, -1 otherwise
 */
int discover_promote(discover_t *discover) {
  
  assert(NULL != discover);
  
  discover->is_master = true;
  discover->is_master_eligible = true;
  
  return 0;
}

/**
 * @brief Demote the instance from being a master
 * @param discover Discover instance
 * @param permanent true to specify that this should not automatically become master again, false otherwise
 * @return 0 if the function succeeded, -1 otherwise
 */
int discover_demote(discover_t *discover, bool permanent) {
  
  assert(NULL != discover);
  
  discover->is_master = false;
  discover->is_master_eligible = !permanent;
  
  return 0;
}

/**
 * @brief Subscribe to wanted event
 * @param discover Discover instance
 * @param event Event
 * @param fct Callback funtion
 * @param user User data
 * @return 0 if the function succeeded, -1 otherwise
 */
int discover_join(discover_t *discover, const char *event, void *fct, void *user) {
  
  assert(NULL != discover);
  assert(NULL != event);
  
  int ret = 0;
  discover_channel_t *last_channel = NULL;
  
  /* Wait semaphore */
  sem_wait(&discover->channels.sem);
  
  /* Parse channels, update callback and user data if event is found */
  discover_channel_t *curr_channel = discover->channels.first;
  while (NULL != curr_channel) {
    if (!strcmp(event, curr_channel->event)) {
      curr_channel->fct = fct;
      curr_channel->user = user;
      goto LEAVE;
    }
    last_channel = curr_channel;
    curr_channel = curr_channel->next;
  }
  
  /* Channel not found, add a new one */
  discover_channel_t *new_channel = (discover_channel_t *)malloc(sizeof(discover_channel_t));
  if (NULL == new_channel) {
    /* Unable to allocate memory */
    ret = -1;
    goto LEAVE;
  }
  memset(new_channel, 0, sizeof(discover_channel_t));
  new_channel->event = strdup(event);
  if (NULL == new_channel->event) {
    /* Unable to allocate memory */
    free(new_channel);
    ret = -1;
    goto LEAVE;
  }
  new_channel->fct = fct;
  new_channel->user = user;
  if (NULL != last_channel) {
    last_channel->next = new_channel;
  } else {
    discover->channels.first = new_channel;
  }
  
LEAVE:

  /* Release semaphore */
  sem_post(&discover->channels.sem);
  
  return ret;
}

/**
 * @brief Leave channel
 * @param discover Discover instance
 * @param event Event
 * @return 0 if the function succeeded, -1 otherwise
 */
int discover_leave(discover_t *discover, const char *event) {
  
  assert(NULL != discover);
  assert(NULL != event);
  
  discover_channel_t *last_channel = NULL;
  
  /* Wait semaphore */
  sem_wait(&discover->channels.sem);
  
  /* Parse channels, remove channel if event is found */
  discover_channel_t *curr_channel = discover->channels.first;
  while (NULL != curr_channel) {
    if (!strcmp(event, curr_channel->event)) {
      if (discover->channels.first == curr_channel) {
        discover->channels.first = curr_channel->next;
      } else {
        last_channel->next = curr_channel->next;
      }
      free(curr_channel->event);
      free(curr_channel);
      goto LEAVE;
    }
    last_channel = curr_channel;
    curr_channel = curr_channel->next;
  }
  
LEAVE:

  /* Release semaphore */
  sem_post(&discover->channels.sem);
  
  return 0;
}

/**
 * @brief Function used to send event data
 * @param discover Discover instance
 * @param event Event
 * @param data Data to send
 * @return 0 if the function succeeded, -1 otherwise
 */
int discover_send(discover_t *discover, const char* event, const cJSON *data) {
  
  assert(NULL != discover);
  assert(NULL != event);
  assert(NULL != data);
  
  /* Create message */
  cJSON *msg = cJSON_CreateObject();
  if (NULL == msg) {
    /* Unable to allocate memory */
    return -1;
  }
  
  /* Wait options semaphore */
  sem_wait(&discover->options.sem);
  
  /* Add fields to the message */
  cJSON_AddStringToObject(msg, "event", event);
  cJSON_AddStringToObject(msg, "pid", discover->pid);
  cJSON_AddStringToObject(msg, "iid", discover->iid);
  cJSON_AddStringToObject(msg, "hostName", discover->options.hostname);
  cJSON_AddItemToObject(msg, "data", cJSON_Duplicate(data, 1));
  
  /* Release options semaphore */
  sem_post(&discover->options.sem);
  
  /* Print to string */
  char *str = cJSON_PrintUnformatted(msg);
  if (NULL != str) {
    /* Send */
    sock_send(discover->sock, str, strlen(str));
  }
  
  /* Release memory */
  cJSON_Delete(msg);
  
  return 0;
}

/**
 * @brief Release discover instance
 * @param discover Discover instance
 */
void discover_release(discover_t *discover) {

  /* Release discover instance */
  if (NULL != discover) {
    
    /* Release sock instance */
    sock_release(discover->sock);
    
    /* Stop hello thread */
    pthread_cancel(discover->thread_hello);
    pthread_join(discover->thread_hello, NULL);
    
    /* Stop check thread */
    pthread_cancel(discover->thread_check);
    pthread_join(discover->thread_check, NULL);
    
    /* Release channels */
    sem_wait(&discover->channels.sem);
    discover_channel_t *curr_channel = discover->channels.first;
    while (NULL != curr_channel) {
      discover_channel_t *tmp = curr_channel;
      curr_channel = curr_channel->next;
      if (NULL != tmp->event) {
        free(tmp->event);
      }
      free(tmp);
    }
    sem_post(&discover->channels.sem);
    sem_close(&discover->channels.sem);
    
    /* Release nodes */
    sem_wait(&discover->nodes.sem);
    discover_node_t *node = discover->nodes.first;
    while (NULL != node) {
      discover_node_t *tmp = node;
      node = node->next;
      if (NULL != tmp->pid) {
        free(tmp->pid);
      }
      if (NULL != tmp->iid) {
        free(tmp->iid);
      }
      if (NULL != tmp->hostname) {
        free(tmp->hostname);
      }
      if (NULL != tmp->address) {
        free(tmp->address);
      }
      if (NULL != tmp->data.address) {
        free(tmp->data.address);
      }
      if (NULL != tmp->data.advertisement) {
        cJSON_Delete(tmp->data.advertisement);
      }
      free(tmp);
    }
    sem_post(&discover->nodes.sem);
    sem_close(&discover->nodes.sem);
    
    /* Release UUIDs */
    if (NULL != discover->pid) {
      free(discover->pid);
    }
    if (NULL != discover->iid) {
      free(discover->iid);
    }
    
    /* Release options */
    sem_wait(&discover->options.sem);
    if (NULL != discover->options.address) {
      free(discover->options.address);
    }
    if (NULL != discover->options.broadcast) {
      free(discover->options.broadcast);
    }
    if (NULL != discover->options.multicast) {
      free(discover->options.multicast);
    }
    if (NULL != discover->options.unicast) {
      free(discover->options.unicast);
    }
    if (NULL != discover->options.key) {
      free(discover->options.key);
    }
    if (NULL != discover->options.advertisement) {
      cJSON_Delete(discover->options.advertisement);
    }
    if (NULL != discover->options.hostname) {
      free(discover->options.hostname);
    }
    sem_post(&discover->options.sem);
    sem_close(&discover->options.sem);
    
    /* Release discover instance */
    free(discover);
  }
}

/**
 * @brief Start hello thread
 * @param discover Discover instance
 * @return 0 if the function succeeded, -1 otherwise
 */
static int discover_start_hello(discover_t *discover) {
  
  /* Initialize attributes of the thread */
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

  /* Start thread */
  if (0 != pthread_create(&discover->thread_hello, &attr, discover_thread_hello, (void *)discover)) {
    /* Unable to start the thread */
    return -1;
  }
  
  return 0;
}

/**
 * @brief Thread used to periodically send the hello message
 * @param arg Discover instance
 * @return Always returns NULL
 */
static void *discover_thread_hello(void *arg) {
  
  assert(NULL != arg);

  /* Retrieve discover */
  discover_t *discover = (discover_t *)arg;
  
  /* Infinite loop */
  while (1) {
    
    /* Create data object to be transmitted in the "hello" message */
    cJSON *data = cJSON_CreateObject();
    if (NULL != data) {
      cJSON *is_master = cJSON_CreateBool(discover->is_master);
      if (NULL != is_master) {
        cJSON_AddItemToObject(data, "isMaster", is_master);
      }
      cJSON *is_master_eligible = cJSON_CreateBool(discover->is_master_eligible);
      if (NULL != is_master_eligible) {
        cJSON_AddItemToObject(data, "isMasterEligible", is_master_eligible);
      }
      sem_wait(&discover->options.sem);
      cJSON_AddNumberToObject(data, "weight", discover->options.weight);
      if (NULL != discover->options.address) {
        cJSON_AddStringToObject(data, "address", discover->options.address);
      }
      if (NULL != discover->options.advertisement) {
        cJSON_AddItemToObject(data, "advertisement", cJSON_Duplicate(discover->options.advertisement, 1));
      }
      sem_post(&discover->options.sem);
    
      /* Send message */
      discover_send(discover, "hello", data);

      /* Invoke helloEmitted callback if defined */
      if (NULL != discover->cb.hello_emitted.fct) {
        discover->cb.hello_emitted.fct(discover, discover->cb.hello_emitted.user);
      }
      
      /* Release memory */
      cJSON_Delete(data);
    }
    
    /* Wait options semaphore */
    sem_wait(&discover->options.sem);
    
    /* Retrieve hello interval value */
    int hello_interval = discover->options.hello_interval;
    
    /* Release options semaphore */
    sem_post(&discover->options.sem);

    /* Sleep until the next loop */
    usleep(hello_interval * 1000);
  }
  
  return NULL;
}

/**
 * @brief Start check thread
 * @param discover Discover instance
 * @return 0 if the function succeeded, -1 otherwise
 */
static int discover_start_check(discover_t *discover) {
  
  /* Initialize attributes of the thread */
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

  /* Start thread */
  if (0 != pthread_create(&discover->thread_check, &attr, discover_thread_check, (void *)discover)) {
    /* Unable to start the thread */
    return -1;
  }
  
  return 0;
}

/**
 * @brief Thread used to periodically refresh the presence of other nodes
 * @param arg Discover instance
 * @return Always returns NULL
 */
static void *discover_thread_check(void *arg) {
  
  assert(NULL != arg);

  /* Retrieve discover */
  discover_t *discover = (discover_t *)arg;
  
  /* Infinite loop */
  while (1) {
    
    /* Flags */
    int masters_found = 0; int masters_higher_weight_found = 0; bool masters_eligible_higher_weight_found = false;
    
    /* Wait semaphores */
    sem_wait(&discover->nodes.sem);
    sem_wait(&discover->options.sem);
    
    /* Parse all nodes in the list */
    discover_node_t *node = discover->nodes.first;
    while (NULL != node) {
      discover_node_t *tmp = node;
      node = node->next;
      time_t now = time(NULL);
      if ((now < tmp->last_seen) || (now - tmp->last_seen > (((true == tmp->data.is_master) ? discover->options.master_timeout : discover->options.node_timeout) / 1000))) {
        /* Node is no more alive, remove it from the list */
        if (NULL != tmp->prev) {
          tmp->prev->next = tmp->next;
        } else {
          discover->nodes.first = tmp->next;
        }
        if (NULL != tmp->next) {
          tmp->next->prev = tmp->prev;
        } else {
          discover->nodes.last = tmp->prev;
        }
        /* Invoke removed callback if defined */
        if (NULL != discover->cb.removed.fct) {
          discover->cb.removed.fct(discover, tmp, discover->cb.removed.user);
        }
        /* Release memory */
        if (NULL != tmp->pid) {
          free(tmp->pid);
        }
        if (NULL != tmp->iid) {
          free(tmp->iid);
        }
        if (NULL != tmp->hostname) {
          free(tmp->hostname);
        }
        if (NULL != tmp->address) {
          free(tmp->address);
        }
        if (NULL != tmp->data.address) {
          free(tmp->data.address);
        }
        if (NULL != tmp->data.advertisement) {
          cJSON_Delete(tmp->data.advertisement);
        }
        free(tmp);
      } else {
        if ((true == tmp->data.is_master) && (discover->options.master_timeout > now - tmp->last_seen)) {
          /* One master found */
          masters_found++;
          if (discover->options.weight < tmp->data.weight) {
            /* Its weight is higher */
            masters_higher_weight_found++;
          }
        }
        if ((false == tmp->data.is_master) && (true == tmp->data.is_master_eligible)) {
          /* One eligible master found */
          if (discover->options.weight < tmp->data.weight) {
            /* Its weight is higher */
            masters_eligible_higher_weight_found = true;
          }
        }
      }
    }
    
    /* Check if I need to demote myself */
    bool was_master = discover->is_master;
    if ((true == was_master) && (discover->options.masters_required <= masters_higher_weight_found)) {
      discover->is_master = false;
      /* Invoke demotion callback if defined */
      if (NULL != discover->cb.demotion.fct) {
        discover->cb.demotion.fct(discover, discover->cb.demotion.user);
      }
    }
    
    /* Check if I need to promote myself */
    if ((false == was_master) && (true == discover->is_master_eligible) && (discover->options.masters_required > masters_higher_weight_found) && (false == masters_eligible_higher_weight_found)) {
      discover->is_master = true;
      /* Invoke promotion callback if defined */
      if (NULL != discover->cb.promotion.fct) {
        discover->cb.promotion.fct(discover, discover->cb.promotion.user);
      }
    }
    
    /* Invoke check callback if defined */
    if (NULL != discover->cb.check.fct) {
      discover->cb.check.fct(discover, discover->cb.check.user);
    }
    
    /* Retrieve check interval value */
    int check_interval = discover->options.check_interval;
    
    /* Release semaphores */
    sem_post(&discover->options.sem);
    sem_post(&discover->nodes.sem);
    
    /* Sleep until the next loop */
    usleep(check_interval * 1000);
  }
  
  return NULL;
}

/**
 * @brief Callback function called to handle received data
 * @param sock Sock instance
 * @param ip IP address of the sender
 * @param port Port of the sender
 * @param buffer Data received
 * @param size Size of data received
 * @param user User data
 */
static void discover_message_cb(const sock_t *sock, const char *ip, uint16_t port, const void *buffer, size_t size, void *user) {
  
  (void)sock;
  assert(NULL != ip);
  assert(NULL != buffer);
  (void)size;
  assert(NULL != user);
  
  /* Retrieve discover instance using user data */
  discover_t *discover = (discover_t *)user;
  
  /* Parse JSON string */
  cJSON *json = cJSON_Parse(buffer);
  if (NULL == json) {
    /* Unable to parse JSON string */
    return;
  }
  
  /* Wait options semaphore */
  sem_wait(&discover->options.sem);

  /* Check Process UUID */
  const cJSON *pid = cJSON_GetObjectItemCaseSensitive(json, "pid");
  if ((NULL == pid) || (!cJSON_IsString(pid))) {
    /* No Process UUID, ignore message */
    sem_post(&discover->options.sem);
    goto END;
  } else {
    if ((true == discover->options.ignore_process) && (!strcmp(cJSON_GetStringValue(pid), discover->pid))) {
      /* Ignore this message */
      sem_post(&discover->options.sem);
      goto END;
    }
  }

  /* Check Instance UUID */
  const cJSON *iid = cJSON_GetObjectItemCaseSensitive(json, "iid");
  if ((NULL == iid) || (!cJSON_IsString(iid))) {
    /* No Instance UUID, ignore message */
    sem_post(&discover->options.sem);
    goto END;
  } else {
    if ((true == discover->options.ignore_instance) && (!strcmp(cJSON_GetStringValue(iid), discover->iid))) {
      /* Ignore this message */
      sem_post(&discover->options.sem);
      goto END;
    }
  }
  
  /* Release options semaphore */
  sem_post(&discover->options.sem);
  
  /* Retrieve event */
  const cJSON *event = cJSON_GetObjectItemCaseSensitive(json, "event");
  if ((NULL != event) && (cJSON_IsString(event))) {
    
    /* Treatment depending of the event type */
    if (!strcmp(cJSON_GetStringValue(event), "hello")) {

      /* Hello event, retrieve data */
      const cJSON *data = cJSON_GetObjectItemCaseSensitive(json, "data");
      if ((NULL != data) && (cJSON_IsObject(data))) {

        /* Retrieve event fields */
        const cJSON *hostname = cJSON_GetObjectItemCaseSensitive(json, "hostName");
        if ((NULL == hostname) || (!cJSON_IsString(hostname))) {
          /* Invalid message, ignore */
          goto END;
        }
        
        /* Retrieve data fields */
        const cJSON *is_master = cJSON_GetObjectItemCaseSensitive(data, "isMaster");
        if ((NULL == is_master) || (!cJSON_IsBool(is_master))) {
          /* Invalid message, ignore */
          goto END;
        }
        const cJSON *is_master_eligible = cJSON_GetObjectItemCaseSensitive(data, "isMasterEligible");
        if ((NULL == is_master_eligible) || (!cJSON_IsBool(is_master_eligible))) {
          /* Invalid message, ignore */
          goto END;
        }
        const cJSON *weight = cJSON_GetObjectItemCaseSensitive(data, "weight");
        if ((NULL == weight) || (!cJSON_IsNumber(weight))) {
          /* Invalid message, ignore */
          goto END;
        }
        const cJSON *address = cJSON_GetObjectItemCaseSensitive(data, "address");
        if ((NULL == address) || (!cJSON_IsString(address))) {
          /* Invalid message, ignore */
          goto END;
        }
        const cJSON *advertisement = cJSON_GetObjectItemCaseSensitive(data, "advertisement");

        /* Flags */
        bool is_new = false; bool was_master = false;
        
        /* Wait semaphore */
        sem_wait(&discover->nodes.sem);
        
        /* Search node in the list */
        discover_node_t *node = discover->nodes.first;
        while (NULL != node) {
          if ((!strcmp(cJSON_GetStringValue(pid), node->pid)) && (!strcmp(cJSON_GetStringValue(iid), node->iid))) {
            break;
          }
          node = node->next;
        }
        if (NULL != node) {
          /* Node found, update the node */
          if (NULL != node->hostname) {
            free(node->hostname);
          }
          node->hostname = strdup(cJSON_GetStringValue(hostname));
          if (NULL != node->address) {
            free(node->address);
          }
          node->address = strdup(ip);
          node->port = port;
          node->last_seen = time(NULL);
          was_master = node->data.is_master;
          node->data.is_master = cJSON_IsTrue(is_master) ? true : false;
          node->data.is_master_eligible = cJSON_IsTrue(is_master_eligible) ? true : false;
          node->data.weight = cJSON_GetNumberValue(weight);
          if (NULL != node->data.address) {
            free(node->data.address);
          }
          node->data.address = strdup(cJSON_GetStringValue(address));
          if (NULL != node->data.advertisement) {
            cJSON_Delete(node->data.advertisement);
          }
          if (NULL != advertisement) {
            node->data.advertisement = cJSON_Duplicate(advertisement, 1);
          } else {
            node->data.advertisement = NULL;
          }
        } else {
          /* No node found, create a new one and add it at the end of the list */
          is_new = true;
          node = (discover_node_t *)malloc(sizeof(discover_node_t));
          if (NULL != node) {
            memset(node, 0, sizeof(discover_node_t));
            node->pid = strdup(cJSON_GetStringValue(pid));
            node->iid = strdup(cJSON_GetStringValue(iid));
            node->hostname = strdup(cJSON_GetStringValue(hostname));
            node->address = strdup(ip);
            node->port = port;
            node->last_seen = time(NULL);
            node->data.is_master = cJSON_IsTrue(is_master) ? true : false;
            node->data.is_master_eligible = cJSON_IsTrue(is_master_eligible) ? true : false;
            node->data.weight = cJSON_GetNumberValue(weight);
            node->data.address = strdup(cJSON_GetStringValue(address));
            if (NULL != advertisement) {
              node->data.advertisement = cJSON_Duplicate(advertisement, 1);
            }
            if (NULL == discover->nodes.last) {
              discover->nodes.first = discover->nodes.last = node;
            } else {
              node->prev = discover->nodes.last;
              discover->nodes.last->next = node;
              discover->nodes.last = node;
            }
          }
        }
        
        /* Check if the node is new */
        if ((NULL != node) && (true == is_new)) {
          /* Invoke added callback if defined */
          if (NULL != discover->cb.added.fct) {
            discover->cb.added.fct(discover, node, discover->cb.added.user);
          }
        }
        
        /* Check if node is a new master */
        if ((NULL != node) && (true == node->data.is_master) && ((true == is_new) || (false == was_master))) {
          /* Invoke master callback if defined */
          if (NULL != discover->cb.master.fct) {
            discover->cb.master.fct(discover, node, discover->cb.master.user);
          }
        }
        
        /* Release semaphore */
        sem_post(&discover->nodes.sem);
        
        /* Invoke helloReceived callback if defined */
        if (NULL != discover->cb.hello_received.fct) {
          discover->cb.hello_received.fct(discover, node, discover->cb.hello_received.user);
        }
      }

    } else {
      
      /* Other event, check channels */
      
      /* Wait channels semaphore */
      sem_wait(&discover->channels.sem);

      /* Invoke channel callback(s) if defined */
      if (NULL != discover->channels.first) {
      
        /* Parse all channels */
        discover_channel_t *curr_channel = discover->channels.first;
        while (NULL != curr_channel) {
          if (NULL != curr_channel->fct) {
            regex_t regex;
            if (0 == regcomp(&regex, curr_channel->event, REG_NOSUB | REG_EXTENDED)) {
              if (0 == regexec(&regex, cJSON_GetStringValue(event), 0, NULL, 0)) {
                /* Invoke channels callback */
                curr_channel->fct(discover, cJSON_GetStringValue(event), json, curr_channel->user);
              }
              regfree(&regex);
            }
          }
          curr_channel = curr_channel->next;
        }
      }

      /* Release channels semaphore */
      sem_post(&discover->channels.sem);
    }
  }
  
END:

  /* Release memory */
  cJSON_Delete(json);
}

/**
 * @brief Callback function called to handle error from sock instance
 * @param sock Sock instance
 * @param err Error as string
 * @param user User data
 */
static void discover_error_cb(const sock_t *sock, char *err, void *user) {

  (void)sock;
  assert(NULL != err);
  assert(NULL != user);
  
  /* Retrieve discover instance using user data */
  discover_t *discover = (discover_t *)user;
  
  /* Invoke error callback if defined */
  if (NULL != discover->cb.error.fct) {
    discover->cb.error.fct(discover, err, discover->cb.error.user);
  }
}

/**
 * @brief Generate UUID V4
 * @param buf Buffer to store UUID
 * @return 0 if the function succeeded, -1 otherwise
 */
static int discover_generate_uuid(char **buf) {
  
  assert(NULL != buf);
  
  /* Generate new UUID */
  UUID4_STATE_T state; UUID4_T uuid;
  uuid4_seed(&state); uuid4_gen(&state, &uuid);

  /* Convert UUID to string */
  if (NULL == (*buf = (char *)malloc(UUID4_STR_BUFFER_SIZE + 1))) {
    /* Unable to allocate memory */
    return -1;
  }
  memset(*buf, 0, UUID4_STR_BUFFER_SIZE + 1);
  if (false == uuid4_to_s(uuid, *buf, UUID4_STR_BUFFER_SIZE)) {
    /* Unable to generate UUID */
    return -1;
  }
  
  return 0;
}
