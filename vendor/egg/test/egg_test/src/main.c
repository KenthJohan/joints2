
/* A friendly warning from bake.test
 * ----------------------------------------------------------------------------
 * This file is generated. To add/remove testcases modify the 'project.json' of
 * the test project. ANY CHANGE TO THIS FILE IS LOST AFTER (RE)BUILDING!
 * ----------------------------------------------------------------------------
 */

#include <bake_test.h>

// Testsuite 'Picking'
void Picking_setup(void);
void Picking_teardown(void);
void Picking_test_integer_target(void);
void Picking_test_region(void);
void Picking_test_async(void);
void Picking_test_benchmark(void);

bake_test_case Picking_testcases[] = {
    {
        "test_integer_target",
        Picking_test_integer_target
    },
    {
        "test_region",
        Picking_test_region
    },
    {
        "test_async",
        Picking_test_async
    },
    {
        "test_benchmark",
        Picking_test_benchmark
    }
};

static bake_test_suite suites[] = {
    {
        "Picking",
        Picking_setup,
        Picking_teardown,
        4,
        Picking_testcases
    }
};

int main(int argc, char *argv[]) {
    return bake_test_run("egg_test", argc, argv, suites, 1);
}
