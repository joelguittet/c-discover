# c-discover

[![CMake Badge](https://github.com/joelguittet/c-discover/workflows/CMake%20+%20SonarCloud%20Analysis/badge.svg)](https://github.com/joelguittet/c-discover/actions)
[![Issues Badge](https://img.shields.io/github/issues/joelguittet/c-discover)](https://github.com/joelguittet/c-discover/issues)
[![License Badge](https://img.shields.io/github/license/joelguittet/c-discover)](https://github.com/joelguittet/c-discover/blob/master/LICENSE)

[![Bugs](https://sonarcloud.io/api/project_badges/measure?project=joelguittet_c-discover&metric=bugs)](https://sonarcloud.io/dashboard?id=joelguittet_c-discover)
[![Code Smells](https://sonarcloud.io/api/project_badges/measure?project=joelguittet_c-discover&metric=code_smells)](https://sonarcloud.io/dashboard?id=joelguittet_c-discover)
[![Duplicated Lines (%)](https://sonarcloud.io/api/project_badges/measure?project=joelguittet_c-discover&metric=duplicated_lines_density)](https://sonarcloud.io/dashboard?id=joelguittet_c-discover)
[![Lines of Code](https://sonarcloud.io/api/project_badges/measure?project=joelguittet_c-discover&metric=ncloc)](https://sonarcloud.io/dashboard?id=joelguittet_c-discover)
[![Vulnerabilities](https://sonarcloud.io/api/project_badges/measure?project=joelguittet_c-discover&metric=vulnerabilities)](https://sonarcloud.io/dashboard?id=joelguittet_c-discover)

[![Maintainability Rating](https://sonarcloud.io/api/project_badges/measure?project=joelguittet_c-discover&metric=sqale_rating)](https://sonarcloud.io/dashboard?id=joelguittet_c-discover)
[![Reliability Rating](https://sonarcloud.io/api/project_badges/measure?project=joelguittet_c-discover&metric=reliability_rating)](https://sonarcloud.io/dashboard?id=joelguittet_c-discover)
[![Security Rating](https://sonarcloud.io/api/project_badges/measure?project=joelguittet_c-discover&metric=security_rating)](https://sonarcloud.io/dashboard?id=joelguittet_c-discover)

Automatic and decentralized discovery and monitoring library in C. Built in support for a variable number of master processes, service advertising and channel messaging.

This repository is not a fork of [discover](https://github.com/wankdanker/node-discover) ! It has the same behavior but it is a complete library written in C in order to be portable and used in various applications. The goal of this library is to be fully compatible with discover Node.js version and permit discovery, monitoring and communications between applications written in C and Javascript.

## Features

*   automatic and decentralized discovery
*   monitoring of the other processes
*   service advertising
*   channel messaging

## Building

Build `libdiscover.so` with the following commands:

``` bash
cmake .
make
```

## Compatibility

This library is compatible with [discover](https://github.com/wankdanker/node-discover) release 1.2.0.

## Examples

Several examples are available in the `examples\` directory.

Build examples with the following commands:
``` bash
cmake -DENABLE_DISCOVER_EXAMPLES=ON .
make
```

### Basic / Basic Self / Basic Advertise

Show how to instanciate and configure a new discover instance in an application. Subscription to events (node added, removed, promotion, demotion...) and usage of channels to send messages.

### Test Multicast / Test Unicast

Show how to use Multicast or Unicast instead of Broadcast (the default).

## Performances

Performances have not been evaluated yet.

## What's it good for?

This goal of this library is to provide a C implementation of the original discover Node.js version. This allow a communication between processes written using different languages.

## API

### discover_t *discover_create(void)

Create a new discover instance.

### int discover_set_option(discover_t *discover, char *option, void *value)

Set discover instance `option` to the wanted `value` which is passed by address. The following table shows the available options and their default value. Options must be set before starting the instance.

| Option          | Type          | Default              |
|-----------------|---------------|----------------------|
| helloInterval   | int           | 1000ms               |
| checkInterval   | int           | 2000ms               |
| nodeTimeout     | int           | 2000ms               |
| masterTimeout   | int           | 2000ms               |
| address         | char *        | "0.0.0.0"            |
| port            | uint16_t      | 12345                |
| broadcast       | char *        | "255.255.255.255"    |
| multicast       | char *        | NULL                 |
| multicastTTL    | unsigned char | 1                    |
| unicast         | char *        | NULL                 |
| key             | char *        | NULL                 |
| mastersRequired | int           | 1                    |
| weight          | double        | Computed on startup  |
| client          | bool          | false                |
| reuseAddr       | bool          | true                 |
| ignoreProcess   | bool          | false                |
| ignoreInstance  | bool          | false                |
| advertisement   | cJSON *       | NULL                 |
| hostname        | char *        | Retrieved on startup |

| :exclamation: The key can't be used today. Encryption of data is not available. First because I have not found any simple and satisfying library to do it, and then because the Cipher initialization used in discover Node.js version is currently deprecated. |
|-|

### int discover_start(discover_t *discover)

Start the discover instance.

### int discover_on(discover_t *discover, char *topic, void *fct, void *user)

Register a callback `fct` on the event `topic`. An optionnal `user` argument is available. The following table shows the available topics and their callback prototype.

| Topic         | Callback                                                     | Description                                |
|---------------|--------------------------------------------------------------|--------------------------------------------|
| helloReceived | void *(*fct)(struct discover_s *, discover_node_t *, void *) | Called when hello message is received      |
| helloEmitted  | void *(*fct)(struct discover_s *, void *)                    | Called when hello message is emitted       |
| promotion     | void *(*fct)(struct discover_s *, void *)                    | Called when the instance is promoted       |
| demotion      | void *(*fct)(struct discover_s *, void *)                    | Called when the instance is demoted        |
| check         | void *(*fct)(struct discover_s *, void *)                    | Called when the check function is executed |
| added         | void *(*fct)(struct discover_s *, discover_node_t *, void *) | Called when a node is discovered           |
| master        | void *(*fct)(struct discover_s *, discover_node_t *, void *) | Called when a node is promoted             |
| removed       | void *(*fct)(struct discover_s *, discover_node_t *, void *) | Called when a node has disappeared         |
| error         | void *(*fct)(struct discover_s *, char *, void *)            | Called when an error occured               |

### int discover_advertise(discover_t *discover, cJSON *advertisement)

Set `advertisement` object. Can be used after starting the instance to update the advertisement content.

### int discover_promote(discover_t *discover)

Promote my own instance.

### int discover_demote(discover_t *discover, bool permanent)

Demote my own instance. If `permanent` is true, prevent the instance to promote itself automatically.

### int discover_join(discover_t *discover, char *event, void *fct, void *user)

Register a callback `fct` on the channel `event`. An optionnal `user` argument is available.

### int discover_leave(discover_t *discover, char *event)

Unregister to the `event`.

### int discover_send(discover_t *discover, char* event, cJSON *data)

Send `data` to the channel `event`.

### void discover_release(discover_t *discover)

Release internal memory and stop discover instance. All sockets are closed. Must be called to free ressources.

## License

MIT
