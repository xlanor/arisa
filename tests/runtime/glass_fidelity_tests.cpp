#include <arisa/components/environment.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/shape_view.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/graphics/path.hpp>
#include <arisa/graphics/stroke_style.hpp>
#include <arisa/motion/angle.hpp>
#include <arisa/provider/gpu/image.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/surface/surface_scale.hpp>
#include <arisa/provider/surface/surface_size.hpp>
#include <arisa/theme/color_scheme.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/glass.hpp>
#include <arisa/theme/shape.hpp>

#include <focus_fixture.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <fstream>
#include <map>
#include <memory>
#include <numbers>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace arisa::runtime {

using composition::Element;

class GlassFidelityTest
    : public FocusFixture,
      public ::testing::WithParamInterface<
          std::tuple<std::string_view, std::string_view, theme::ColorScheme>> {
 protected:
  enum class Shape : std::uint8_t { capsule, circle, rounded, arc };

  enum class Backdrop : std::uint8_t {
    ramp_x,
    ramp_y,
    grey,
    black,
    stripes,
    stripes_wide
  };

  struct Cell {
    std::string_view name;
    Shape shape;
    composition::Rect rect;
    float radius;
  };

  struct Axis {
    std::string_view side;
    composition::Point start;
    composition::Point direction;
    float length;
  };

  class Shot {
   public:
    Shot(provider::Image image, composition::Point origin, float scale)
        : image_(std::move(image)), origin_(origin), scale_(scale) {}

    auto colour(composition::Point point) const -> std::array<float, 3> {
      const float x = ((point.x - origin_.x) * scale_) - 0.5F;
      const float y = ((point.y - origin_.y) * scale_) - 0.5F;
      const float fx = std::floor(x);
      const float fy = std::floor(y);
      const float ax = x - fx;
      const float ay = y - fy;
      std::array<float, 3> sum{};
      for (const auto& [dx, dy, weight] :
           {std::tuple{0.0F, 0.0F, (1.0F - ax) * (1.0F - ay)},
            std::tuple{1.0F, 0.0F, ax * (1.0F - ay)},
            std::tuple{0.0F, 1.0F, (1.0F - ax) * ay},
            std::tuple{1.0F, 1.0F, ax * ay}}) {
        const auto px = static_cast<std::uint32_t>(
            std::clamp(fx + dx, 0.0F, static_cast<float>(image_.width() - 1)));
        const auto py = static_cast<std::uint32_t>(
            std::clamp(fy + dy, 0.0F, static_cast<float>(image_.height() - 1)));
        const auto found = image_.pixel(px, py);
        if (!found.has_value()) {
          continue;
        }
        for (std::size_t channel = 0; channel < 3; ++channel) {
          sum[channel] +=
              weight * static_cast<float>((*found)[channel]) / 255.0F;
        }
      }
      return sum;
    }

    auto luma(composition::Point point) const -> float {
      const std::array<float, 3> c = colour(point);
      return (c[0] * 0.2126F) + (c[1] * 0.7152F) + (c[2] * 0.0722F);
    }

    auto scale() const noexcept -> float { return scale_; }

   private:
    provider::Image image_;
    composition::Point origin_;
    float scale_;
  };

  static constexpr float scale = 1.25F;
  static constexpr float ramp_margin = 16.0F;
  static constexpr float pad = 12.0F;
  static constexpr float grey = 0.5F;
  static constexpr float stripe_period = 4.0F;
  static constexpr float wide_stripe_period = 16.0F;
  static constexpr composition::Point arc_centre{201.0F, 534.0F};
  static constexpr float arc_radius = 200.0F;
  static constexpr float arc_width = 44.0F;
  static constexpr double arc_start = 215.0;
  static constexpr double arc_end = 325.0;
  static constexpr float soft_tint = 0.3F;
  static constexpr float saturated_frost = 8.0F;
  static constexpr float sharp_frost = 3.0F;
  static constexpr float clear_frost_tolerance = 1.0F;
  static constexpr float clear_veil_tolerance = 0.02F;
  static constexpr float quantised_contrast = 0.15F;
  static constexpr std::array<std::string_view, 2> unmatched{
      "regular.pill.shift_left", "tinted_soft.pill.shift_left"};
  static constexpr std::array<float, 6> depths{1.0F, 2.0F,  4.0F,
                                               8.0F, 12.0F, 16.0F};
  static constexpr std::array<float, 3> rim_depths{1.0F, 2.0F, 4.0F};

  static constexpr std::array<Cell, 6> cells{
      Cell{"capsule", Shape::capsule, {24.0F, 48.0F, 200.0F, 56.0F}, 28.0F},
      Cell{"circle", Shape::circle, {266.0F, 36.0F, 88.0F, 88.0F}, 44.0F},
      Cell{"pill", Shape::capsule, {250.0F, 160.0F, 120.0F, 28.0F}, 14.0F},
      Cell{"rounded", Shape::rounded, {24.0F, 140.0F, 200.0F, 140.0F}, 28.0F},
      Cell{"arc", Shape::arc, {15.0F, 312.0F, 372.0F, 130.0F}, 22.0F},
      Cell{"panel", Shape::rounded, {24.0F, 500.0F, 354.0F, 340.0F}, 34.0F},
  };

  using Reference =
      std::map<std::string, std::array<std::string, 2>, std::less<>>;

  static auto reference() -> const Reference& {
    static const Reference loaded = [] {
      Reference table;
      std::ifstream stream{ARISA_SWIFTUI_GLASS};
      std::string line;
      while (std::getline(stream, line)) {
        if (line.empty() || line.starts_with('#') || line.starts_with("key")) {
          continue;
        }
        std::istringstream fields{line};
        std::string key;
        std::string light;
        std::string dark;
        std::getline(fields, key, '\t');
        std::getline(fields, light, '\t');
        std::getline(fields, dark, '\t');
        table.emplace(key, std::array<std::string, 2>{light, dark});
      }
      return table;
    }();
    return loaded;
  }

  static auto apple(std::string_view key, theme::ColorScheme scheme)
      -> std::optional<float> {
    const auto found = reference().find(key);
    if (found == reference().end()) {
      return std::nullopt;
    }
    const std::string& text =
        found->second[scheme == theme::ColorScheme::dark ? 1 : 0];
    if (text.empty() || text == "nan") {
      return std::nullopt;
    }
    return std::stof(text);
  }

  static auto apple_colour(std::string_view key, theme::ColorScheme scheme)
      -> std::optional<std::array<float, 3>> {
    const auto found = reference().find(key);
    if (found == reference().end()) {
      return std::nullopt;
    }
    const std::string& text =
        found->second[scheme == theme::ColorScheme::dark ? 1 : 0];
    if (text.size() < 7 || text[0] != '#') {
      return std::nullopt;
    }
    const unsigned long value = std::stoul(text.substr(1, 6), nullptr, 16);
    return std::array<float, 3>{
        static_cast<float>((value >> 16U) & 0xFFU) / 255.0F,
        static_cast<float>((value >> 8U) & 0xFFU) / 255.0F,
        static_cast<float>(value & 0xFFU) / 255.0F};
  }

  static auto glass_named(std::string_view name) -> theme::Glass {
    if (name == "clear") {
      return theme::Glass::clear();
    }
    if (name == "tinted") {
      return theme::Glass::regular().tint(theme::ColourRole::accent);
    }
    if (name == "tinted_soft") {
      return theme::Glass::regular().tint(
          theme::ColourValue{theme::ColourRole::accent}.opacity(soft_tint));
    }
    return theme::Glass::regular();
  }

  static auto cell_named(std::string_view name) -> const Cell& {
    return *std::ranges::find(cells, name, &Cell::name);
  }

  static auto ramp(const Cell& cell) -> composition::Rect {
    return {cell.rect.x - ramp_margin, cell.rect.y - ramp_margin,
            cell.rect.width + (2.0F * ramp_margin),
            cell.rect.height + (2.0F * ramp_margin)};
  }

  static auto origin(const Cell& cell) -> composition::Point {
    const composition::Rect area = ramp(cell);
    return {area.x - pad, area.y - pad};
  }

  static auto canvas(const Cell& cell) -> composition::Size {
    const composition::Rect area = ramp(cell);
    return {area.width + (2.0F * pad), area.height + (2.0F * pad)};
  }

  static auto pattern(Backdrop backdrop, const Cell& cell,
                      composition::Point point) -> std::uint8_t {
    const auto level = [](float span) {
      return static_cast<std::uint8_t>(
          std::clamp(std::round(span * 255.0F), 0.0F, 255.0F));
    };
    switch (backdrop) {
      case Backdrop::ramp_x:
      case Backdrop::ramp_y: {
        const composition::Rect area = ramp(cell);
        if (!area.contains(point.x, point.y)) {
          return static_cast<std::uint8_t>(grey * 255.0F);
        }
        return backdrop == Backdrop::ramp_x
                   ? level((point.x - area.x) / area.width)
                   : level((point.y - area.y) / area.height);
      }
      case Backdrop::black:
        return 0;
      case Backdrop::stripes:
      case Backdrop::stripes_wide: {
        const float period =
            backdrop == Backdrop::stripes ? stripe_period : wide_stripe_period;
        const float phase = std::fmod(point.x, period);
        return phase < period * 0.5F ? std::uint8_t{0} : std::uint8_t{255};
      }
      case Backdrop::grey:
        break;
    }
    return static_cast<std::uint8_t>(grey * 255.0F);
  }

  static auto backdrop_image(Backdrop backdrop, const Cell& cell)
      -> std::shared_ptr<const provider::Image> {
    const composition::Size size = canvas(cell);
    const auto width = static_cast<std::uint32_t>(size.width * 3.0F);
    const auto height = static_cast<std::uint32_t>(size.height * 3.0F);
    const composition::Point corner = origin(cell);
    std::vector<std::byte> bytes(static_cast<std::size_t>(width) * height * 4);
    for (std::uint32_t y = 0; y < height; ++y) {
      for (std::uint32_t x = 0; x < width; ++x) {
        const composition::Point point{
            corner.x + ((static_cast<float>(x) + 0.5F) / 3.0F),
            corner.y + ((static_cast<float>(y) + 0.5F) / 3.0F)};
        const std::uint8_t value = pattern(backdrop, cell, point);
        const std::size_t at = ((static_cast<std::size_t>(y) * width) + x) * 4;
        bytes[at] = std::byte{value};
        bytes[at + 1] = std::byte{value};
        bytes[at + 2] = std::byte{value};
        bytes[at + 3] = std::byte{255};
      }
    }
    auto made = provider::Image::create(width, height, std::move(bytes));
    EXPECT_TRUE(made.has_value());
    return std::make_shared<const provider::Image>(std::move(*made));
  }

  static auto lens(const Cell& cell, theme::Glass glass) -> Element {
    const composition::Point corner = origin(cell);
    const composition::Size size = canvas(cell);
    if (cell.shape == Shape::arc) {
      graphics::Path path;
      path.add_arc({arc_centre.x - corner.x, arc_centre.y - corner.y},
                   arc_radius, motion::Angle::degrees(arc_start),
                   motion::Angle::degrees(arc_end), false);
      return Element{components::ShapeView(std::move(path))
                         .stroke(theme::Colour::hex(0xFFFFFF),
                                 graphics::StrokeStyle{
                                     .line_width = arc_width,
                                     .line_cap = graphics::LineCap::round,
                                     .line_join = graphics::LineJoin::round})
                         .glass_effect(glass)}
          .frame({.width = size.width, .height = size.height});
    }
    const theme::Shape shape =
        cell.shape == Shape::circle    ? theme::Shape{theme::Circle{}}
        : cell.shape == Shape::capsule ? theme::Shape::capsule()
                                       : theme::Shape::rounded(cell.radius);
    return Element{components::ZStack()}
        .frame({.width = cell.rect.width, .height = cell.rect.height})
        .glass_effect(glass, shape)
        .offset(cell.rect.x - corner.x, cell.rect.y - corner.y);
  }

  static auto render(const Cell& cell, theme::Glass glass,
                     theme::ColorScheme scheme, Backdrop backdrop) -> Shot {
    const composition::Size size = canvas(cell);
    provider::headless::HeadlessProviderFactory providers;
    const provider::SurfaceSize extent{
        .width = provider::SurfaceWidth{static_cast<std::uint32_t>(
            std::ceil(size.width * scale))},
        .height = provider::SurfaceHeight{
            static_cast<std::uint32_t>(std::ceil(size.height * scale))}};
    providers.surface().resize(extent);
    providers.surface().set_scale(*provider::SurfaceScale::create(scale));
    providers.gpu().rasterize(extent);
    Application application = start(
        providers,
        Element{components::ZStack(
                    components::ZStackOptions{
                        .alignment = components::Alignment::top_leading},
                    Element{components::Image::Raster(
                                backdrop_image(backdrop, cell))}
                        .frame({.width = size.width, .height = size.height}),
                    lens(cell, glass))}
            .frame({.width = size.width,
                    .height = size.height,
                    .alignment = components::Alignment::top_leading})
            .environment(components::Environment::colour_scheme, scheme));
    frames(application, 3);
    auto image = providers.gpu().capture();
    EXPECT_TRUE(image.has_value());
    return Shot{std::move(*image), origin(cell), scale};
  }

  static auto axes(const Cell& cell) -> std::vector<Axis> {
    const composition::Rect& r = cell.rect;
    if (cell.shape == Shape::arc) {
      const double middle =
          (arc_start + arc_end) * 0.5 * std::numbers::pi / 180.0;
      const composition::Point outer{
          arc_centre.x + ((arc_radius + (arc_width * 0.5F)) *
                          static_cast<float>(std::cos(middle))),
          arc_centre.y + ((arc_radius + (arc_width * 0.5F)) *
                          static_cast<float>(std::sin(middle)))};
      return {Axis{"top", outer, {0.0F, 1.0F}, arc_width}};
    }
    const float mx = r.x + (r.width * 0.5F);
    const float my = r.y + (r.height * 0.5F);
    return {Axis{"left", {r.x, my}, {1.0F, 0.0F}, r.width},
            Axis{"top", {mx, r.y}, {0.0F, 1.0F}, r.height},
            Axis{"bottom", {mx, r.y + r.height}, {0.0F, -1.0F}, r.height}};
  }

  static auto centre(const Cell& cell) -> composition::Point {
    if (cell.shape == Shape::arc) {
      const Axis axis = axes(cell).front();
      return {axis.start.x, axis.start.y + (arc_width * 0.5F)};
    }
    return {cell.rect.x + (cell.rect.width * 0.5F),
            cell.rect.y + (cell.rect.height * 0.5F)};
  }

  static auto at(const Axis& axis, float depth) -> composition::Point {
    return {axis.start.x + (axis.direction.x * depth),
            axis.start.y + (axis.direction.y * depth)};
  }

  static auto key(std::string_view variant, const Cell& cell,
                  std::string_view metric) -> std::string {
    return std::format("glass.{}.{}.{}", variant, cell.name, metric);
  }

  static auto depth_label(float depth) -> std::string {
    return depth == std::floor(depth)
               ? std::format("{}", static_cast<int>(depth))
               : std::format("{}", depth);
  }
};

TEST_P(GlassFidelityTest, MatchesTheAppleHarness) {
  const auto [variant, shape, scheme] = GetParam();
  const Cell& cell = cell_named(shape);
  const theme::Glass glass = glass_named(variant);
  const Shot ramp_x = render(cell, glass, scheme, Backdrop::ramp_x);
  const Shot ramp_y = render(cell, glass, scheme, Backdrop::ramp_y);
  const Shot flat = render(cell, glass, scheme, Backdrop::grey);
  const Shot black = render(cell, glass, scheme, Backdrop::black);
  const Shot stripes = render(cell, glass, scheme, Backdrop::stripes);
  const Shot wide = render(cell, glass, scheme, Backdrop::stripes_wide);
  const composition::Rect area = ramp(cell);
  const auto decode = [&](const Shot& shot, composition::Point point,
                          bool horizontal) -> std::optional<float> {
    const float low = black.luma(point);
    const float gain = (flat.luma(point) - low) / grey;
    if (gain < 0.05F) {
      return std::nullopt;
    }
    const float t = (shot.luma(point) - low) / gain;
    return horizontal ? area.x + (t * area.width) : area.y + (t * area.height);
  };
  const bool opaque = variant == "tinted";
  const bool dark = scheme == theme::ColorScheme::dark;
  const composition::Point middle = centre(cell);
  const float veil = flat.luma(middle) - grey;
  if (const auto expected = apple(key(variant, cell, "veil_luma"), scheme)) {
    EXPECT_NEAR(veil, *expected,
                variant == "clear" ? clear_veil_tolerance : 0.06F)
        << key(variant, cell, "veil_luma");
  }
  if (const auto expected = apple_colour(key(variant, cell, "veil"), scheme)) {
    const std::array<float, 3> seen = flat.colour(middle);
    for (std::size_t channel = 0; channel < 3; ++channel) {
      EXPECT_NEAR(seen[channel], (*expected)[channel], 0.09F)
          << key(variant, cell, "veil") << " channel " << channel;
    }
  }
  for (const Axis& axis : axes(cell)) {
    const bool horizontal = axis.direction.x != 0.0F;
    const bool frosted =
        apple(key(variant, cell, "frost_sigma"), scheme).value_or(0.0F) >
        sharp_frost;
    const bool readable =
        axis.side != "bottom" &&
        !std::ranges::contains(unmatched, std::format("{}.{}.shift_{}", variant,
                                                      cell.name, axis.side));
    if (!opaque && !frosted && readable && !(dark && variant != "clear")) {
      for (const float depth : depths) {
        if (depth >= axis.length * 0.5F) {
          continue;
        }
        const std::string name =
            key(variant, cell,
                std::format("shift_{}.{}", axis.side, depth_label(depth)));
        const auto expected = apple(name, scheme);
        const composition::Point point = at(axis, depth);
        const auto source =
            decode(horizontal ? ramp_x : ramp_y, point, horizontal);
        if (!expected.has_value() || !source.has_value()) {
          continue;
        }
        const float inward = horizontal ? axis.direction.x : axis.direction.y;
        const float shift =
            (*source - (horizontal ? point.x : point.y)) * inward;
        const float tolerance = std::max(4.0F, std::abs(*expected) * 0.35F);
        EXPECT_NEAR(shift, *expected, tolerance) << name;
      }
    }
    for (const float depth : rim_depths) {
      const std::string name =
          key(variant, cell,
              std::format("rim_{}.{}", axis.side, depth_label(depth)));
      const auto expected = apple(name, scheme);
      const auto interior = apple(key(variant, cell, "veil_luma"), scheme);
      if (!expected.has_value() || !interior.has_value()) {
        continue;
      }
      const float excess = flat.luma(at(axis, depth)) - grey - veil;
      EXPECT_NEAR(excess, *expected - *interior, 0.07F) << name;
    }
  }
  if (!opaque) {
    const float window = cell.shape == Shape::arc
                             ? 8.0F
                             : std::min(16.0F, cell.rect.width / 4.0F);
    const auto contrast = [&](const Shot& shot) {
      float low = 1.0F;
      float high = 0.0F;
      for (float x = middle.x - window; x <= middle.x + window;
           x += 1.0F / shot.scale()) {
        const float value = shot.luma({x, middle.y});
        low = std::min(low, value);
        high = std::max(high, value);
      }
      const float alpha =
          1.0F - ((flat.luma(middle) - black.luma(middle)) / grey);
      return (high - low) / std::max(1.0F - alpha, 0.05F);
    };
    const float fine = contrast(stripes);
    const float coarse = contrast(wide);
    const bool use_wide = fine < quantised_contrast;
    const float measured = std::min(use_wide ? coarse : fine, 0.999F);
    const float period = use_wide ? wide_stripe_period : stripe_period;
    const float sigma =
        measured >= 0.98F
            ? 0.0F
            : period / (std::numbers::pi_v<float> *
                        std::numbers::sqrt2_v<
                            float>)*std::sqrt(-std::log(std::max(measured,
                                                                 1.0e-4F)));
    if (const auto expected =
            apple(key(variant, cell, "frost_sigma"), scheme)) {
      if (*expected >= saturated_frost) {
        EXPECT_GE(sigma, saturated_frost - 2.0F)
            << key(variant, cell, "frost_sigma");
      } else {
        const float tolerance = variant == "clear"
                                    ? clear_frost_tolerance
                                    : std::max(2.0F, *expected * 0.4F);
        EXPECT_NEAR(sigma, *expected, tolerance)
            << key(variant, cell, "frost_sigma");
      }
    }
  }
  if (cell.shape != Shape::arc) {
    const Axis bottom = axes(cell).back();
    for (const float depth : {2.0F, 8.0F}) {
      const std::string name = key(
          variant, cell, std::format("shadow_bottom.{}", depth_label(depth)));
      if (const auto expected = apple(name, scheme)) {
        EXPECT_NEAR(flat.luma(at(bottom, -depth)) - grey, *expected, 0.03F)
            << name;
      }
    }
  }
}

INSTANTIATE_TEST_SUITE_P(
    Harness, GlassFidelityTest,
    ::testing::Combine(
        ::testing::Values(std::string_view{"regular"},
                          std::string_view{"clear"}, std::string_view{"tinted"},
                          std::string_view{"tinted_soft"}),
        ::testing::Values(std::string_view{"pill"}, std::string_view{"capsule"},
                          std::string_view{"circle"},
                          std::string_view{"rounded"},
                          std::string_view{"panel"}, std::string_view{"arc"}),
        ::testing::Values(theme::ColorScheme::light, theme::ColorScheme::dark)),
    [](const auto& named) {
      return std::format(
          "{}_{}_{}", std::get<0>(named.param), std::get<1>(named.param),
          std::get<2>(named.param) == theme::ColorScheme::dark ? "dark"
                                                               : "light");
    });

}
