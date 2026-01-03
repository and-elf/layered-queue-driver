/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * Common definitions
 */

#ifndef LQ_COMMON_H_
#define LQ_COMMON_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Voting method for merge/voter
 */
enum lq_vote_method {
    LQ_VOTE_MEDIAN,      /**< Use median value */
    LQ_VOTE_AVERAGE,     /**< Use average of all values */
    LQ_VOTE_MIN,         /**< Use minimum value */
    LQ_VOTE_MAX,         /**< Use maximum value */
};

#ifdef __cplusplus
}
#endif

#endif /* LQ_COMMON_H_ */
