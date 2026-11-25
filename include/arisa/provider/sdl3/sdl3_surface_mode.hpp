#pragma once

#include <arisa/provider/surface/surface_size.hpp>

#include <string>
#include <variant>

namespace arisa::provider::sdl3 {

struct NoSurface {};

struct WindowMode {
  std::string title;
  SurfaceSize size;
};

struct OffscreenMode {
  SurfaceSize size;
};

using SurfaceMode = std::variant<NoSurface, WindowMode, OffscreenMode>;

}
