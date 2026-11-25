#include <arisa/components/button.hpp>
#include <arisa/components/text.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/node.hpp>
#include <arisa/motion/animatable_vector.hpp>
#include <arisa/motion/animation.hpp>
#include <arisa/motion/spring.hpp>
#include <arisa/presentation/presentation_layer.hpp>
#include <arisa/provider/gpu/shape_vertex.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/headless/null_gpu.hpp>
#include <arisa/rendering/painter.hpp>
#include <arisa/structure/tab_view.hpp>
#include <arisa/theme/metrics.hpp>
#include <arisa/theme/theme.hpp>

#include <fidelity_page.hpp>
#include <gallery_fixture.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <format>
#include <fstream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace arisa::runtime {

class GalleryFidelityTest : public GalleryFixture {
 protected:
  struct Size {
    float width{0.0F};
    float height{0.0F};
    float radius{0.0F};
    std::uint32_t colour{0};
  };

  static auto reference() -> const std::map<std::string, std::string>& {
    static const std::map<std::string, std::string> rows = [] {
      std::map<std::string, std::string> found;
      std::ifstream stream{ARISA_SWIFTUI_REFERENCE};
      std::string line;
      while (std::getline(stream, line)) {
        if (line.empty() || line.front() == '#') {
          continue;
        }
        std::istringstream fields{line};
        std::string key;
        std::string light;
        std::getline(fields, key, '\t');
        std::getline(fields, light, '\t');
        found[key] = light;
      }
      return found;
    }();
    return rows;
  }

  static auto value(std::string_view key) -> double {
    const auto found = reference().find(std::string{key});
    EXPECT_NE(found, reference().end()) << key;
    if (found == reference().end()) {
      return 0.0;
    }
    return std::stod(found->second);
  }

  static auto frame(Application& application, std::string_view element)
      -> composition::Rect {
    const composition::Tree& tree = application.root().tree();
    const auto id = tree.find(
        composition::ElementKey{examples::FidelityPage::key(element)});
    EXPECT_TRUE(id.has_value()) << element;
    if (!id.has_value()) {
      return {};
    }
    const composition::Node& node = tree.node(*id)->get();
    EXPECT_TRUE(node.presented().has_value()) << element;
    return node.presented().has_value() ? node.presented()->bounds
                                        : composition::Rect{};
  }

  static auto text_inside(Application& application, std::string_view element,
                          std::string_view label)
      -> std::optional<composition::Rect> {
    const composition::Tree& tree = application.root().tree();
    const auto id = tree.find(
        composition::ElementKey{examples::FidelityPage::key(element)});
    if (!id.has_value()) {
      return std::nullopt;
    }
    std::optional<composition::Rect> found;
    walk(tree.node(*id)->get(), [&](const composition::Node& node) {
      const auto text = node.content_as<components::Text::Content>();
      if (!found.has_value() && text.has_value() &&
          text->get().text() == label && node.presented().has_value()) {
        found = node.presented()->bounds;
      }
    });
    return found;
  }

  static auto fills(provider::headless::HeadlessProviderFactory& providers,
                    composition::Rect within) -> std::vector<Size> {
    const auto record = providers.gpu().last_shapes();
    std::vector<Size> found;
    for (std::size_t base = 0; base + 3 < record.vertices.size(); base += 4) {
      const provider::ShapeVertex& corner = record.vertices[base];
      const provider::ShapeVertex& opposite = record.vertices[base + 2];
      if (corner.kind != provider::ShapeKind::fill ||
          (corner.colour >> 24U) < 128U) {
        continue;
      }
      const float cx = (corner.position[0] + opposite.position[0]) * 0.5F;
      const float cy = (corner.position[1] + opposite.position[1]) * 0.5F;
      if (cx < within.x || cx > within.x + within.width || cy < within.y ||
          cy > within.y + within.height) {
        continue;
      }
      found.push_back(
          Size{.width = corner.half_size[0] * 2.0F,
               .height = corner.half_size[1] * 2.0F,
               .radius = corner.shape[provider::ShapeVertex::radius],
               .colour = corner.colour});
    }
    return found;
  }

  static auto has(const std::vector<Size>& found, float width, float height,
                  float tolerance = 1.0F) -> bool {
    return std::ranges::any_of(found, [&](const Size& each) {
      return std::abs(each.width - width) <= tolerance &&
             std::abs(each.height - height) <= tolerance;
    });
  }

  static auto reveal(provider::headless::HeadlessProviderFactory& providers,
                     Application& application, std::string_view element)
      -> composition::Rect {
    composition::Rect shown = frame(application, element);
    for (int attempt = 0; attempt < 6; ++attempt) {
      if (shown.y > 100.0F && shown.y + shown.height < 620.0F) {
        break;
      }
      scroll_by(providers, application,
                provider::PixelPoint{.x = 1100.0F, .y = 400.0F},
                std::clamp(shown.y - 300.0F, -300.0F, 300.0F));
      shown = frame(application, element);
    }
    return shown;
  }

  static auto open(provider::headless::HeadlessProviderFactory& providers,
                   theme::ColorScheme scheme = theme::ColorScheme::light)
      -> Application {
    Application application =
        start_gallery(providers, examples::GalleryPage::fidelity, scheme);
    run(providers, application, 30);
    return application;
  }

  static auto sample(const motion::Animation& animation, double time)
      -> double {
    return animation
        .sample(motion::Animation::Span{.from = motion::AnimatableVector{0.0},
                                        .to = motion::AnimatableVector{1.0},
                                        .velocity = {},
                                        .precision = 1.0e-9},
                time)
        .value[0];
  }

  static auto curve_error(const motion::Animation& animation,
                          std::string_view series) -> double {
    double worst = 0.0;
    std::size_t matched = 0;
    for (const auto& [key, text] : reference()) {
      const std::string prefix = std::format("motion.{}.sample.", series);
      if (!key.starts_with(prefix)) {
        continue;
      }
      const double time = std::stod(key.substr(prefix.size())) / 1000.0;
      worst =
          std::max(worst, std::abs(sample(animation, time) - std::stod(text)));
      ++matched;
    }
    EXPECT_GT(matched, 50U) << series;
    return worst;
  }

  static constexpr float exact = 0.51F;
};

TEST_F(GalleryFidelityTest, TypeStylesMatchSwiftUI) {
  const theme::Theme theme = theme::Theme::defaults();
  constexpr std::array roles{
      std::pair{theme::TextRole::large_title, "large_title"},
      std::pair{theme::TextRole::title, "title"},
      std::pair{theme::TextRole::title2, "title2"},
      std::pair{theme::TextRole::title3, "title3"},
      std::pair{theme::TextRole::headline, "headline"},
      std::pair{theme::TextRole::subheadline, "subheadline"},
      std::pair{theme::TextRole::body, "body"},
      std::pair{theme::TextRole::callout, "callout"},
      std::pair{theme::TextRole::footnote, "footnote"},
      std::pair{theme::TextRole::caption, "caption"},
      std::pair{theme::TextRole::caption2, "caption2"}};
  for (const auto& [role, name] : roles) {
    const theme::TypeStyle& style = theme.type(role);
    EXPECT_FLOAT_EQ(style.size, value(std::format("text.{}.size", name)))
        << name;
    EXPECT_EQ(style.weight, value(std::format("text.{}.weight", name))) << name;
    EXPECT_NEAR(style.line_height,
                value(std::format("text.{}.line_height", name)), exact)
        << name;
  }
}

TEST_F(GalleryFidelityTest, TextRunsMatchSwiftUIWidths) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers);
  for (const std::string_view name :
       {"large_title", "title", "title3", "headline", "body", "caption"}) {
    const composition::Rect shown =
        frame(application, std::format("text.{}", name));
    const double width = value(std::format("text.{}.swiftui_width", name));
    EXPECT_NEAR(shown.width, width, width * 0.06) << name;
    EXPECT_NEAR(shown.height, value(std::format("text.{}.line_height", name)),
                exact)
        << name;
  }
}

TEST_F(GalleryFidelityTest, StackSpacingPaddingAndSpacerMatchSwiftUI) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers);
  const composition::Rect va = frame(application, "vstack.a");
  const composition::Rect vb = frame(application, "vstack.b");
  EXPECT_NEAR(vb.y - (va.y + va.height), value("stack.vstack.text_spacing"),
              exact);
  const composition::Rect ha = frame(application, "hstack.a");
  const composition::Rect hb = frame(application, "hstack.b");
  EXPECT_NEAR(hb.x - (ha.x + ha.width), value("stack.hstack.text_spacing"),
              exact);
  const composition::Rect padded = frame(application, "padding.text");
  EXPECT_NEAR((padded.height - value("text.body.line_height")) * 0.5,
              value("padding.default.top"), exact);
  const composition::Rect sa = frame(application, "spacer.a");
  const composition::Rect sb = frame(application, "spacer.b");
  EXPECT_NEAR(sb.x - (sa.x + sa.width), value("spacer.horizontal.min_length"),
              exact);
  EXPECT_FLOAT_EQ(theme::metrics::stack.spacing,
                  value("stack.hstack.shape_spacing"));
}

TEST_F(GalleryFidelityTest, ButtonsMatchSwiftUI) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers);
  for (const std::string_view style :
       {"automatic", "bordered", "bordered_prominent"}) {
    const composition::Rect shown =
        reveal(providers, application, std::format("button.{}", style));
    EXPECT_NEAR(shown.height,
                value(std::format("button.{}.regular.height", style)), exact)
        << style;
    EXPECT_NEAR(shown.width,
                value(std::format("button.{}.regular.width", style)), 4.0)
        << style;
    const auto label =
        text_inside(application, std::format("button.{}", style), "Button");
    if (label.has_value()) {
      EXPECT_NEAR(
          label->x - shown.x,
          value(std::format("button.{}.regular.padding_leading", style)), exact)
          << style;
      EXPECT_NEAR(label->y - shown.y,
                  value(std::format("button.{}.regular.padding_top", style)),
                  exact)
          << style;
    }
    const auto drawn = fills(providers, shown);
    ASSERT_FALSE(drawn.empty()) << style;
    EXPECT_NEAR(drawn.front().radius, theme::metrics::button.corner_radius,
                exact)
        << style;
  }
  for (const std::string_view style : {"borderless", "plain"}) {
    const composition::Rect shown =
        frame(application, std::format("button.{}", style));
    EXPECT_NEAR(shown.height,
                value(std::format("button.{}.regular.height", style)), exact)
        << style;
  }
  EXPECT_NEAR(theme::metrics::button.corner_radius,
              value("button.bordered.regular.radius"), 0.5);
}

TEST_F(GalleryFidelityTest, TogglesMatchSwiftUIInPainterOutput) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers);
  const composition::Rect switched =
      reveal(providers, application, "toggle.switch_on");
  EXPECT_NEAR(switched.height, value("toggle.switch.regular.on.height"), exact);
  const auto track = fills(providers, switched);
  EXPECT_TRUE(
      has(track, static_cast<float>(value("toggle.switch.regular.on.width")),
          static_cast<float>(value("toggle.switch.regular.on.height"))));
  EXPECT_TRUE(has(
      track, static_cast<float>(value("toggle.switch.regular.on.knob_width")),
      static_cast<float>(value("toggle.switch.regular.on.knob_height"))));
  const composition::Rect boxed =
      reveal(providers, application, "toggle.checkbox");
  const auto box = fills(providers, boxed);
  const auto extent =
      static_cast<float>(value("toggle.checkbox.regular.on.width"));
  EXPECT_TRUE(has(box, extent, extent));
  const auto checkbox = std::ranges::find_if(box, [extent](const Size& each) {
    return std::abs(each.width - extent) <= 1.0F;
  });
  if (checkbox != box.end()) {
    EXPECT_NEAR(checkbox->radius, value("toggle.checkbox.regular.on.radius"),
                0.5);
  }
  const auto label = text_inside(application, "toggle.checkbox", "Checkbox");
  ASSERT_TRUE(label.has_value());
  EXPECT_NEAR(label->x - boxed.x,
              value("toggle.checkbox.labelled.label_leading"), exact);
}

TEST_F(GalleryFidelityTest, SliderPickerMenuProgressAndFieldMatchSwiftUI) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers);
  const composition::Rect slider = reveal(providers, application, "slider");
  EXPECT_NEAR(slider.height, value("slider.regular.height"), exact);
  const auto knob = fills(providers, slider);
  EXPECT_TRUE(has(knob, static_cast<float>(value("slider.regular.knob_width")),
                  static_cast<float>(value("slider.regular.knob_height"))));
  EXPECT_TRUE(std::ranges::any_of(knob, [](const Size& each) {
    return std::abs(each.height - static_cast<float>(value(
                                      "slider.regular.track_thickness"))) <=
           0.5F;
  }));
  EXPECT_NEAR(frame(application, "picker.segmented").height,
              value("picker.segmented.regular.height"), exact);
  const composition::Rect picker = frame(application, "picker.menu");
  EXPECT_NEAR(picker.height, value("picker.menu.regular.height"), exact);
  EXPECT_NEAR(frame(application, "menu").height, value("menu.regular.height"),
              exact);
  const composition::Rect bar =
      reveal(providers, application, "progress.linear");
  EXPECT_TRUE(std::ranges::any_of(fills(providers, bar), [](const Size& each) {
    return std::abs(each.height -
                    static_cast<float>(value(
                        "progress.linear.regular.bar_thickness"))) <= 0.5F;
  }));
  EXPECT_NEAR(frame(application, "progress.circular").height,
              value("progress.circular.regular.height"), exact);
  EXPECT_NEAR(frame(application, "text_field").height,
              value("text_field.rounded_border.regular.height"), exact);
}

TEST_F(GalleryFidelityTest, ListsSidebarBarsAndAlertsMatchSwiftUI) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers);
  const auto one = text_inside(application, "form", "One");
  const auto two = text_inside(application, "form", "Two");
  ASSERT_TRUE(one.has_value() && two.has_value());
  EXPECT_NEAR(two->y - one->y, value("form.grouped.row_pitch"), 1.0);
  const auto plain_one = text_inside(application, "list.plain", "One");
  const auto plain_two = text_inside(application, "list.plain", "Two");
  ASSERT_TRUE(plain_one.has_value() && plain_two.has_value());
  EXPECT_NEAR(plain_two->y - plain_one->y, value("list.plain.row_pitch"), 1.0);
  const composition::Tree& tree = application.root().tree();
  const auto sidebar = tree.find(composition::ElementKey{"arisa.tab.sidebar"});
  ASSERT_TRUE(sidebar.has_value());
  EXPECT_NEAR(tree.node(*sidebar)->get().presented()->bounds.width,
              value("sidebar.width"), 1.0);
  EXPECT_NEAR(theme::metrics::list.sidebar_row_height,
              value("sidebar.row_pitch"), exact);
  EXPECT_NEAR(theme::metrics::list.sidebar_selection_inset,
              value("sidebar.selection_inset_leading"), exact);
  EXPECT_NEAR(theme::metrics::list.sidebar_selection_radius,
              value("sidebar.selection_radius"), 0.5);
  EXPECT_NEAR(theme::metrics::list.grouped_margin,
              value("form.grouped.group_inset"), exact);
  EXPECT_NEAR(theme::metrics::list.grouped_corner_radius,
              value("form.grouped.group_radius"), 0.5);
  EXPECT_NEAR(theme::metrics::navigation.bar_height,
              value("navigation.swiftui_stack.bar_height"), exact);
  EXPECT_NEAR(presentation::PresentationLayer::alert_width,
              value("presentation.alert.width"), exact);
  EXPECT_NEAR(theme::metrics::alert.button_height,
              value("presentation.alert.button_height"), exact);
  EXPECT_NEAR(theme::metrics::alert.inset,
              value("presentation.alert_single.button_side_inset"), exact);
  EXPECT_NEAR(theme::metrics::alert.button_gap,
              value("presentation.alert.button_gap"), exact);
  EXPECT_NEAR(rendering::Painter::focus_ring_width, value("focus_ring.width"),
              exact);
  EXPECT_NEAR(
      rendering::Painter::focus_ring_gap + rendering::Painter::focus_ring_width,
      value("focus_ring.outset"), exact);
}

TEST_F(GalleryFidelityTest, SpringPresetsFollowSwiftUICurves) {
  constexpr double analytic = 1.0e-5;
  constexpr double rendered = 0.02;
  EXPECT_LT(curve_error(motion::Animation::smooth(), "smooth.spring_value"),
            analytic);
  EXPECT_LT(curve_error(motion::Animation::snappy(), "snappy.spring_value"),
            analytic);
  EXPECT_LT(curve_error(motion::Animation::bouncy(), "bouncy.spring_value"),
            analytic);
  EXPECT_LT(curve_error(motion::Animation::spring(motion::Spring{}),
                        "spring_init.spring_value"),
            analytic);
  EXPECT_LT(curve_error(
                motion::Animation::spring(motion::Spring{
                    motion::Spring::Duration{.duration = 0.3, .bounce = -0.3}}),
                "duration_0_3_bounce_minus_0_3.spring_value"),
            analytic);
  EXPECT_LT(curve_error(
                motion::Animation::spring(motion::Spring{
                    motion::Spring::Duration{.duration = 1.0, .bounce = 0.5}}),
                "duration_1_bounce_0_5.spring_value"),
            analytic);
  EXPECT_LT(curve_error(motion::Animation::spring(0.3, 0.6),
                        "response_0_3_damping_0_6.spring_value"),
            analytic);
  EXPECT_LT(
      curve_error(
          motion::Animation::spring(motion::Spring{motion::Spring::Physical{
              .mass = 1.0, .stiffness = 100.0, .damping = 10.0}}),
          "mass_1_stiffness_100_damping_10.spring_value"),
      analytic);
  EXPECT_LT(
      curve_error(motion::Animation::smooth(0.35), "smooth_0_35.spring_value"),
      analytic);
  EXPECT_LT(
      curve_error(motion::Animation::snappy(0.2), "snappy_0_2.spring_value"),
      analytic);
  EXPECT_LT(curve_error(motion::Animation::bouncy(0.5, 0.1),
                        "bouncy_extra_0_1.spring_value"),
            analytic);
  EXPECT_LT(
      curve_error(motion::Animation::default_animation(), "default.animation"),
      rendered);
  EXPECT_LT(curve_error(motion::Animation::smooth(), "smooth.animation"),
            rendered);
  EXPECT_LT(curve_error(motion::Animation::snappy(), "snappy.animation"),
            rendered);
  EXPECT_LT(curve_error(motion::Animation::bouncy(), "bouncy.animation"),
            rendered);
  EXPECT_LT(curve_error(motion::Animation::spring(), "spring.animation"),
            rendered);
  EXPECT_LT(curve_error(motion::Animation::interactive_spring(),
                        "interactive_spring.animation"),
            rendered);
  EXPECT_LT(
      curve_error(motion::Animation::ease_in_out(), "ease_in_out.animation"),
      0.005);
  EXPECT_LT(curve_error(motion::Animation::ease_in(), "ease_in.animation"),
            0.005);
  EXPECT_LT(curve_error(motion::Animation::ease_out(), "ease_out.animation"),
            0.005);
  EXPECT_LT(curve_error(motion::Animation::linear(), "linear.animation"),
            0.005);
}

}

namespace arisa::runtime {

class GalleryFidelityHandheldTest : public GalleryFixture {};

TEST_F(GalleryFidelityHandheldTest, OpensInHandheldModeAtTheSwitchTwoScale) {
  provider::headless::HeadlessProviderFactory providers;
  providers.surface().set_display_mode(provider::DisplayMode::handheld);
  Application application = start_gallery(
      providers, examples::GalleryPage::fidelity, theme::ColorScheme::light,
      provider::SurfaceSize{.width = provider::SurfaceWidth{1920},
                            .height = provider::SurfaceHeight{1080}},
      std::nullopt, 1.5F);
  run(providers, application, 120);
  EXPECT_TRUE(application.root()
                  .tree()
                  .find(composition::ElementKey{
                      examples::FidelityPage::key("text.large_title")})
                  .has_value());
}

}
