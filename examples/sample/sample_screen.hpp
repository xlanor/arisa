#pragma once

#include <sample_model.hpp>
#include <sample_renderer.hpp>

#include <arisa/async/task.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>

#include <functional>
#include <string_view>

namespace arisa::examples {

class SampleScreen final : public composition::Component {
 public:
  static constexpr std::string_view sidebar = "sample.sidebar";
  static constexpr std::string_view play_button = "sample.play";
  static constexpr std::string_view full_screen_button = "sample.full-screen";
  static constexpr std::string_view list = "sample.list";
  static constexpr std::string_view card_caption = "sample.card-caption";
  static constexpr std::string_view picture_caption = "sample.picture-caption";
  static constexpr float sidebar_width = 280.0F;
  static constexpr float sidebar_radius = 26.0F;
  static constexpr float card_width = 360.0F;
  static constexpr float card_height = 200.0F;
  static constexpr float card_radius = 22.0F;
  static constexpr float list_width = 400.0F;
  static constexpr float list_tail = 600.0F;

  explicit SampleScreen(SampleModel& model) noexcept : model_(model) {}

  auto operator==(const SampleScreen& other) const -> bool {
    return &model_.get() == &other.model_.get();
  }

  auto body(composition::Context& context) const
      -> composition::Element override;

 private:
  static auto card(composition::Element content, std::string_view label,
                   std::string_view key) -> composition::Element;

  static auto repaint(std::reference_wrapper<SampleModel> model, bool playing)
      -> async::Task<void>;

  class Picture;

  std::reference_wrapper<SampleModel> model_;
};

class SampleScreen::Picture final : public composition::Component {
 public:
  explicit Picture(const SampleModel& model) noexcept : model_(model) {}

  auto operator==(const Picture& other) const -> bool {
    return &model_.get() == &other.model_.get();
  }

  auto body(composition::Context& context) const
      -> composition::Element override;

 private:
  std::reference_wrapper<const SampleModel> model_;
};

}
