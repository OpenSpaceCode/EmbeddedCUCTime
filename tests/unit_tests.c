#include "test_runners.h"

#include <stdio.h>

#define REPORT(label, r) printf("  %-14s Passed %d/%d\n\n", label ":", (r).passed, (r).total)

int main(void)
{
    test_result_t r;
    int total_passed = 0;
    int total_tests = 0;

    r = test_cuc_run_all();
    REPORT("cuc", r);
    total_passed += r.passed;
    total_tests += r.total;

    r = test_cds_run_all();
    REPORT("cds", r);
    total_passed += r.passed;
    total_tests += r.total;

    r = test_ccs_run_all();
    REPORT("ccs", r);
    total_passed += r.passed;
    total_tests += r.total;

    printf("  ------------------------------\n");
    printf("  %-14s Passed %d/%d\n", "All UT:", total_passed, total_tests);

    return (total_passed == total_tests) ? 0 : 1;
}
