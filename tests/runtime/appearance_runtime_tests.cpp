#include <arisa/appearance/appearance_model.hpp>
#include <arisa/appearance/appearance_policy.hpp>
#include <arisa/components/button.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/motion/animation.hpp>
#include <arisa/motion/with_animation.hpp>
#include <arisa/provider/gpu/shape_vertex.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/headless/memory_storage.hpp>
#include <arisa/structure/appearance_settings_section.hpp>
#include <arisa/structure/form.hpp>
#include <arisa/theme/oklab.hpp>
#include <arisa/theme/theme.hpp>

#include <focus_fixture.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace arisa::runtime {

using composition::Element;

class AppearanceRuntimeTest : public FocusFixture {
 protected:
  struct Seen {
    std::optional<theme::ColorScheme> scheme;
    theme::DynamicTypeSize size{theme::DynamicTypeSize::large};
    bool reduce_motion{false};
    bool reduce_transparency{false};
    theme::ColorSchemeContrast contrast{theme::ColorSchemeContrast::standard};
    bool model{false};
    composition::NavigationSplitViewVisibility sidebar{
        composition::NavigationSplitViewVisibility::all};
    bool button_shapes{false};
    bool differentiate{false};
    bool dim_flashing_lights{false};
  };

  class Reader final : public composition::Component {
   public:
    explicit Reader(std::shared_ptr<Seen> seen) : seen_(std::move(seen)) {}

    auto operator==(const Reader& other) const -> bool {
      return seen_ == other.seen_;
    }

    auto body(composition::Context& context) const -> Element override {
      using components::Environment;
      seen_->scheme = context.environment(Environment::color_scheme);
      seen_->size = context.environment(Environment::dynamic_type_size);
      seen_->reduce_motion =
          context.environment(Environment::accessibility_reduce_motion);
      seen_->reduce_transparency =
          context.environment(Environment::accessibility_reduce_transparency);
      seen_->contrast = context.environment(Environment::color_scheme_contrast);
      seen_->model = context.environment(Environment::appearance).has_value();
      seen_->sidebar = context.environment(Environment::sidebar_visibility);
      seen_->button_shapes =
          context.environment(Environment::accessibility_show_button_shapes);
      seen_->differentiate = context.environment(
          Environment::accessibility_differentiate_without_color);
      seen_->dim_flashing_lights =
          context.environment(Environment::accessibility_dim_flashing_lights);
      return components::ZStack().frame({.width = 10.0F, .height = 10.0F});
    }

   private:
    std::shared_ptr<Seen> seen_;
  };

  class Swatch final : public composition::Component {
   public:
    static constexpr composition::StateKey<bool> warm{"swatch-warm", false};
    static constexpr composition::StateKey<bool> moved{"swatch-moved", false};

    auto operator==(const Swatch&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      const auto toggled = context.state(warm);
      const auto shifted = context.state(moved);
      Element box = components::ZStack()
                        .frame({.width = 100.0F, .height = 40.0F})
                        .background(theme::ColourRole::accent,
                                    theme::Shape::rounded(theme::radius::card))
                        .offset(shifted.get() ? 200.0F : 0.0F)
                        .id("box");
      if (toggled.get()) {
        box = std::move(box).theme_override(warm_theme());
      }
      return components::VStack(std::move(box),
                                components::ZStack()
                                    .frame({.width = 60.0F, .height = 30.0F})
                                    .background(theme::ColourRole::gray)
                                    .id("rule"),
                                components::ZStack()
                                    .frame({.width = 80.0F, .height = 20.0F})
                                    .glass_effect(theme::Glass::regular(),
                                                  theme::Shape::rectangle())
                                    .id("glass"))
          .on_appear([toggled, shifted] {
            handles().warm = toggled;
            handles().moved = shifted;
          });
    }

    struct Handles {
      std::optional<composition::StateHandle<bool>> warm;
      std::optional<composition::StateHandle<bool>> moved;
    };

    static auto handles() -> Handles& {
      static Handles held;
      return held;
    }
  };

  static auto warm_theme() -> theme::Theme {
    return theme::Theme::defaults()
        .with_accent(theme::Colour::hex(0xFF3B30))
        .with(theme::Theme::Radii{
            .control = 6.0F, .card = 4.0F, .sheet = 20.0F, .continuous = true});
  }

  auto start_with(Element content, appearance::AppearancePolicy policy)
      -> Application {
    auto application = Application::create(
        providers_, std::move(content),
        ApplicationOptions{
            .executors = std::nullopt,
            .fonts = {},
            .atlas = {},
            .interaction = {},
            .appearance = AppearanceOptions{.policy = std::move(policy),
                                            .storage = std::ref(storage_)}});
    EXPECT_TRUE(application.has_value());
    Application started = std::move(*application);
    frames(started, 2);
    return started;
  }

  static auto frame_of(Application& application, std::string_view key)
      -> composition::Rect {
    const auto node = node_named(application, key);
    if (!node.has_value()) {
      return composition::Rect{};
    }
    return node->get().presented().has_value() ? node->get().presented()->bounds
                                               : node->get().frame();
  }

  void step(Application& application, std::chrono::milliseconds by) {
    ASSERT_TRUE(providers_.clock().advance(by).has_value());
    frames(application, 1);
  }

  auto fill(float half_width, float half_height)
      -> std::optional<provider::ShapeVertex> {
    for (const provider::ShapeVertex& vertex :
         providers_.gpu().last_shapes().vertices) {
      if (vertex.kind == provider::ShapeKind::fill &&
          std::abs(vertex.half_size[0] - half_width) < 0.6F &&
          std::abs(vertex.half_size[1] - half_height) < 0.6F) {
        return vertex;
      }
    }
    return std::nullopt;
  }

  static auto colour_of(const provider::ShapeVertex& vertex) -> theme::Colour {
    const auto channel = [&vertex](unsigned shift) {
      return static_cast<float>((vertex.colour >> shift) & 0xffU) / 255.0F;
    };
    return theme::Colour{channel(0U), channel(8U), channel(16U), channel(24U)};
  }

  static auto close(theme::Colour left, theme::Colour right) -> bool {
    return std::abs(left.red - right.red) < 0.01F &&
           std::abs(left.green - right.green) < 0.01F &&
           std::abs(left.blue - right.blue) < 0.01F;
  }

  provider::headless::HeadlessProviderFactory providers_;
  provider::headless::MemoryStorage storage_;
};

TEST_F(AppearanceRuntimeTest, TheEnvironmentCarriesEverySetting) {
  auto path = provider::AssetPath::create("appearance.toml");
  ASSERT_TRUE(path.has_value());
  storage_.add(*path,
               "schema = 1\ncolor_scheme = \"dark\"\n"
               "dynamic_type_size = \"accessibility3\"\nreduce_motion = true\n"
               "reduce_transparency = true\nincrease_contrast = true\n"
               "sidebar = \"icons_only\"\n");
  auto seen = std::make_shared<Seen>();
  Application application = start_with(Reader{seen}, {});
  EXPECT_EQ(seen->scheme, theme::ColorScheme::dark);
  EXPECT_EQ(seen->size, theme::DynamicTypeSize::accessibility3);
  EXPECT_TRUE(seen->reduce_motion);
  EXPECT_TRUE(seen->reduce_transparency);
  EXPECT_EQ(seen->contrast, theme::ColorSchemeContrast::increased);
  EXPECT_TRUE(seen->model);
  EXPECT_EQ(seen->sidebar,
            composition::NavigationSplitViewVisibility::icons_only);
  ASSERT_TRUE(application.appearance().has_value());
  application.appearance()->update([](appearance::AppearanceOverrides& o) {
    o.color_scheme = appearance::ColorSchemePreference::light;
    o.reduce_motion = false;
  });
  frames(application, 2);
  EXPECT_EQ(seen->scheme, theme::ColorScheme::light);
  EXPECT_FALSE(seen->reduce_motion);
}

TEST_F(AppearanceRuntimeTest, TheAppearanceSectionShowsTheLabelsItIsGiven) {
  structure::AppearanceSettingsSection::Labels labels;
  labels.header = "Darstellung";
  labels.theme = "Thema";
  labels.dark = "Dunkel";
  labels.text_size = "Textgröße: {}";
  labels.sizes[3] = "Groß";
  labels.reduce_motion = "Bewegung reduzieren";
  labels.haptic_strength = "Haptikstärke";
  labels.theme_names.emplace("default", "Standard");
  Application application = start_with(
      structure::Form(structure::AppearanceSettingsSection{labels}), {});
  EXPECT_TRUE(shows(application, "Darstellung"));
  EXPECT_TRUE(shows(application, "Thema"));
  EXPECT_TRUE(shows(application, "Dunkel"));
  EXPECT_TRUE(shows(application, "Textgröße: Groß"));
  EXPECT_TRUE(shows(application, "Bewegung reduzieren"));
  EXPECT_TRUE(shows(application, "Haptikstärke"));
  EXPECT_FALSE(shows(application, "Reduce motion"));
}

TEST_F(AppearanceRuntimeTest, DisplayAccommodationsReachViewsAndButtons) {
  const auto filled = [](const auto& self, const composition::Node& node) {
    if (node.modifiers().background.has_value()) {
      return true;
    }
    return std::ranges::any_of(
        node.children(),
        [&self](const composition::Node& child) { return self(self, child); });
  };
  const auto build = [](std::shared_ptr<Seen> seen) {
    return components::VStack(
        Element{Reader{std::move(seen)}},
        components::Button("Go", [] {})
            .button_style(components::ButtonStyle::borderless())
            .id("go"));
  };
  auto plain_seen = std::make_shared<Seen>();
  Application plain = start_with(build(plain_seen), {});
  const auto plain_go = node_named(plain, "go");
  ASSERT_TRUE(plain_go.has_value());
  EXPECT_FALSE(filled(filled, plain_go->get()));
  EXPECT_FALSE(plain_seen->button_shapes);
  auto path = provider::AssetPath::create("appearance.toml");
  ASSERT_TRUE(path.has_value());
  storage_.add(*path,
               "schema = 1\nbutton_shapes = true\n"
               "differentiate_without_color = true\n"
               "dim_flashing_lights = true\n");
  auto seen = std::make_shared<Seen>();
  Application application = start_with(build(seen), {});
  EXPECT_TRUE(seen->button_shapes);
  EXPECT_TRUE(seen->differentiate);
  EXPECT_TRUE(seen->dim_flashing_lights);
  const auto go = node_named(application, "go");
  ASSERT_TRUE(go.has_value());
  EXPECT_TRUE(filled(filled, go->get()));
}

TEST_F(AppearanceRuntimeTest, APreferredSchemeOverridesTheSetting) {
  auto path = provider::AssetPath::create("appearance.toml");
  ASSERT_TRUE(path.has_value());
  storage_.add(*path, "schema = 1\ncolor_scheme = \"light\"\n");
  auto seen = std::make_shared<Seen>();
  Application application = start_with(
      components::VStack(Element{Reader{seen}},
                         components::ZStack()
                             .frame({.width = 10.0F, .height = 10.0F})
                             .preferred_color_scheme(theme::ColorScheme::dark)),
      {});
  frames(application, 2);
  EXPECT_EQ(seen->scheme, theme::ColorScheme::dark);
}

TEST_F(AppearanceRuntimeTest, ThemeSwitchesInterpolateInOklabAndSpringRadii) {
  Application application = start_with(Swatch{}, {});
  const theme::Colour from =
      theme::Theme::defaults().palette(theme::ColourRole::accent).light;
  const theme::Colour to =
      warm_theme().palette(theme::ColourRole::accent).light;
  const auto before = fill(50.0F, 20.0F);
  ASSERT_TRUE(before.has_value());
  EXPECT_TRUE(close(colour_of(*before), from));
  const float corner_from = before->shape[provider::ShapeVertex::radius];
  motion::with_animation(motion::Animation::linear(0.4),
                         [] { Swatch::handles().warm->set(true); });
  frames(application, 1);
  step(application, std::chrono::milliseconds{200});
  const auto middle = fill(50.0F, 20.0F);
  ASSERT_TRUE(middle.has_value());
  const theme::Colour seen = colour_of(*middle);
  EXPECT_FALSE(close(seen, from));
  EXPECT_FALSE(close(seen, to));
  const float lightness = theme::Oklab::from(seen).lightness();
  const float low = std::min(theme::Oklab::from(from).lightness(),
                             theme::Oklab::from(to).lightness());
  const float high = std::max(theme::Oklab::from(from).lightness(),
                              theme::Oklab::from(to).lightness());
  EXPECT_GE(lightness, low - 0.02F);
  EXPECT_LE(lightness, high + 0.02F);
  const float corner = middle->shape[provider::ShapeVertex::radius];
  EXPECT_LT(corner, corner_from);
  EXPECT_GT(corner, 4.0F);
  for (int index = 0; index < 40; ++index) {
    step(application, std::chrono::milliseconds{16});
  }
  const auto after = fill(50.0F, 20.0F);
  ASSERT_TRUE(after.has_value());
  EXPECT_TRUE(close(colour_of(*after), to));
  EXPECT_NEAR(after->shape[provider::ShapeVertex::radius], 4.0F, 0.01F);
}

TEST_F(AppearanceRuntimeTest, ScopedOverridesResolveTheInnermostTheme) {
  const theme::Theme warm = warm_theme();
  Application application = start_with(
      components::HStack(
          components::ZStack()
              .frame({.width = 30.0F, .height = 30.0F})
              .background(theme::ColourRole::accent),
          components::VStack(components::ZStack()
                                 .frame({.width = 50.0F, .height = 50.0F})
                                 .background(theme::ColourRole::accent),
                             components::ZStack()
                                 .frame({.width = 70.0F, .height = 70.0F})
                                 .background(theme::ColourRole::accent)
                                 .theme_override(theme::Theme::defaults()))
              .theme_override(warm)),
      {});
  const auto outside = fill(15.0F, 15.0F);
  const auto scoped = fill(25.0F, 25.0F);
  const auto inner = fill(35.0F, 35.0F);
  ASSERT_TRUE(outside.has_value() && scoped.has_value() && inner.has_value());
  const theme::Colour standard =
      theme::Theme::defaults().palette(theme::ColourRole::accent).light;
  EXPECT_TRUE(close(colour_of(*outside), standard));
  EXPECT_TRUE(
      close(colour_of(*scoped), warm.palette(theme::ColourRole::accent).light));
  EXPECT_TRUE(close(colour_of(*inner), standard));
}

TEST_F(AppearanceRuntimeTest, ReduceMotionSwapsSpringsForQuickCrossFades) {
  appearance::AppearancePolicy policy;
  policy.defaults.reduce_motion = true;
  Application application = start_with(Swatch{}, policy);
  motion::with_animation(motion::Animation::smooth(0.6), [] {
    Swatch::handles().moved->set(true);
    Swatch::handles().warm->set(true);
  });
  frames(application, 1);
  step(application, std::chrono::milliseconds{16});
  const auto box = node_of(application, "box");
  ASSERT_TRUE(box.has_value());
  const composition::Rect moved =
      application.root().tree().node(*box)->get().presented()->bounds;
  const composition::Rect laid =
      application.root().tree().node(*box)->get().frame();
  EXPECT_NEAR(moved.x, laid.x + 200.0F, 0.5F);
  const auto fading = fill(50.0F, 20.0F);
  ASSERT_TRUE(fading.has_value());
  EXPECT_FALSE(close(colour_of(*fading),
                     warm_theme().palette(theme::ColourRole::accent).light));
  for (int index = 0; index < 20; ++index) {
    step(application, std::chrono::milliseconds{16});
  }
  EXPECT_TRUE(close(colour_of(*fill(50.0F, 20.0F)),
                    warm_theme().palette(theme::ColourRole::accent).light));
}

TEST_F(AppearanceRuntimeTest, IncreaseContrastUsesTheHighContrastColumns) {
  appearance::AppearancePolicy policy;
  policy.defaults.increase_contrast = true;
  policy.defaults.color_scheme = appearance::ColorSchemePreference::light;
  Application application = start_with(Swatch{}, policy);
  const theme::Theme standard = theme::Theme::defaults();
  const theme::AppearanceVariants variants =
      standard.variants(theme::ColourRole::gray);
  ASSERT_FALSE(close(variants.light, variants.high_contrast_light));
  const auto rule = fill(30.0F, 15.0F);
  ASSERT_TRUE(rule.has_value());
  EXPECT_TRUE(close(colour_of(*rule), variants.high_contrast_light));
  application.appearance()->update(
      [](appearance::AppearanceOverrides& o) { o.increase_contrast = false; });
  for (int index = 0; index < 30; ++index) {
    step(application, std::chrono::milliseconds{16});
  }
  EXPECT_TRUE(close(colour_of(*fill(30.0F, 15.0F)), variants.light));
}

TEST_F(AppearanceRuntimeTest, ReduceTransparencyMakesGlassOpaque) {
  Application application = start_with(Swatch{}, {});
  EXPECT_FALSE(fill(40.0F, 10.0F).has_value());
  EXPECT_TRUE(
      std::ranges::any_of(providers_.gpu().last_shapes().vertices,
                          [](const provider::ShapeVertex& vertex) {
                            return vertex.kind == provider::ShapeKind::glass &&
                                   std::abs(vertex.half_size[0] - 40.0F) < 0.6F;
                          }));
  application.appearance()->update(
      [](appearance::AppearanceOverrides& o) { o.reduce_transparency = true; });
  frames(application, 2);
  EXPECT_FLOAT_EQ(colour_of(*fill(40.0F, 10.0F)).alpha, 1.0F);
}

TEST_F(AppearanceRuntimeTest, TheUiScaleFollowsTheDisplayModeAndLaysOutOnce) {
  Application application = start_with(Swatch{}, {});
  ASSERT_TRUE(fill(50.0F, 20.0F).has_value());
  providers_.surface().set_display_mode(provider::DisplayMode::handheld);
  frames(application, 1);
  EXPECT_TRUE(application.last_frame().laid_out);
  int layouts = 0;
  for (int index = 0; index < 40; ++index) {
    step(application, std::chrono::milliseconds{16});
    layouts += application.last_frame().laid_out ? 1 : 0;
  }
  EXPECT_EQ(layouts, 0);
  const float scale = appearance::UiScales::handheld_default;
  EXPECT_TRUE(fill(50.0F * scale, 20.0F * scale).has_value());
  application.appearance()->update([](appearance::AppearanceOverrides& o) {
    o.ui_scale = o.ui_scale.with(provider::DisplayMode::handheld, 1.5F);
  });
  frames(application, 1);
  step(application, std::chrono::milliseconds{48});
  const auto zooming = providers_.gpu().last_shapes().vertices;
  bool between = false;
  for (std::size_t base = 0; base + 3 < zooming.size(); base += 4) {
    const provider::ShapeVertex& corner = zooming[base];
    const provider::ShapeVertex& opposite = zooming[base + 2];
    if (corner.kind != provider::ShapeKind::fill ||
        std::abs((corner.half_size[0] / corner.half_size[1]) - 2.5F) > 0.01F) {
      continue;
    }
    const float drawn = std::abs(opposite.position[0] - corner.position[0]);
    const float laid = corner.half_size[0] * 2.0F;
    if (std::abs(drawn - laid) > 1.0F && corner.half_size[0] > 75.0F - 0.5F) {
      between = true;
    }
  }
  EXPECT_TRUE(between);
  for (int index = 0; index < 40; ++index) {
    step(application, std::chrono::milliseconds{16});
  }
  EXPECT_TRUE(fill(75.0F, 30.0F).has_value());
  providers_.surface().set_display_mode(provider::DisplayMode::docked);
  frames(application, 1);
  for (int index = 0; index < 40; ++index) {
    step(application, std::chrono::milliseconds{16});
  }
  EXPECT_TRUE(fill(50.0F * appearance::UiScales::docked_default,
                   20.0F * appearance::UiScales::docked_default)
                  .has_value());
}

TEST_F(AppearanceRuntimeTest, DynamicTypeGrowsScaledMetricPadding) {
  appearance::AppearancePolicy policy;
  policy.defaults.dynamic_type_size = theme::DynamicTypeSize::xxx_large;
  Application application = start_with(
      components::VStack(
          components::ZStack()
              .frame({.width = theme::metric::sidebar_width, .height = 10.0F})
              .id("wide"),
          components::ZStack()
              .frame({.width = 20.0F, .height = 10.0F})
              .padding(theme::metric::button_horizontal_padding)
              .id("padded")),
      policy);
  EXPECT_NEAR(frame_of(application, "wide").width, 144.0F * 23.0F / 17.0F,
              0.5F);
  EXPECT_NEAR(frame_of(application, "padded").width,
              2.0F * 12.0F * 23.0F / 17.0F, 0.5F);
  application.appearance()->update([](appearance::AppearanceOverrides& o) {
    o.dynamic_type_size = theme::DynamicTypeSize::large;
  });
  for (int index = 0; index < 40; ++index) {
    step(application, std::chrono::milliseconds{16});
  }
  EXPECT_NEAR(frame_of(application, "wide").width, 144.0F, 0.5F);
}

}
