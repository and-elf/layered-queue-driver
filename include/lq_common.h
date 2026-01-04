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

/**
 * @brief Fault severity level
 * 
 * Configurable via LQ_MAX_FAULT_LEVELS (default 4).
 * Allows different safety responses based on severity.
 */
enum lq_fault_level {
    LQ_FAULT_LEVEL_0 = 0,    /**< Informational / No fault */
    LQ_FAULT_LEVEL_1 = 1,    /**< Warning / Soft fault */
    LQ_FAULT_LEVEL_2 = 2,    /**< Error / Medium fault */
    LQ_FAULT_LEVEL_3 = 3,    /**< Critical / Hard fault */
#if defined(LQ_MAX_FAULT_LEVELS) && LQ_MAX_FAULT_LEVELS > 4
    LQ_FAULT_LEVEL_4 = 4,
#endif
#if defined(LQ_MAX_FAULT_LEVELS) && LQ_MAX_FAULT_LEVELS > 5
    LQ_FAULT_LEVEL_5 = 5,
#endif
#if defined(LQ_MAX_FAULT_LEVELS) && LQ_MAX_FAULT_LEVELS > 6
    LQ_FAULT_LEVEL_6 = 6,
#endif
#if defined(LQ_MAX_FAULT_LEVELS) && LQ_MAX_FAULT_LEVELS > 7
    LQ_FAULT_LEVEL_7 = 7,
#endif
};

#ifdef __cplusplus
}
#endif

#endif /* LQ_COMMON_H_ */
