#include <sample_model.hpp>

namespace arisa::examples {

SampleModel::SampleModel(Animation& animation, SampleRenderer& renderer)
    : animation_(animation), renderer_(renderer), picture_(renderer.picture()) {
  play(animation.playing());
}

void SampleModel::play(bool playing) {
  animation_.get().play(playing);
  const provider::FrameDrive drive = playing ? provider::FrameDrive::continuous
                                             : provider::FrameDrive::on_demand;
  const SampleRenderer& renderer = renderer_;
  renderer.backdrop().set_drive(drive);
  renderer.card().set_drive(drive);
  renderer.backdrop().request_frame();
  renderer.card().request_frame();
  playing_.set(playing);
}

void SampleModel::set_full_screen(bool full_screen) {
  full_screen_.set(full_screen);
}

void SampleModel::repaint_picture() {
  if (const auto shown = renderer_.get().repaint_picture(); shown.has_value()) {
    picture_.set(*shown);
  }
}

}
