#pragma once

#include <gtest/gtest.h>

#include <cstdlib>

namespace arisa::provider::sdl3 {

class Sdl3Test : public ::testing::Test {
 protected:
  static void SetUpTestSuite() {
    setenv("SDL_VIDEODRIVER", "offscreen", 1);
    setenv("SDL_VIDEO_DRIVER", "offscreen", 1);
    setenv("SDL_AUDIO_DRIVER", "dummy", 1);
#if defined(ARISA_VULKAN_LOADER)
    setenv("SDL_VULKAN_LIBRARY", ARISA_VULKAN_LOADER, 0);
#endif
  }
};

}
