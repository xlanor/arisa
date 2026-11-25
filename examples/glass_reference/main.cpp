#include <arisa/components/button.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/shape_view.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/graphics/path.hpp>
#include <arisa/graphics/stroke_style.hpp>
#include <arisa/motion/angle.hpp>
#include <arisa/provider/gpu/image.hpp>
#include <arisa/runtime/application.hpp>
#include <arisa/runtime/application_options.hpp>
#include <arisa/runtime/root.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/glass.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>
#include <desktop_example.hpp>
#include <example_options.hpp>
#include <gallery_fonts.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace arisa::examples {

class GlassReference final : public composition::Component {
 public:
  enum class Shape : std::uint8_t { capsule, circle, rounded, arc };

  struct Cell {
    Shape shape{Shape::capsule};
    composition::Rect rect;
    float radius{0.0F};
  };

  static constexpr float width = 402.0F;
  static constexpr float height = 874.0F;
  static constexpr composition::Point arc_centre{201.0F, 534.0F};
  static constexpr float arc_radius = 200.0F;
  static constexpr float arc_width = 44.0F;
  static constexpr double arc_start = 215.0;
  static constexpr double arc_end = 325.0;
  static constexpr float soft_tint = 0.3F;

  static constexpr std::array<Cell, 6> cells{
      Cell{Shape::capsule, {24.0F, 48.0F, 200.0F, 56.0F}, 28.0F},
      Cell{Shape::circle, {266.0F, 36.0F, 88.0F, 88.0F}, 44.0F},
      Cell{Shape::capsule, {250.0F, 160.0F, 120.0F, 28.0F}, 14.0F},
      Cell{Shape::rounded, {24.0F, 140.0F, 200.0F, 140.0F}, 28.0F},
      Cell{Shape::arc, {15.0F, 312.0F, 372.0F, 130.0F}, 22.0F},
      Cell{Shape::rounded, {24.0F, 500.0F, 354.0F, 340.0F}, 34.0F},
  };

  GlassReference(std::shared_ptr<const provider::Image> backdrop,
                 std::optional<theme::Glass> glass, bool sheet)
      : backdrop_(std::move(backdrop)), glass_(glass), sheet_(sheet) {}

  auto operator==(const GlassReference&) const -> bool = default;

  static auto glass_named(std::string_view name)
      -> std::optional<theme::Glass> {
    if (name == "regular") {
      return theme::Glass::regular();
    }
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
    return std::nullopt;
  }

  auto body(composition::Context&) const -> composition::Element override {
    std::vector<composition::Element> layers;
    layers.push_back(
        composition::Element{components::Image::Raster(backdrop_)}.frame(
            {.width = width, .height = height}));
    if (glass_.has_value() && sheet_) {
      layers.push_back(sheet(*glass_));
    } else if (glass_.has_value()) {
      for (const Cell& cell : cells) {
        layers.push_back(lens(cell, *glass_));
      }
    }
    return components::ZStack(
               components::ZStackOptions{
                   .alignment = components::Alignment::top_leading},
               std::move(layers))
        .frame({.width = width,
                .height = height,
                .alignment = components::Alignment::top_leading});
  }

 private:
  static auto lens(const Cell& cell, theme::Glass glass)
      -> composition::Element {
    switch (cell.shape) {
      case Shape::arc: {
        graphics::Path path;
        path.add_arc(arc_centre, arc_radius, motion::Angle::degrees(arc_start),
                     motion::Angle::degrees(arc_end), false);
        return composition::Element{
            components::ShapeView(std::move(path))
                .stroke(theme::Colour::hex(0xFFFFFF),
                        graphics::StrokeStyle{
                            .line_width = arc_width,
                            .line_cap = graphics::LineCap::round,
                            .line_join = graphics::LineJoin::round})
                .glass_effect(glass)}
            .frame({.width = width, .height = height});
      }
      case Shape::circle:
        return placed(theme::Circle{}, glass, cell.rect);
      case Shape::capsule:
        return placed(theme::Shape::capsule(), glass, cell.rect);
      case Shape::rounded:
        break;
    }
    return placed(theme::Shape::rounded(cell.radius), glass, cell.rect);
  }

  static auto placed(theme::Shape shape, theme::Glass glass,
                     composition::Rect rect) -> composition::Element {
    return composition::Element{components::ZStack()}
        .frame({.width = rect.width, .height = rect.height})
        .glass_effect(glass, shape)
        .offset(rect.x, rect.y);
  }

  static auto sheet(theme::Glass glass) -> composition::Element {
    const auto button = [](std::string_view label) {
      return composition::Element{
          components::Button(std::string{label}, [] {
          }).button_style(components::GlassButtonStyle{})};
    };
    return components::VStack(
               components::VStackOptions{
                   .alignment = components::HorizontalAlignment::leading,
                   .spacing = theme::Space{14.0F}},
               components::Text("Sheet").font(
                   theme::Font::system(22.0F, theme::Font::Weight::bold)),
               components::Text("Content slides up over a dimmed page.")
                   .font(theme::Font::system(17.0F)),
               components::HStack(
                   components::HStackOptions{.spacing = theme::Space{10.0F}},
                   button("Done"), button("Present alert"), button("Block B")))
        .padding(theme::Space{24.0F})
        .frame({.width = 386.0F,
                .height = 520.0F,
                .alignment = components::Alignment::top_leading})
        .glass_effect(glass, theme::Shape::rounded(34.0F))
        .offset(8.0F, 346.0F);
  }

  std::shared_ptr<const provider::Image> backdrop_;
  std::optional<theme::Glass> glass_;
  bool sheet_{false};
};

class GlassReferenceApp {
 public:
  static auto run(const ExampleOptions& options) -> int {
    const theme::ColorScheme scheme =
        options.scheme().value_or(theme::ColorScheme::light);
    const std::filesystem::path file =
        std::filesystem::path{ARISA_GLASS_FIXTURES} /
        (scheme == theme::ColorScheme::dark ? "glass_backdrop_dark.png"
                                            : "glass_backdrop_light.png");
    std::ifstream stream{file, std::ios::binary};
    const std::vector<char> raw{std::istreambuf_iterator<char>{stream},
                                std::istreambuf_iterator<char>{}};
    std::vector<std::byte> bytes(raw.size());
    std::ranges::transform(raw, bytes.begin(),
                           [](char value) { return std::byte(value); });
    auto decoded = provider::Image::decode(bytes);
    if (!decoded.has_value()) {
      std::println(stderr, "the backdrop {} could not be decoded",
                   file.string());
      return 1;
    }
    auto factory = DesktopExample::factory(options, "arisa glass reference");
    if (!factory.has_value()) {
      std::println(stderr, "{}", factory.error());
      return 1;
    }
    const bool sheet = options.theme().value_or("") == "sheet";
    auto application = runtime::Application::create(
        *factory,
        composition::Element{
            GlassReference{
                std::make_shared<const provider::Image>(std::move(*decoded)),
                GlassReference::glass_named(options.page().value_or("none")),
                sheet}}
            .frame({.max_width = composition::infinity,
                    .max_height = composition::infinity,
                    .alignment = components::Alignment::top_leading})
            .environment(components::Environment::colour_scheme, scheme),
        runtime::ApplicationOptions{
            .executors = std::nullopt,
            .fonts = GalleryFonts::load(factory->assets()),
            .atlas = {},
            .interaction = {}});
    if (!application.has_value()) {
      std::println(stderr, "the application could not start: {}",
                   application.error().explanation());
      return 1;
    }
    const std::uint32_t limit = options.frames().value_or(3);
    std::uint32_t frame = 0;
    if (auto ran = application->run_until([&](runtime::Root&) {
          ++frame;
          return frame >= limit;
        });
        !ran.has_value()) {
      std::println(stderr, "frame {} failed: {}", frame,
                   ran.error().explanation());
      return 1;
    }
    if (options.capture().has_value()) {
      if (auto captured =
              DesktopExample::capture(factory->gpu(), *options.capture());
          !captured.has_value()) {
        std::println(stderr, "{}", captured.error());
        return 1;
      }
    }
    return 0;
  }
};

}

auto main(int argc, char* argv[]) -> int {
  const std::span arguments{argv, static_cast<std::size_t>(argc)};
  const auto options =
      arisa::examples::ExampleOptions::parse(arguments.subspan(1));
  if (!options.has_value()) {
    std::println(stderr, "{}", options.error());
    return 2;
  }
  return arisa::examples::GlassReferenceApp::run(*options);
}
