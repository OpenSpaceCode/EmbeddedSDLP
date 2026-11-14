/**
 * @file    unit_tests.c
 * @brief   Unit test entry point: runs each module's suite and reports the tally.
 *
 * Copyright 2026 OpenSpaceCode contributors
 * SPDX-License-Identifier: Apache-2.0
 *
 * OpenSpaceCode — https://github.com/OpenSpaceCode
 */
#include "test_runners.h"

#include <stdio.h>

#define REPORT(label, r) printf("  %-14s Passed %d/%d\n\n", label ":", (r).passed, (r).total)

int main(void)
{
    test_result_t r;
    int total_passed = 0;
    int total_tests = 0;

    r = test_tm_run_all();
    REPORT("tm", r);
    total_passed += r.passed;
    total_tests += r.total;

    r = test_tc_run_all();
    REPORT("tc", r);
    total_passed += r.passed;
    total_tests += r.total;

    printf("  ------------------------------\n");
    printf("  %-14s Passed %d/%d\n", "All UT:", total_passed, total_tests);

    return (total_passed == total_tests) ? 0 : 1;
}
