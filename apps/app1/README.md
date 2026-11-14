# app1 GPU picking

`app.draw.GpuPicking` on an OpenGL window enables the experimental EgDraws
picking pass for that window. Without the tag, the window has no mouse picking;
there is no CPU fallback. `huds.flecs` contains a `Button2` example.

Picking runs in the same pass as visible drawing. Only geometry whose entity
has `eg.ui.script.events.Target` participates.
Untagged geometry does not block a target behind it. Text is picked only where
its atlas alpha multiplied by the vertex alpha is greater than zero. A tagged
entity without drawn geometry cannot be picked.

The existing EgUi dispatcher sends the resolved target to
`ecs_script_mouse_event`. The fragment shader selects eligible fragments at the
exact mouse pixel with a 64-bit SSBO atomic. The smallest window-space depth
wins, with the last-drawn primitive breaking exact depth ties. Visible rendering
still uses its existing blending and draw order, so the picked object can differ
from the visually topmost object. Depth values from different cameras are
compared in window space.

The result is read asynchronously after the frame is presented. The latest
completed valid result is retained while newer results are pending; when all
three readback slots are busy, a new pick is skipped. Moving the pointer can
therefore target an object that is no longer under it. A result is rejected if
its entity is no longer alive or eligible, or if the window state and framebuffer
size are invalid. The first frame, a resize and a minimized window have no pick
until a valid result is ready. Mouse positions are converted from logical window
coordinates to framebuffer pixels. Eligibility mapping still costs CPU time and
memory. Buttons retain frame-based input aggregation.

The SDL backend requests OpenGL 4.3 core. GPU picking additionally requires
`GL_ARB_gpu_shader_int64` and `GL_NV_shader_atomic_int64`; a context without
both extensions reports that picking is unavailable. Rendering without picking
does not require those extensions.

For a synthetic comparison of the visible-only, former synchronous and
single-pass asynchronous renderer paths on a supporting OpenGL context, run
`EGG_PICK_BENCHMARK=1 bake test vendor/egg/test/egg_test` from the repository
root. The benchmark reports skipped picks as well as frame time; it does not
measure the complete application.
The current Flecs input dispatcher does not deliver a release to the pressed
entity when the pointer is over the background, so the example button's
`held` state can remain set after dragging off and releasing.

Resize handles opt into `eg.ui.script.events.Capture`. A left-button press
captures that handle until release, so `on.drag` keeps bubbling to the
rectangle even over other objects inside the window. Leaving the window, losing
window focus, closing the window, or disabling/removing the handle cancels the
capture. App1 does not request SDL mouse capture for outside-window dragging.
Other mouse buttons are ignored during the captured drag until they are
released. If a handle is deleted mid-drag it cannot receive its release; its
owner's script must clean up any state that outlives that handle.

Flecs sees the captured handle as the target throughout the drag and release.
Its hover therefore remains on the handle during capture, and its normal click
event may also be delivered on release outside it (or on cancellation).
Ordinary buttons do not opt in and retain their existing behavior.

Enabling GPU picking installs app1's script-input `to_local` hook for ancestor
coordinates. Flecs event hooks are world-wide and this replaces any hooks
previously registered by the application.

Window focus, mouse coordinates and logical/framebuffer sizes are read from
`EgWindows` state, refreshed by `EgWindowsSdl` after polling events each frame.
The SDL GL backend supplies context switching, frame callbacks, and
context-current resource cleanup through `EgWindows`, so AppDraw does not
depend on SDL APIs. Renderer and picking resources are created during rendering
and cleaned up when their components or owning window/context are removed.
Other backends must provide the same state and cleanup operation. Native window
and GL context handles must remain valid until their components are removed;
they must not be replaced on the same entity.

The draw list stores only entity indices. A reused index can therefore refer to
a new entity before draw submission. At submission the full entity ID is saved
with the pending result, preventing a later reuse before event delivery from
selecting a different entity. This experiment does not implement 3D picking.
