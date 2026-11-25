#pragma once

#include <arisa/async/task.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/point.hpp>
#include <arisa/composition/rect.hpp>
#include <arisa/composition/size.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/graphics/path.hpp>
#include <arisa/graphics/shape_style.hpp>
#include <arisa/motion/animation.hpp>
#include <arisa/provider/gpu/image.hpp>
#include <arisa/text/font_face.hpp>

#include <array>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string_view>
#include <vector>

namespace arisa::examples {

class LaunchScreen final : public composition::Component {
 public:
  enum class Ending : std::uint8_t {
    fade,
    erase,
  };

  enum class Ink : std::uint8_t {
    white,
    gradient,
    glass,
  };

  struct Style {
    Ending ending{Ending::erase};
    Ink ink{Ink::white};

    friend constexpr auto operator==(const Style&, const Style&)
        -> bool = default;
  };

  struct Knot {
    float x{0.0F};
    float y{0.0F};
    bool corner{false};
    bool lift{false};
  };

  using Finished = std::shared_ptr<const std::function<void()>>;
  using Font = std::shared_ptr<const text::FontFace>;
  using Photo = std::shared_ptr<const provider::Image>;

  static constexpr composition::StateKey<bool> written{"launch-written", false};
  static constexpr composition::StateKey<bool> dotted{"launch-dotted", false};
  static constexpr composition::StateKey<bool> undotted{"launch-undotted",
                                                        false};
  static constexpr double dot_duration = 0.18;
  static constexpr composition::StateKey<bool> erased{"launch-erased", false};
  static constexpr composition::StateKey<bool> faded{"launch-faded", false};
  static constexpr composition::StateKey<bool> drifted{"launch-drifted", false};
  static constexpr composition::StateKey<bool> skipped{"launch-skipped", false};

  static constexpr std::string_view stroke_view = "launch-stroke";
  static constexpr std::string_view root_view = "launch-screen";
  static constexpr double write_start = 0.21;
  static constexpr double write_end = 2.50;
  static constexpr double hold_end = 4.16;
  static constexpr double erase_end = 5.75;
  static constexpr double fade_duration = 0.6;
  static constexpr double skip_duration = 0.25;
  static constexpr double drift_period = 9.0;
  static constexpr double drift_frame_rate = 15.0;
  static constexpr double pan_period = 24.0;
  static constexpr double pan_frame_rate = 10.0;
  static constexpr float pan_rest_scale = 1.04F;
  static constexpr float pan_scale = 1.12F;
  static constexpr float pan_shift = 18.0F;
  static constexpr float glow_opacity = 0.34F;
  static constexpr float glow_spread = 1.5F;
  static constexpr float resting_width = 19.0F;
  static constexpr float glass_width = 26.0F;
  static constexpr float unit = 1.05F;
  static constexpr float slant_degrees = 12.0F;
  static constexpr float margin = 24.0F;
  static constexpr float type_size = 230.0F;
  static constexpr float reveal_reach = 22.0F;
  static constexpr float dot_ceiling = 0.3F;
  static constexpr composition::Rect guide_frame{
      .x = 24.0F, .y = 24.0F, .width = 579.0F, .height = 152.0F};
  static constexpr std::array<composition::Point, 51> guide_points{
      {{122.0F, 89.0F},  {92.0F, 69.0F},   {57.0F, 74.0F},   {34.0F, 104.0F},
       {30.0F, 139.0F},  {47.0F, 164.0F},  {77.0F, 161.0F},  {102.0F, 134.0F},
       {119.0F, 94.0F},  {114.0F, 139.0F}, {124.0F, 164.0F}, {147.0F, 167.0F},
       {162.0F, 139.0F}, {172.0F, 89.0F},  {175.0F, 44.0F},  {169.0F, 37.0F},
       {187.0F, 59.0F},  {212.0F, 69.0F},  {225.0F, 94.0F},  {227.0F, 134.0F},
       {232.0F, 164.0F}, {257.0F, 167.0F}, {277.0F, 139.0F}, {287.0F, 99.0F},
       {297.0F, 79.0F},  {292.0F, 129.0F}, {297.0F, 164.0F}, {322.0F, 167.0F},
       {347.0F, 149.0F}, {372.0F, 119.0F}, {390.0F, 94.0F},  {400.0F, 82.0F},
       {384.0F, 97.0F},  {388.0F, 117.0F}, {410.0F, 135.0F}, {420.0F, 153.0F},
       {412.0F, 169.0F}, {432.0F, 171.0F}, {462.0F, 164.0F}, {542.0F, 89.0F},
       {512.0F, 69.0F},  {477.0F, 79.0F},  {462.0F, 114.0F}, {467.0F, 154.0F},
       {492.0F, 169.0F}, {522.0F, 154.0F}, {540.0F, 109.0F}, {537.0F, 149.0F},
       {550.0F, 171.0F}, {577.0F, 159.0F}, {597.0F, 124.0F}}};
  static constexpr std::string_view word_text = "arisa";
  static constexpr float grain = 0.035F;
  static constexpr std::array<double, 4> write_curve{0.30, 0.14, 0.67, 1.0};
  static constexpr std::array<double, 4> erase_curve{0.48, 0.0, 0.73, 1.0};
  static constexpr std::uint32_t blue = 0x0AB9E6;
  static constexpr std::uint32_t red = 0xFF3C28;
  static constexpr std::array<std::uint32_t, 6> wallpaper{
      0x7DB8E6, 0x2482C0, 0xD9406E, 0xE07468, 0x8A3A78, 0x1F4570};

  LaunchScreen(Style style, Finished finished, Font font = nullptr,
               Photo photo = nullptr) noexcept
      : style_(style),
        finished_(std::move(finished)),
        font_(std::move(font)),
        photo_(std::move(photo)) {}

  auto operator==(const LaunchScreen& other) const -> bool {
    return style_ == other.style_ && finished_ == other.finished_ &&
           font_ == other.font_ && photo_ == other.photo_;
  }

  auto body(composition::Context& context) const
      -> composition::Element override;

  static auto lettering(bool with_dot = true) -> graphics::Path;

  static auto lettering_size() -> composition::Size;

  static auto mesh_points(bool drifting) -> std::vector<composition::Point>;

  static auto write_animation() -> motion::Animation;

  static auto erase_animation() -> motion::Animation;

 private:
  static auto knots() -> std::vector<Knot>;

  static auto placed() -> std::vector<Knot>;

  static auto handles(composition::Point before, composition::Point from,
                      composition::Point to, composition::Point after)
      -> std::array<composition::Point, 2>;

  struct Beats {
    composition::StateHandle<bool> write;
    composition::StateHandle<bool> drift;
    composition::StateHandle<bool> erase;
    composition::StateHandle<bool> fade;
    composition::StateHandle<bool> skip;
    composition::StateHandle<bool> dot;
    composition::StateHandle<bool> undot;
  };

  static auto timeline(Ending ending, Beats beats, Finished finished,
                       bool panning) -> async::Task<void>;

  static auto seconds(double value) -> std::chrono::nanoseconds;

  static void append(graphics::Path& into,
                     const graphics::Path::Element& element);

  static auto skip(composition::StateHandle<bool> skip,
                   composition::StateHandle<bool> fade, Finished finished)
      -> void;

  static auto wallpaper_view(bool drift) -> composition::Element;

  static auto photo_view(const Photo& photo, bool drift)
      -> composition::Element;

  static auto glow() -> composition::Element;

  static auto word(Ink ink, bool write, bool erase) -> composition::Element;

  struct Reveal {
    bool write{false};
    bool erase{false};
    bool dot{false};
    bool undot{false};
  };

  struct Parts {
    graphics::Path body;
    graphics::Path dot;
  };

  static auto typeset(const text::FontFace& font, Ink ink, Reveal reveal)
      -> std::optional<composition::Element>;

  static auto separated(const graphics::Path& path) -> Parts;

  static auto guide() -> graphics::Path;

  static auto fitted(const graphics::Path& path, composition::Rect area)
      -> graphics::Path;

  static auto ink_style(Ink ink, bool along) -> graphics::ShapeStyle;

  Style style_;
  Finished finished_;
  Font font_;
  Photo photo_;
};

}
