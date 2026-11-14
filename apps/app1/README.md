# app1 GPU picking

`app.draw.GpuPicking` on an OpenGL window enables the experimental EgDraws
picking pass for that window. Without the tag, the window has no mouse picking;
there is no CPU fallback. `huds.flecs` contains a `Button2` example.

The pass stores the low 32 bits of each entity and a primitive draw-order key
in a single-sample `GL_RG32UI` texture. It draws all of the window's EgDraws contexts in their normal order,
but only geometry whose entity has `eg.ui.script.events.Target` participates.
Untagged geometry does not block a target behind it. Text is picked only where
its atlas alpha multiplied by the vertex alpha is greater than zero. A tagged
entity without drawn geometry cannot be picked.

The existing EgUi dispatcher sends the resolved target to
`ecs_script_mouse_event`. During the window's render cycle, a scissored 3×3
framebuffer-pixel region is drawn alongside the visible draw lists while the
window's GL context is current. The region is read before presentation; only
after a successful presentation does the input picker use the resulting target
in the next frame.
The center pixel wins; otherwise the nearest nonempty pixel wins, with the
last-drawn primitive breaking distance ties. If that still ties, the first
pixel in bottom-to-top, left-to-right framebuffer order wins. The region is
clipped at window edges. This makes nearby objects clickable even when the
center pixel is empty.

The result uses the pointer position and geometry from the preceding render.
Moving the pointer before the next input event can therefore target an object
that is no longer under it. A result is rejected if its entity is no longer
alive or eligible, or if the window state and framebuffer size are invalid.
The first frame, a resize and a minimized window have no pick until a valid
frame has been rendered. Mouse positions are converted from logical window
coordinates to framebuffer pixels. Each eligible frame performs synchronous
readback, which may stall the GPU. Eligibility checks also cost CPU time and
memory. Buttons retain frame-based input aggregation.
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

The GPU stores only entity indices. If an entity is deleted and its index is
reused between geometry generation and readback, the result can refer to the
new entity. After readback the full entity ID is retained, preventing an index
reused before event delivery from selecting a different entity. This experiment
does not implement 3D picking.
