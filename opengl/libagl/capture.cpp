/*
 * RK2818 private software render target for SurfaceFlinger screen captures.
 * Copyright (C) 2026 The Android Open Source Project
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy at http://www.apache.org/licenses/LICENSE-2.0
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "context.h"
#include <stdlib.h>
#include <string.h>
#include "matrix.h"

namespace android {

struct RK28CaptureTarget {
    ogles_context_t* context;
    GGLSurface color, read, target;
    viewport_t viewport;
    bool scissorEnabled;
};

extern "C" __attribute__((visibility("default")))
void* rk28_begin_capture(uint32_t width, uint32_t height) {
    if (!width || !height || width > 4096 || height > 4096) return NULL;
    ogles_context_t* c = ogles_context_t::get();
    if (!c) return NULL;
    RK28CaptureTarget* saved = (RK28CaptureTarget*)calloc(1, sizeof(RK28CaptureTarget));
    if (!saved) return NULL;
    saved->context = c;
    saved->color = c->rasterizer.state.buffers.color.s;
    saved->read = c->rasterizer.state.buffers.read.s;
    saved->viewport = c->viewport;
    saved->scissorEnabled = c->rasterizer.state.enables & GGL_ENABLE_SCISSOR_TEST;
    GGLSurface& target = saved->target;
    target.version = sizeof(GGLSurface);
    target.width = width; target.height = height; target.stride = width;
    target.format = GGL_PIXEL_FORMAT_RGBA_8888;
    target.data = (GGLubyte*)calloc((size_t)width * height, 4);
    if (!target.data) { free(saved); return NULL; }
    // No EGL surface disconnect, native-window dequeue, queue or page flip.
    // Keep all layer textures and the current EGL window/context untouched.
    c->rasterizer.procs.colorBuffer(c, &target);
    c->rasterizer.procs.readBuffer(c, &target);
    return saved;
}

extern "C" __attribute__((visibility("default")))
void rk28_end_capture(void* handle) {
    RK28CaptureTarget* saved = (RK28CaptureTarget*)handle;
    if (!saved) return;
    ogles_context_t* c = saved->context;
    c->rasterizer.procs.colorBuffer(c, &saved->color);
    c->rasterizer.procs.readBuffer(c, &saved->read);
    // libagl does not implement glGetIntegerv(GL_VIEWPORT/GL_SCISSOR_BOX).
    // Restore from the context itself, after restoring the original height.
    const viewport_t& v = saved->viewport;
    ogles_viewport(c, v.x, v.y, v.w, v.h);
    ogles_scissor(c, v.scissor.x, v.scissor.y, v.scissor.w, v.scissor.h);
    c->rasterizer.procs.enableDisable(c, GGL_SCISSOR_TEST, saved->scissorEnabled);
    c->rasterizer.state.clear.dirty |= GGL_COLOR_BUFFER_BIT;
    free(saved->target.data); free(saved);
}

} // namespace android
