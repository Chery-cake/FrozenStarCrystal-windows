module;

export module windows;

import :check;

#if defined(WINDOW_BACKEND_SDL)
export import windows.sdl;

#else
#error "No graphics backend selected"
#endif

export namespace windows {

#if defined(GRAPHICS_BACKEND_VULKAN)
using namespace sdl;
using WindowsBackend = void *;
#else
#error "No graphics backend selected"
#endif

static_assert(
    ApiCheck<int>,
    "The selected backend doesn't support all the minimun requirements");
} // namespace windows
