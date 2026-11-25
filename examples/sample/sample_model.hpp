#pragma once

#include <animation.hpp>
#include <sample_renderer.hpp>

#include <arisa/composition/observable.hpp>
#include <arisa/provider/gpu/frame_hook.hpp>
#include <arisa/provider/gpu/texture.hpp>

#include <chrono>
#include <functional>

namespace arisa::examples {

class SampleModel {
 public:
  static constexpr std::chrono::milliseconds picture_interval{100};

  SampleModel(Animation& animation, SampleRenderer& renderer);

  auto renderer() const noexcept -> SampleRenderer& { return renderer_; }

  auto playing() const noexcept -> const composition::Observable<bool>& {
    return playing_;
  }

  auto full_screen() const noexcept -> const composition::Observable<bool>& {
    return full_screen_;
  }

  auto picture() const noexcept
      -> const composition::Observable<provider::TextureId>& {
    return picture_;
  }

  void play(bool playing);

  void set_full_screen(bool full_screen);

  void repaint_picture();

 private:
  std::reference_wrapper<Animation> animation_;
  std::reference_wrapper<SampleRenderer> renderer_;
  composition::Observable<bool> playing_{true};
  composition::Observable<bool> full_screen_{false};
  composition::Observable<provider::TextureId> picture_;
};

}
