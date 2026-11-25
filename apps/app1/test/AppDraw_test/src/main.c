
/* A friendly warning from bake.test
 * ----------------------------------------------------------------------------
 * This file is generated. To add/remove testcases modify the 'project.json' of
 * the test project. ANY CHANGE TO THIS FILE IS LOST AFTER (RE)BUILDING!
 * ----------------------------------------------------------------------------
 */

#include <bake_test.h>

// Testsuite 'Plan'
void Plan_setup(void);
void Plan_teardown(void);
void Plan_test_compile_snapshot(void);
void Plan_test_reject_invalid(void);
void Plan_test_source_camera_and_skip(void);
void Plan_test_deleted_references(void);

// Testsuite 'Snapshot'
void Snapshot_setup(void);
void Snapshot_teardown(void);
void Snapshot_test_publish_and_invalidate(void);
void Snapshot_test_live_eligibility(void);
void Snapshot_test_on_demand(void);
void Snapshot_test_pick_target_lifecycle(void);
void Snapshot_test_renderer_lifecycle(void);
void Snapshot_test_multiple_windows(void);
void Snapshot_test_drag_window_bounds(void);
void Snapshot_test_window_state_system(void);
void Snapshot_test_frame_callbacks(void);
void Snapshot_test_render_cycle_picking(void);
void Snapshot_test_frame_plan_two_windows(void);
void Snapshot_test_native_world_fini(void);
void Snapshot_test_frame_snapshot_order(void);
void Snapshot_test_frame_plan_pick_hardware(void);
void Snapshot_test_external_window_ownership(void);
void Snapshot_test_missing_context_outcome(void);

bake_test_case Plan_testcases[] = {
    {
        "test_compile_snapshot",
        Plan_test_compile_snapshot
    },
    {
        "test_reject_invalid",
        Plan_test_reject_invalid
    },
    {
        "test_source_camera_and_skip",
        Plan_test_source_camera_and_skip
    },
    {
        "test_deleted_references",
        Plan_test_deleted_references
    }
};

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
        "test_renderer_lifecycle",
        Snapshot_test_renderer_lifecycle
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
        "test_window_state_system",
        Snapshot_test_window_state_system
    },
    {
        "test_frame_callbacks",
        Snapshot_test_frame_callbacks
    },
    {
        "test_render_cycle_picking",
        Snapshot_test_render_cycle_picking
    },
    {
        "test_frame_plan_two_windows",
        Snapshot_test_frame_plan_two_windows
    },
    {
        "test_native_world_fini",
        Snapshot_test_native_world_fini
    },
    {
        "test_frame_snapshot_order",
        Snapshot_test_frame_snapshot_order
    },
    {
        "test_frame_plan_pick_hardware",
        Snapshot_test_frame_plan_pick_hardware
    },
    {
        "test_external_window_ownership",
        Snapshot_test_external_window_ownership
    },
    {
        "test_missing_context_outcome",
        Snapshot_test_missing_context_outcome
    }
};

static bake_test_suite suites[] = {
    {
        "Plan",
        Plan_setup,
        Plan_teardown,
        4,
        Plan_testcases
    },
    {
        "Snapshot",
        Snapshot_setup,
        Snapshot_teardown,
        16,
        Snapshot_testcases
    }
};

int main(int argc, char *argv[]) {
    return bake_test_run("AppDraw_test", argc, argv, suites, 2);
}
