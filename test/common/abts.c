/*************************************************************************** 

    Copyright (C) 2019 NextEPC Inc. All rights reserved.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

***************************************************************************/


/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "abts.h"
#include "testutil.h"

#include "core_debug.h"
#include "core_pkbuf.h"

#define ABTS_STAT_SIZE 6
static char status[ABTS_STAT_SIZE] = {'|', '/', '-', '|', '\\', '-'};
static int curr_char;
static int verbose = 1;
static int exclude = 0;
static int quiet = 0;
static int list_tests = 0;
int test_only_control_plane = 0;

const char **testlist = NULL;
extern const struct testlist alltests[];

static int find_test_name(const char *testname) {
    int i;
    for (i = 0; testlist[i] != NULL; i++) {
        if (!strcmp(testlist[i], testname)) {
            return 1;
        }
    }
    return 0;
}

/* Determine if the test should be run at all */
static int should_test_run(const char *testname) {
    int found = 0;
    if (list_tests == 1) {
        return 0;
    }
    if (testlist == NULL) {
        return 1;
    }
    found = find_test_name(testname);
    if ((found && !exclude) || (!found && exclude)) {
        return 1;
    }
    return 0;
}

static void reset_status(void)
{
    curr_char = 0;
}

static void update_status(void)
{
    if (!quiet) {
        curr_char = (curr_char + 1) % ABTS_STAT_SIZE;
        fprintf(stdout, "\b%c", status[curr_char]);
        fflush(stdout);
    }
}

static void end_suite(abts_suite *suite)
{
    if (suite != NULL) {
        sub_suite *last = suite->tail;
        if (!quiet) {
            fprintf(stdout, "\b");
            fflush(stdout);
        }
        if (last->failed == 0) {
            fprintf(stdout, "SUCCESS\n");
            fflush(stdout);
        }
        else {
            fprintf(stdout, "FAILED %d of %d\n", last->failed, last->num_test);
            fflush(stdout);
        }
    }
}

abts_suite *abts_add_suite(abts_suite *suite, const char *suite_name_full)
{
    sub_suite *subsuite;
    char *p;
    const char *suite_name;
    curr_char = 0;

    /* Only end the suite if we actually ran it */
    if (suite && suite->tail &&!suite->tail->not_run) {
        end_suite(suite);
    }

    subsuite = core_malloc(sizeof(*subsuite));
    subsuite->num_test = 0;
    subsuite->failed = 0;
    subsuite->next = NULL;
    /* suite_name_full may be an absolute path depending on __FILE__
     * expansion */
    suite_name = strrchr(suite_name_full, '/');
    if (suite_name) {
        suite_name++;
    } else {
        suite_name = suite_name_full;
    }
    p = strrchr(suite_name, '.');
    if (p) {
        subsuite->name = memcpy(core_calloc(p - suite_name + 1, 1),
                                suite_name, p - suite_name);
    }
    else {
        subsuite->name = suite_name;
    }

    if (list_tests) {
        fprintf(stdout, "%s\n", subsuite->name);
    }

    subsuite->not_run = 0;

    if (suite == NULL) {
        suite = core_malloc(sizeof(*suite));
        suite->head = subsuite;
        suite->tail = subsuite;
    }
    else {
        suite->tail->next = subsuite;
        suite->tail = subsuite;
    }

    if (!should_test_run(subsuite->name)) {
        subsuite->not_run = 1;
        return suite;
    }

    reset_status();
    fprintf(stdout, "%-20s:  ", subsuite->name);
    update_status();
    fflush(stdout);

    return suite;
}

void abts_run_test(abts_suite *ts, test_func f, void *value)
{
    abts_case tc;
    sub_suite *ss;

    if (!should_test_run(ts->tail->name)) {
        return;
    }
    ss = ts->tail;

    tc.failed = 0;
    tc.suite = ss;

    ss->num_test++;
    update_status();

    f(&tc, value);

    if (tc.failed) {
        ss->failed++;
    }
}

static int report(abts_suite *suite)
{
    int count = 0;
    sub_suite *dptr;

    if (suite && suite->tail &&!suite->tail->not_run) {
        end_suite(suite);
    }

    for (dptr = suite->head; dptr; dptr = dptr->next) {
        count += dptr->failed;
    }

    if (list_tests) {
        return 0;
    }

    if (count == 0) {
        printf("All tests passed.\n");
        return 0;
    }

    dptr = suite->head;
    fprintf(stdout, "%-15s\t\tTotal\tFail\tFailed %%\n", "Failed Tests");
    fprintf(stdout, "===================================================\n");
    while (dptr != NULL) {
        if (dptr->failed != 0) {
            float percent = ((float)dptr->failed / (float)dptr->num_test);
            fprintf(stdout, "%-15s\t\t%5d\t%4d\t%6.2f%%\n", dptr->name,
                    dptr->num_test, dptr->failed, percent * 100);
        }
        dptr = dptr->next;
    }
    return 1;
}

static void abts_free(abts_suite *suite)
{
    sub_suite *ptr = NULL, *next_ptr = NULL;

    ptr = suite->head;
    while (ptr != NULL) {
        next_ptr = ptr->next;

        CORE_FREE((void*)ptr->name);
        CORE_FREE(ptr);
        ptr = next_ptr;
    }

    CORE_FREE(suite);
}

void abts_log_message(const char *fmt, ...)
{
    va_list args;
    update_status();

    if (verbose) {
        va_start(args, fmt);
        vfprintf(stderr, fmt, args);
        va_end(args);
        fprintf(stderr, "\n");
        fflush(stderr);
    }
}

void abts_int_equal(abts_case *tc, const int expected, const int actual, int lineno)
{
    update_status();
    if (tc->failed) return;

    if (expected == actual) return;

    tc->failed = TRUE;
    if (verbose) {
        fprintf(stderr, "Line %d: expected <%d>, but saw <%d>\n", lineno, expected, actual);
        fflush(stderr);
    }
}

void abts_int_nequal(abts_case *tc, const int expected, const int actual, int lineno)
{
    update_status();
    if (tc->failed) return;

    if (expected != actual) return;

    tc->failed = TRUE;
    if (verbose) {
        fprintf(stderr, "Line %d: expected <%d>, but saw <%d>\n", lineno, expected, actual);
        fflush(stderr);
    }
}

void abts_size_equal(abts_case *tc, size_t expected, size_t actual, int lineno)
{
    update_status();
    if (tc->failed) return;

    if (expected == actual) return;

    tc->failed = TRUE;
    if (verbose) {
        /* Note that the comparison is type-exact, reporting must be a best-fit */
        fprintf(stderr, "Line %d: expected %lu, but saw %lu\n", lineno,
                (unsigned long)expected, (unsigned long)actual);
        fflush(stderr);
    }
}

void abts_str_equal(abts_case *tc, const char *expected, const char *actual, int lineno)
{
    update_status();
    if (tc->failed) return;

    if (!expected && !actual) return;
    if (expected && actual)
        if (!strcmp(expected, actual)) return;

    tc->failed = TRUE;
    if (verbose) {
        fprintf(stderr, "Line %d: expected <%s>, but saw <%s>\n", lineno, expected, actual);
        fflush(stderr);
    }
}

void abts_str_nequal(abts_case *tc, const char *expected, const char *actual,
                       size_t n, int lineno)
{
    update_status();
    if (tc->failed) return;

    if (!strncmp(expected, actual, n)) return;

    tc->failed = TRUE;
    if (verbose) {
        fprintf(stderr, "Line %d: expected <%s>, but saw <%s>\n", lineno, expected, actual);
        fflush(stderr);
    }
}

void abts_ptr_null(abts_case *tc, const void *ptr, int lineno)
{
    update_status();
    if (tc->failed) return;

    if (ptr == NULL) return;

    tc->failed = TRUE;
    if (verbose) {
        fprintf(stderr, "Line %d: Expected NULL, but saw <%p>\n", lineno, ptr);
        fflush(stderr);
    }
}

void abts_ptr_notnull(abts_case *tc, const void *ptr, int lineno)
{
    update_status();
    if (tc->failed) return;

    if (ptr != NULL) return;

    tc->failed = TRUE;
    if (verbose) {
        fprintf(stderr, "Line %d: Expected not NULL, but saw <%p>\n", lineno, ptr);
        fflush(stderr);
    }
}

void abts_ptr_equal(abts_case *tc, const void *expected, const void *actual, int lineno)
{
    update_status();
    if (tc->failed) return;

    if (expected == actual) return;

    tc->failed = TRUE;
    if (verbose) {
        fprintf(stderr, "Line %d: expected <%p>, but saw <%p>\n", lineno, expected, actual);
        fflush(stderr);
    }
}

void abts_fail(abts_case *tc, const char *message, int lineno)
{
    update_status();
    if (tc->failed) return;

    tc->failed = TRUE;
    if (verbose) {
        fprintf(stderr, "Line %d: %s\n", lineno, message);
        fflush(stderr);
    }
}

void abts_assert(abts_case *tc, const char *message, int condition, int lineno)
{
    update_status();
    if (tc->failed) return;

    if (condition) return;

    tc->failed = TRUE;
    if (verbose) {
        fprintf(stderr, "Line %d: %s\n", lineno, message);
        fflush(stderr);
    }
}

void abts_true(abts_case *tc, int condition, int lineno)
{
    update_status();
    if (tc->failed) return;

    if (condition) return;

    tc->failed = TRUE;
    if (verbose) {
        fprintf(stderr, "Line %d: Condition is false, but expected true\n", lineno);
        fflush(stderr);
    }
}

void abts_false(abts_case *tc, int condition, int lineno)
{
    update_status();
    if (tc->failed) return;

    if (!condition) return;

    tc->failed = TRUE;
    if (verbose) {
        fprintf(stderr, "Line %d: Condition is true, but expected false\n", lineno);
        fflush(stderr);
    }
}

void abts_not_impl(abts_case *tc, const char *message, int lineno)
{
    update_status();

    tc->suite->not_impl++;
    if (verbose) {
        fprintf(stderr, "Line %d: %s\n", lineno, message);
        fflush(stderr);
    }
}

int main(int argc, const char *const argv[]) {
    int i;
    int rv;
    int list_provided = 0;
    abts_suite *suite = NULL;
    const char *config_path = NULL;

    d_trace_global_off();
    d_log_set_level(D_MSG_TO_STDOUT, D_LOG_LEVEL_ERROR);

    quiet = !isatty(STDOUT_FILENO);

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-v")) {
            verbose = 1;
            continue;
        }
        if (!strcmp(argv[i], "-x")) {
            exclude = 1;
            continue;
        }
        if (!strcmp(argv[i], "-l")) {
            list_tests = 1;
            continue;
        }
        if (!strcmp(argv[i], "-q")) {
            quiet = 1;
            continue;
        }
        if (!strcmp(argv[i], "-t")) {
            d_trace_global_on();
            d_log_set_level(D_MSG_TO_STDOUT, D_LOG_LEVEL_FULL);
            continue;
        }
        if (!strcmp(argv[i], "-f")) {
            config_path = argv[++i];
            continue;
        }
        if (!strcmp(argv[i], "-c")) {
            test_only_control_plane = 1;
            continue;
        }
        if (argv[i][0] == '-') {
            fprintf(stderr, "Invalid option: `%s'\n", argv[i]);
            exit(1);
        }
        list_provided = 1;
    }

    rv = test_initialize(argc, argv, (char*)config_path);
    if (rv != CORE_OK) 
        return EXIT_FAILURE;

    if (list_provided) {
        /* Waste a little space here, because it is easier than counting the
         * number of tests listed.  Besides it is at most three char *.
         */
        testlist = core_calloc(argc + 1, sizeof(char *));
        for (i = 1; i < argc; i++) {
            testlist[i - 1] = argv[i];
        }
    }

    for (i = 0; alltests[i].func; i++) {
        suite = alltests[i].func(suite);
    }

    rv = report(suite);

    abts_free(suite);

    CORE_FREE(testlist);
    return rv;
}

