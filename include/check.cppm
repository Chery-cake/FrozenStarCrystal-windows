module;

export module windows:check;

import std.compat;

#if defined(WINDOW_BACKEND_SDL)
// import graphics.sdl;

#else
#error "No graphics backend selected"
#endif

export namespace windows {

template <typename T>
concept ApiCheck = requires(T &api) {
  // {api.initialize()} -> std::same_as<void>;
  std::not_equal_to<void>();
};

} // namespace windows
