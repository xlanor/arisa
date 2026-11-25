#include <arisa/components/button.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/content_shape_kinds.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/provider/gpu/image.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/surface/surface_scale.hpp>
#include <arisa/provider/surface/surface_size.hpp>
#include <arisa/runtime/application.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>

#include <focus_fixture.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace arisa::runtime {

class FocusRingTest : public FocusFixture {
 protected:
  static constexpr std::uint32_t side = 240;
  static constexpr float control_width = 120.0F;
  static constexpr float control_height = 48.0F;
  static constexpr int seam_tolerance = 16;
  static constexpr int colour_slack = 4;
  static constexpr float cell_inset = 4.0F;
  static constexpr std::size_t max_seam = 3;

  static auto scene(theme::Shape shape, float inset) -> composition::Element {
    return components::ZStack(
               components::Button(
                   [] {}, components::ZStack()
                              .frame({.width = control_width,
                                      .height = control_height})
                              .background(theme::Colour::hex(0xF0F0F0), shape))
                   .button_style(components::ButtonStyle::plain())
                   .content_shape(composition::ContentShapeKinds::focus_effect,
                                  shape)
                   .focus_ring_inset(inset)
                   .id("control"))
        .frame({.width = static_cast<float>(side),
                .height = static_cast<float>(side)})
        .background(theme::Colour::hex(0x000000));
  }

  static constexpr float bar_height = 36.0F;
  static constexpr float bar_item_height = 24.0F;
  static constexpr float bar_width = 160.0F;
  static constexpr float outset = 3.0F;
  static constexpr int dark = 8;

  static auto control(theme::Shape shape) -> composition::Element {
    return components::Button(
               [] {},
               components::ZStack()
                   .frame({.width = control_width, .height = bar_item_height})
                   .background(theme::Colour::hex(0xF0F0F0), shape))
        .button_style(components::ButtonStyle::plain())
        .content_shape(composition::ContentShapeKinds::focus_effect, shape)
        .id("control");
  }

  static auto toolbar() -> composition::Element {
    return components::ZStack(
               components::HStack(
                   components::HStackOptions{
                       .alignment = components::VerticalAlignment::center,
                       .spacing = theme::Space{0.0F}},
                   control(theme::Shape::capsule()),
                   components::ZStack().frame(
                       {.width = bar_width - control_width,
                        .height = bar_item_height}))
                   .padding(theme::Insets::symmetric(
                       theme::Space{0.0F},
                       theme::Space{(bar_height - bar_item_height) / 2.0F}))
                   .background(theme::Colour::hex(0x404040),
                               theme::Shape::capsule()))
        .frame({.width = static_cast<float>(side),
                .height = static_cast<float>(side)})
        .background(theme::Colour::hex(0x000000));
  }

  static auto free_standing() -> composition::Element {
    return components::ZStack(control(theme::Shape::capsule()))
        .frame({.width = static_cast<float>(side),
                .height = static_cast<float>(side)})
        .background(theme::Colour::hex(0x000000));
  }

  static auto capture(composition::Element content, float scale)
      -> provider::Image {
    provider::headless::HeadlessProviderFactory providers;
    const auto pixels = static_cast<std::uint32_t>(side * scale);
    const provider::SurfaceSize extent{
        .width = provider::SurfaceWidth{pixels},
        .height = provider::SurfaceHeight{pixels}};
    providers.surface().resize(extent);
    providers.surface().set_scale(*provider::SurfaceScale::create(scale));
    providers.gpu().rasterize(extent);
    Application application = start(providers, std::move(content));
    frames(application, 3);
    auto image = providers.gpu().capture();
    EXPECT_TRUE(image.has_value());
    return std::move(*image);
  }

  static auto beyond_capsule(float x, float y, float width, float height)
      -> float {
    const float radius = height / 2.0F;
    const float centre_x = side / 2.0F;
    const float centre_y = side / 2.0F;
    const float along =
        std::max(std::abs(x - centre_x) - ((width / 2.0F) - radius), 0.0F);
    const float across = std::abs(y - centre_y);
    return std::hypot(along, across) - radius;
  }

  static auto render(theme::Shape shape, float scale, float inset = 0.0F)
      -> provider::Image {
    provider::headless::HeadlessProviderFactory providers;
    const auto pixels = static_cast<std::uint32_t>(side * scale);
    const provider::SurfaceSize extent{
        .width = provider::SurfaceWidth{pixels},
        .height = provider::SurfaceHeight{pixels}};
    providers.surface().resize(extent);
    providers.surface().set_scale(*provider::SurfaceScale::create(scale));
    providers.gpu().rasterize(extent);
    Application application = start(providers, scene(shape, inset));
    frames(application, 3);
    auto image = providers.gpu().capture();
    EXPECT_TRUE(image.has_value());
    return std::move(*image);
  }

  static auto luma(const provider::Image& image, std::uint32_t x,
                   std::uint32_t y) -> int {
    const auto pixel = image.pixel(x, y);
    return pixel.has_value() ? ((*pixel)[0] + (*pixel)[1] + (*pixel)[2]) / 3
                             : 0;
  }

  static auto crossings(std::span<const int> line, int ring, int control)
      -> int {
    const auto matches = [](int value, int wanted) {
      return std::abs(value - wanted) <= colour_slack;
    };
    int found = 0;
    for (std::size_t at = 0; at + 1 < line.size(); ++at) {
      if (!matches(line[at], ring)) {
        continue;
      }
      for (std::size_t step = 1; step <= max_seam && at + step < line.size();
           ++step) {
        if (matches(line[at + step], ring)) {
          break;
        }
        if (matches(line[at + step], control)) {
          const auto between = line.subspan(at + 1, step - 1);
          found += static_cast<int>(std::ranges::count_if(
              between,
              [ring](int value) { return value < ring - seam_tolerance; }));
          break;
        }
      }
    }
    return found;
  }

  static auto seams(const provider::Image& image, float scale,
                    float ring_offset = -1.5F) -> int {
    const int control = luma(image, image.width() / 2, image.height() / 2);
    const auto ring_x = static_cast<std::uint32_t>(
        (((side - control_width) / 2.0F) + ring_offset) * scale);
    const int ring = luma(image, ring_x, image.height() / 2);
    EXPECT_GT(control, ring + seam_tolerance);
    int found = 0;
    std::vector<int> line;
    for (std::uint32_t y = 0; y < image.height(); ++y) {
      line.clear();
      for (std::uint32_t x = 0; x < image.width(); ++x) {
        line.push_back(luma(image, x, y));
      }
      found += crossings(line, ring, control);
      std::ranges::reverse(line);
      found += crossings(line, ring, control);
    }
    for (std::uint32_t x = 0; x < image.width(); ++x) {
      line.clear();
      for (std::uint32_t y = 0; y < image.height(); ++y) {
        line.push_back(luma(image, x, y));
      }
      found += crossings(line, ring, control);
      std::ranges::reverse(line);
      found += crossings(line, ring, control);
    }
    return found;
  }
};

TEST_F(FocusRingTest, DifferentiateAddsAHaloAndDisablingTheEffectDropsTheRing) {
  const auto strokes = [](composition::Element content) {
    provider::headless::HeadlessProviderFactory providers;
    Application application = start(providers, std::move(content));
    frames(application, 3);
    return static_cast<std::size_t>(std::ranges::count_if(
        providers.gpu().last_shapes().vertices,
        [](const provider::ShapeVertex& vertex) {
          return vertex.kind == provider::ShapeKind::stroke;
        }));
  };
  const std::size_t plain = strokes(free_standing());
  ASSERT_GT(plain, 0U);
  EXPECT_EQ(
      strokes(free_standing().environment(
          components::Environment::accessibility_differentiate_without_color,
          true)),
      plain + 4U);
  EXPECT_LT(strokes(free_standing().focus_effect_disabled()), plain);
}

TEST_F(FocusRingTest, FocusEffectDisabledIsReadableBelowIt) {
  class Reader final : public composition::Component {
   public:
    explicit Reader(std::shared_ptr<std::vector<bool>> seen)
        : seen_(std::move(seen)) {}

    auto operator==(const Reader&) const -> bool = default;

    auto body(composition::Context& context) const
        -> composition::Element override {
      seen_->push_back(context.environment(
          components::Environment::is_focus_effect_enabled));
      return components::ZStack().frame({.width = 10.0F, .height = 10.0F});
    }

   private:
    std::shared_ptr<std::vector<bool>> seen_;
  };
  provider::headless::HeadlessProviderFactory providers;
  auto seen = std::make_shared<std::vector<bool>>();
  Application application =
      start(providers,
            components::HStack(
                composition::Element{Reader{seen}},
                composition::Element{Reader{seen}}.focus_effect_disabled()));
  frames(application, 2);
  ASSERT_GE(seen->size(), 2U);
  EXPECT_TRUE((*seen)[0]);
  EXPECT_FALSE((*seen)[1]);
}

TEST_F(FocusRingTest, RingMeetsTheControlWithoutABackgroundSeam) {
  for (const float scale : {1.0F, 1.5F}) {
    for (const theme::Shape& shape :
         {theme::Shape::rectangle(), theme::Shape::capsule(),
          theme::Shape::rounded(12.0F), theme::Shape::rounded(12.0F, false)}) {
      const provider::Image image = render(shape, scale);
      EXPECT_EQ(seams(image, scale), 0) << "scale " << scale;
    }
  }
}

TEST_F(FocusRingTest, RingFollowsUnevenAndEllipticalOutlines) {
  const theme::UnevenRoundedRectangle uneven{0.0F, 0.0F, 20.0F, 0.0F};
  const provider::Image image = render(uneven, 1.0F);
  EXPECT_EQ(seams(image, 1.0F), 0);
  const auto left = static_cast<std::uint32_t>((side - control_width) / 2.0F);
  const auto top = static_cast<std::uint32_t>((side - control_height) / 2.0F);
  const std::uint32_t right = left + static_cast<std::uint32_t>(control_width);
  const std::uint32_t bottom = top + static_cast<std::uint32_t>(control_height);
  EXPECT_GT(luma(image, left - 2, top - 2), dark);
  EXPECT_LE(luma(image, right + 1, bottom + 1), dark);
  EXPECT_GT(luma(image, right + 1, top - 2), dark);
  const provider::Image oval = render(theme::Ellipse{}, 1.0F);
  EXPECT_EQ(seams(oval, 1.0F), 0);
  EXPECT_LE(luma(oval, left - 2, top - 2), dark);
  EXPECT_GT(luma(oval, left - 2, side / 2), dark);
}

TEST_F(FocusRingTest, InsetRingOverACellKeepsNoSeam) {
  for (const float scale : {1.0F, 1.5F}) {
    const provider::Image image =
        render(theme::Shape::rounded(12.0F), scale, cell_inset);
    EXPECT_EQ(seams(image, scale, cell_inset - 1.5F), 0) << "scale " << scale;
  }
}

TEST_F(FocusRingTest, FlushToolbarItemRingStaysInsideTheBar) {
  for (const float scale : {1.0F, 1.5F, 2.0F}) {
    const provider::Image image = capture(toolbar(), scale);
    int escaped = 0;
    for (std::uint32_t y = 0; y < image.height(); ++y) {
      for (std::uint32_t x = 0; x < image.width(); ++x) {
        const float px = (static_cast<float>(x) + 0.5F) / scale;
        const float py = (static_cast<float>(y) + 0.5F) / scale;
        if (beyond_capsule(px, py, bar_width, bar_height) > 1.0F / scale &&
            luma(image, x, y) > dark) {
          ++escaped;
        }
      }
    }
    EXPECT_EQ(escaped, 0) << "scale " << scale;
    const auto band_x = static_cast<std::uint32_t>(
        (((side - bar_width) / 2.0F) + 1.5F) * scale);
    const auto middle = static_cast<std::uint32_t>((side / 2.0F) * scale);
    const auto fill_x = static_cast<std::uint32_t>(
        (((side + bar_width) / 2.0F) - 12.0F) * scale);
    EXPECT_GT(
        std::abs(luma(image, band_x, middle) - luma(image, fill_x, middle)),
        seam_tolerance)
        << "scale " << scale;
  }
}

TEST_F(FocusRingTest, FreeStandingRingKeepsItsOutset) {
  for (const float scale : {1.0F, 1.5F, 2.0F}) {
    const provider::Image image = capture(free_standing(), scale);
    const auto middle = static_cast<std::uint32_t>((side / 2.0F) * scale);
    const float left = (side - control_width) / 2.0F;
    const float top = (side - bar_item_height) / 2.0F;
    const auto at = [scale](float points) {
      return static_cast<std::uint32_t>(points * scale);
    };
    EXPECT_GT(luma(image, at(left - (outset / 2.0F)), middle), dark)
        << "scale " << scale;
    EXPECT_LE(luma(image, at(left - outset - 1.5F), middle), dark)
        << "scale " << scale;
    EXPECT_GT(luma(image, middle, at(top - (outset / 2.0F))), dark)
        << "scale " << scale;
    EXPECT_LE(luma(image, middle, at(top - outset - 1.5F)), dark)
        << "scale " << scale;
  }
}

}
