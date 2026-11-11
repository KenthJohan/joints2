# app1 GPU picking

`app.draw.GpuPicking` on an OpenGL window enables the experimental EgDraws
picking pass for that window. Without the tag, the window has no mouse picking;
there is no CPU fallback. `huds.flecs` contains a `Button2` example.

The pass stores the low 32 bits of each entity in a single-sample `GL_R32UI`
texture. It draws all of the window's EgDraws contexts in their normal order,
but only geometry whose entity has `eg.ui.script.events.Target` participates.
Untagged geometry does not block a target behind it. Text is picked only where
its atlas alpha multiplied by the vertex alpha is greater than zero. A tagged
entity without drawn geometry cannot be picked.

The existing EgUi dispatcher sends the resolved target to
`ecs_script_mouse_event`. The read uses the last rendered frame, so it may not
reflect ECS changes made since that frame. The first frame, a resize and a
minimized window have no pick until a valid frame has been rendered. Mouse
positions are converted from logical window coordinates to framebuffer pixels.
Readback is synchronous and may stall the GPU.

Enabling GPU picking installs app1's script-input `to_local` hook for ancestor
coordinates. Flecs event hooks are world-wide and this replaces any hooks
previously registered by the application.

Entity generations are not stored. If an entity is deleted and its index is
reused between rendering and readback, the result can refer to the new entity.
This experiment does not implement 3D picking.
