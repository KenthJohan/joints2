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
`ecs_script_mouse_event`. The picker retains the last rendered frame's ordered
geometry and renders a scissored 3×3 framebuffer-pixel region on demand.
The center pixel wins; otherwise the nearest nonempty pixel wins, with the
last-drawn primitive breaking distance ties. If that still ties, the first
pixel in bottom-to-top, left-to-right framebuffer order wins. The region is
clipped at window edges. This makes nearby objects clickable even when the
center pixel is empty.

Mouse movement, an observed button transition, focus or framebuffer changes,
and changes to the retained scene or target eligibility invalidate the cached
result. Consequently a stationary cursor still tracks moving objects, but an
animated scene can require a pick every frame. Snapshot copying, comparison,
and eligibility checks also cost CPU time and memory. The read uses the last
rendered frame, so it may not reflect geometry or camera changes made since
that frame. The first frame, a resize and a minimized window have no pick until
a valid frame has been rendered. Mouse positions are converted from logical
window coordinates to framebuffer pixels. Readback is synchronous and may
stall the GPU. Buttons retain frame-based input aggregation.
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

Entity generations are not stored. If an entity is deleted and its index is
reused between rendering and readback, the result can refer to the new entity.
This experiment does not implement 3D picking.
