/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * Device Tree Helper Macros for Layered Queue Drivers
 */

#ifndef ZEPHYR_INCLUDE_DT_BINDINGS_LAYERED_QUEUE_H_
#define ZEPHYR_INCLUDE_DT_BINDINGS_LAYERED_QUEUE_H_

/* Status codes for use in device tree */
#define LQ_OK           0
#define LQ_DEGRADED     1
#define LQ_OUT_OF_RANGE 2
#define LQ_ERROR        3
#define LQ_TIMEOUT      4
#define LQ_INCONSISTENT 5

/* Drop policies */
#define LQ_DROP_OLDEST  0
#define LQ_DROP_NEWEST  1
#define LQ_BLOCK        2

/* Voting methods */
#define LQ_VOTE_MEDIAN   0
#define LQ_VOTE_AVERAGE  1
#define LQ_VOTE_MIN      2
#define LQ_VOTE_MAX      3
#define LQ_VOTE_MAJORITY 4

#endif /* ZEPHYR_INCLUDE_DT_BINDINGS_LAYERED_QUEUE_H_ */
