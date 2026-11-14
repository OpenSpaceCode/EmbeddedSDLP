/**
 * @file    test_runners.h
 * @brief   Per-module test runner interface
 *
 * Copyright 2026 OpenSpaceCode contributors
 * SPDX-License-Identifier: Apache-2.0
 *
 * OpenSpaceCode — https://github.com/OpenSpaceCode
 */

#ifndef TEST_RUNNERS_H
#define TEST_RUNNERS_H

/** @brief Pass/total tally returned by a module's test runner. */
typedef struct
{
    int passed; /**< Tests that passed. */
    int total;  /**< Tests that ran. */
} test_result_t;

/* Per-module test runners. Each runs all of its module's tests and returns the
 * passed/total tally. */
test_result_t test_tm_run_all(void);
test_result_t test_tc_run_all(void);

#endif /* TEST_RUNNERS_H */
