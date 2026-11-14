
/* A friendly warning from bake.test
 * ----------------------------------------------------------------------------
 * This file is generated. To add/remove testcases modify the 'project.json' of
 * the test project. ANY CHANGE TO THIS FILE IS LOST AFTER (RE)BUILDING!
 * ----------------------------------------------------------------------------
 */

#include <bake_test.h>

// Testsuite 'Snapshot'
void Snapshot_setup(void);
void Snapshot_teardown(void);
void Snapshot_test_publish_and_invalidate(void);
void Snapshot_test_live_eligibility(void);
void Snapshot_test_on_demand(void);
void Snapshot_test_pick_target_lifecycle(void);
void Snapshot_test_multiple_windows(void);
void Snapshot_test_drag_window_bounds(void);
void Snapshot_test_frame_callbacks(void);

bake_test_case Snapshot_testcases[] = {
    {
        "test_publish_and_invalidate",
        Snapshot_test_publish_and_invalidate
    },
    {
        "test_live_eligibility",
        Snapshot_test_live_eligibility
    },
    {
        "test_on_demand",
        Snapshot_test_on_demand
    },
    {
        "test_pick_target_lifecycle",
        Snapshot_test_pick_target_lifecycle
    },
    {
        "test_multiple_windows",
        Snapshot_test_multiple_windows
    },
    {
        "test_drag_window_bounds",
        Snapshot_test_drag_window_bounds
    },
    {
        "test_frame_callbacks",
        Snapshot_test_frame_callbacks
    }
};

static bake_test_suite suites[] = {
    {
        "Snapshot",
        Snapshot_setup,
        Snapshot_teardown,
        7,
        Snapshot_testcases
    }
};

int main(int argc, char *argv[]) {
    return bake_test_run("AppDraw_test", argc, argv, suites, 1);
}
