#pragma once

#include <SDL3/SDL_stdinc.h>

#include <memory>

namespace arisa::provider::sdl3 {

struct Sdl3Free {
  void operator()(void* memory) const noexcept { SDL_free(memory); }
};

template <class T>
using Sdl3Owned = std::unique_ptr<T, Sdl3Free>;

}
