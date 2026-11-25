#include <arisa/components/button.hpp>
#include <arisa/components/button_role.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/control_metrics.hpp>
#include <arisa/components/control_size.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/label.hpp>
#include <arisa/components/share_destination.hpp>
#include <arisa/components/share_preview.hpp>
#include <arisa/components/system_symbols.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/binding.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/control_environment.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/menu_action_dismiss_behavior.hpp>
#include <arisa/composition/node.hpp>
#include <arisa/composition/pasteboard.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/composition/symbol_variants.hpp>
#include <arisa/composition/transfer_item.hpp>
#include <arisa/composition/visibility.hpp>
#include <arisa/controls/closed_range.hpp>
#include <arisa/controls/date_picker.hpp>
#include <arisa/controls/date_picker_style.hpp>
#include <arisa/controls/gauge.hpp>
#include <arisa/controls/gauge_style.hpp>
#include <arisa/controls/menu.hpp>
#include <arisa/controls/menu_style.hpp>
#include <arisa/controls/number_format.hpp>
#include <arisa/controls/palette_selection_effect.hpp>
#include <arisa/controls/picker.hpp>
#include <arisa/controls/picker_style.hpp>
#include <arisa/controls/progress_view.hpp>
#include <arisa/controls/progress_view_style.hpp>
#include <arisa/controls/secure_field.hpp>
#include <arisa/controls/share_link.hpp>
#include <arisa/controls/slider.hpp>
#include <arisa/controls/slider_tick.hpp>
#include <arisa/controls/slider_tick_content.hpp>
#include <arisa/controls/slider_tick_content_for_each.hpp>
#include <arisa/controls/stepper.hpp>
#include <arisa/controls/text_field.hpp>
#include <arisa/controls/text_field_link.hpp>
#include <arisa/controls/toggle.hpp>
#include <arisa/controls/toggle_style.hpp>
#include <arisa/controls/tuple_slider_tick_content.hpp>
#include <arisa/graphics/shape_style.hpp>
#include <arisa/provider/assets/asset_path.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/structure/form.hpp>
#include <arisa/structure/form_style.hpp>
#include <arisa/theme/colour_value.hpp>

#include <focus_fixture.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace arisa::runtime {

using composition::Element;
using provider::Button;

class ControlsParityTest : public FocusFixture {
 protected:
  enum class Shade : std::uint8_t { red, green, blue };

  static constexpr composition::StateKey<int> count_key{"parity-count", 0};
  static constexpr composition::StateKey<double> level_key{"parity-level", 0.5};
  static constexpr composition::StateKey<bool> first_key{"parity-first", true};
  static constexpr composition::StateKey<bool> second_key{"parity-second",
                                                          false};
  static constexpr composition::StateKey<Shade> left_key{"parity-left",
                                                         Shade::red};
  static constexpr composition::StateKey<Shade> right_key{"parity-right",
                                                          Shade::green};
  static constexpr composition::StateKey<std::string> secret_key{
      "parity-secret", ""};

  struct Probe {
    std::optional<composition::StateHandle<int>> count;
    std::optional<composition::StateHandle<double>> level;
    std::optional<composition::StateHandle<bool>> first;
    std::optional<composition::StateHandle<bool>> second;
    std::optional<composition::StateHandle<Shade>> left;
    std::optional<composition::StateHandle<Shade>> right;
    int actions{0};
    int primaries{0};
    std::vector<bool> editing;
    std::vector<bool> mixed;
    std::vector<std::vector<bool>> option_mixes;
    std::vector<std::string> submitted;
    std::vector<std::size_t> shared;
  };

  using Build = std::function<Element(Probe&, composition::Context&)>;

  class Host final : public composition::Component {
   public:
    Host(Probe* probe, const Build* build) noexcept
        : probe_(probe), build_(build) {}

    auto operator==(const Host&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      return (*build_)(*probe_, context);
    }

   private:
    Probe* probe_;
    const Build* build_;
  };

  struct MixProbeStyle {
    Probe* probe{nullptr};

    auto make_body(controls::ToggleStyle::Configuration configuration) const
        -> Element {
      probe->mixed.push_back(configuration.is_mixed);
      return components::Button(
                 [binding = configuration.is_on_binding] {
                   binding.set(!binding.get());
                 },
                 std::move(configuration.label))
          .id("mix-style");
    }

    friend auto operator==(const MixProbeStyle&, const MixProbeStyle&)
        -> bool = default;
  };

  struct OptionProbeStyle {
    Probe* probe{nullptr};

    auto make_body(controls::PickerStyle::Configuration configuration) const
        -> Element {
      std::vector<controls::PickerStyle::Option> options =
          configuration.options();
      std::vector<bool> mixes;
      std::vector<Element> rows;
      for (std::size_t index = 0; index < options.size(); ++index) {
        mixes.push_back(options[index].is_mixed);
        rows.push_back(
            components::Button([select = options[index].select] { select(); },
                               std::move(options[index].label))
                .id(std::string{"option-"} + std::to_string(index)));
      }
      probe->option_mixes.push_back(std::move(mixes));
      return components::VStack(std::move(configuration.label),
                                components::VStack(std::move(rows)));
    }

    friend auto operator==(const OptionProbeStyle&, const OptionProbeStyle&)
        -> bool = default;
  };

  static auto glyph(std::string_view name) -> std::string {
    const auto resolved =
        components::SystemSymbols::resolve(name, composition::SymbolVariants{});
    return resolved.has_value() ? resolved->base : std::string{};
  }

  static auto shades() -> Element {
    return components::Group(components::Text("Red").tag(Shade::red),
                             components::Text("Green").tag(Shade::green),
                             components::Text("Blue").tag(Shade::blue));
  }

  static void settle(provider::headless::HeadlessProviderFactory& providers,
                     Application& application) {
    for (int step = 0; step < 30; ++step) {
      ASSERT_TRUE(providers.clock().advance(frame_step).has_value());
      frames(application, 1);
    }
  }

  static void fonts(provider::headless::HeadlessProviderFactory& providers) {
    for (const std::string_view file :
         {std::string_view{"arisa/fonts/InterVariable.ttf"},
          std::string_view{"arisa/fonts/MaterialSymbolsRounded.ttf"},
          std::string_view{"arisa/fonts/MaterialSymbolsRoundedFilled.ttf"}}) {
      std::ifstream stream{std::filesystem::path{ARISA_SOURCE_ASSETS} / file,
                           std::ios::binary};
      const std::vector<char> raw{std::istreambuf_iterator<char>{stream},
                                  std::istreambuf_iterator<char>{}};
      std::vector<std::byte> bytes(raw.size());
      std::ranges::transform(raw, bytes.begin(),
                             [](char value) { return std::byte(value); });
      ASSERT_FALSE(bytes.empty()) << file;
      providers.assets().add(*provider::AssetPath::create(file), bytes);
    }
  }

  static auto frame_of(Application& application, std::string_view key)
      -> composition::Rect {
    const auto node = node_named(application, key);
    return node.has_value() ? node->get().frame() : composition::Rect{};
  }
};

TEST_F(ControlsParityTest, ButtonRolesAndSymbolFormsBuildTheirLabels) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe& seen, composition::Context&) {
    Probe* target = &seen;
    return components::VStack(
        components::Button(components::ButtonRole::close,
                           [target] { ++target->actions; })
            .id("close"),
        components::Button(components::ButtonRole::confirm,
                           [target] { target->actions += 10; })
            .id("confirm"),
        components::Button("Add", components::Image::SystemName{"plus"},
                           [target] { target->actions += 100; })
            .id("add"),
        components::Button("Remove", components::Image::SystemName{"minus"},
                           components::ButtonRole::destructive, [] {}),
        components::Button("Badge", components::Image::Resource{"badge"},
                           [] {}),
        components::Button("Saved", components::Image::Resource{"badge"},
                           components::ButtonRole::confirm, [] {}));
  };
  Application application = start(providers, Host{&probe, &build});
  EXPECT_TRUE(shows(application, glyph("xmark")));
  EXPECT_TRUE(shows(application, glyph("checkmark")));
  EXPECT_TRUE(shows(application, glyph("plus")));
  EXPECT_TRUE(shows(application, glyph("minus")));
  EXPECT_TRUE(shows(application, "Add"));
  EXPECT_TRUE(shows(application, "Badge"));
  EXPECT_TRUE(shows(application, "Saved"));
  ASSERT_TRUE(application.root().activate(composition::ElementKey{"close"}));
  ASSERT_TRUE(application.root().activate(composition::ElementKey{"confirm"}));
  ASSERT_TRUE(application.root().activate(composition::ElementKey{"add"}));
  frames(application, 2);
  EXPECT_EQ(probe.actions, 111);
  const composition::Rect close = frame_of(application, "close");
  EXPECT_FLOAT_EQ(close.width, close.height);
}

TEST_F(ControlsParityTest, AccessoryLinkAndCardButtonStylesReachTheirBodies) {
  provider::headless::HeadlessProviderFactory providers;
  fonts(providers);
  Probe probe;
  const Build build = [](Probe&, composition::Context&) {
    return components::VStack(
        components::HStack(components::Text("Filter").fixed_size().id("plain")),
        components::HStack(
            components::Button("Filter", [] {})
                .button_style(components::AccessoryBarButtonStyle{})
                .fixed_size()
                .id("accessory")),
        components::HStack(
            components::Button("Filter", [] {})
                .button_style(components::AccessoryBarActionButtonStyle{})
                .fixed_size()
                .id("action")),
        components::HStack(components::Button("Filter", [] {})
                               .button_style(components::LinkButtonStyle{})
                               .fixed_size()
                               .id("link")),
        components::HStack(components::Button("Filter", [] {})
                               .button_style(components::CardButtonStyle{})
                               .fixed_size()
                               .id("card")));
  };
  Application application = start(providers, Host{&probe, &build});
  const float bare = frame_of(application, "plain").width;
  const float tall = frame_of(application, "plain").height;
  for (const std::string_view key : {"accessory", "action"}) {
    EXPECT_FLOAT_EQ(frame_of(application, key).width, bare + 24.0F) << key;
    EXPECT_FLOAT_EQ(frame_of(application, key).height, tall + 8.0F) << key;
  }
  EXPECT_FLOAT_EQ(frame_of(application, "link").width, bare);
  EXPECT_FLOAT_EQ(frame_of(application, "card").width,
                  bare + (2.0F * components::ButtonStyle::card_padding));
  EXPECT_FALSE(components::ButtonStyle::link() ==
               components::ButtonStyle::card());
  EXPECT_TRUE(components::ButtonStyle::accessory_bar() ==
              components::ButtonStyle{components::AccessoryBarButtonStyle{}});
}

TEST_F(ControlsParityTest, LabelsTakeSymbolsSpacingAndAReservedIconWidth) {
  provider::headless::HeadlessProviderFactory providers;
  fonts(providers);
  Probe probe;
  const Build build = [](Probe&, composition::Context&) {
    return components::VStack(
        components::HStack(
            components::Label("Star", components::Image::SystemName{"star"})
                .fixed_size()
                .id("plain")),
        components::HStack(
            components::Label("Star", components::Image::SystemName{"star"})
                .label_icon_to_title_spacing(20.0F)
                .fixed_size()
                .id("spaced")),
        components::HStack(
            components::Label("Star", components::Image::SystemName{"star"})
                .label_icon_to_title_spacing(8.0F)
                .fixed_size()
                .id("measured")),
        components::HStack(
            components::Label("Star", components::Image::SystemName{"star"})
                .label_reserved_icon_width(40.0F)
                .fixed_size()
                .id("reserved")),
        components::Label("Asset", components::Image::Resource{"badge"}));
  };
  Application application = start(providers, Host{&probe, &build});
  EXPECT_TRUE(shows(application, glyph("star")));
  EXPECT_TRUE(shows(application, "Asset"));
  const float plain = frame_of(application, "plain").width;
  const float spaced = frame_of(application, "spaced").width;
  const float reserved = frame_of(application, "reserved").width;
  EXPECT_FLOAT_EQ(frame_of(application, "measured").width, plain);
  EXPECT_FLOAT_EQ(spaced - plain, 12.0F);
  EXPECT_GT(reserved, plain);
}

TEST_F(ControlsParityTest, MenuPrimaryActionRunsOnSelectAndOpensOnPlus) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe& seen, composition::Context&) {
    Probe* target = &seen;
    const auto entries = [target] {
      return Element{components::Group(
          components::Button("Duplicate", [target] { ++target->actions; }),
          components::Button("Keep", [target] { target->actions += 10; })
              .menu_action_dismiss_behavior(
                  composition::MenuActionDismissBehavior::disabled))};
    };
    return components::VStack(
        components::HStack(
            controls::Menu("Edit", entries, [target] { ++target->primaries; })
                .id("split")),
        components::HStack(controls::Menu(
            "Shared", components::Image::SystemName{"share"}, entries)),
        components::HStack(controls::Menu("Pull", entries)
                               .menu_style(controls::MenuStyle::pull_down())));
  };
  Application application = start(providers, Host{&probe, &build});
  EXPECT_TRUE(shows(application, glyph("share")));
  press(providers, application, Button::a);
  settle(providers, application);
  EXPECT_EQ(probe.primaries, 1);
  EXPECT_FALSE(shows(application, "Duplicate"));
  press(providers, application, Button::plus);
  settle(providers, application);
  ASSERT_TRUE(shows(application, "Keep"));
  EXPECT_EQ(probe.primaries, 1);
  press(providers, application, Button::dpad_down);
  press(providers, application, Button::a);
  settle(providers, application);
  EXPECT_EQ(probe.actions, 10);
  EXPECT_TRUE(shows(application, "Keep"));
  EXPECT_TRUE(controls::MenuStyle::pull_down().pulls_down());
  EXPECT_TRUE(controls::MenuStyle::borderless_pull_down().pulls_down());
  EXPECT_FALSE(controls::MenuStyle::button().pulls_down());
}

TEST_F(ControlsParityTest, TogglesTakeSymbolsAndMixedSources) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe& seen, composition::Context& context) {
    const auto first = context.state(first_key);
    const auto second = context.state(second_key);
    seen.first = first;
    seen.second = second;
    return components::VStack(
        controls::Toggle("Wi-Fi", components::Image::SystemName{"wifi"},
                         composition::Binding<bool>{first}),
        controls::Toggle("All",
                         std::vector<composition::Binding<bool>>{
                             composition::Binding<bool>{first},
                             composition::Binding<bool>{second}})
            .id("all"),
        controls::Toggle(
            std::vector<composition::Binding<bool>>{
                composition::Binding<bool>{first},
                composition::Binding<bool>{second}},
            components::Text("Styled"))
            .toggle_style(MixProbeStyle{&seen}));
  };
  Application application = start(providers, Host{&probe, &build});
  EXPECT_TRUE(shows(application, glyph("wifi")));
  ASSERT_FALSE(probe.mixed.empty());
  EXPECT_TRUE(probe.mixed.back());
  EXPECT_TRUE(controls::Toggle::is_mixed(
      {composition::Binding<bool>::constant(true),
       composition::Binding<bool>::constant(false)}));
  ASSERT_TRUE(application.root().activate(composition::ElementKey{"all"}));
  frames(application, 2);
  EXPECT_TRUE(probe.first->get());
  EXPECT_TRUE(probe.second->get());
  EXPECT_FALSE(probe.mixed.back());
  ASSERT_TRUE(application.root().activate(composition::ElementKey{"all"}));
  frames(application, 2);
  EXPECT_FALSE(probe.first->get());
  EXPECT_FALSE(probe.second->get());
  ASSERT_TRUE(
      application.root().activate(composition::ElementKey{"mix-style"}));
  frames(application, 2);
  EXPECT_TRUE(probe.first->get());
  EXPECT_TRUE(probe.second->get());
}

TEST_F(ControlsParityTest, PickersTakeLabelViewsSymbolsAndMixedSources) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe& seen, composition::Context& context) {
    const auto left = context.state(left_key);
    const auto right = context.state(right_key);
    seen.left = left;
    seen.right = right;
    return components::VStack(
        controls::Picker(composition::Binding<Shade>{left}, shades,
                         components::Text("Viewed")),
        controls::Picker("Tinted", components::Image::SystemName{"palette"},
                         composition::Binding<Shade>{left}, shades),
        controls::Picker("Asset", components::Image::Resource{"badge"},
                         composition::Binding<Shade>{left}, shades),
        controls::Picker("Both",
                         std::vector<composition::Binding<Shade>>{
                             composition::Binding<Shade>{left},
                             composition::Binding<Shade>{right}},
                         shades)
            .picker_style(OptionProbeStyle{&seen}),
        controls::Picker("Symbol", components::Image::SystemName{"tag"},
                         std::vector<composition::Binding<Shade>>{
                             composition::Binding<Shade>{left}},
                         shades),
        controls::Picker("Resource", components::Image::Resource{"badge"},
                         std::vector<composition::Binding<Shade>>{
                             composition::Binding<Shade>{left}},
                         shades),
        controls::Picker(
            std::vector<composition::Binding<Shade>>{
                composition::Binding<Shade>{right}},
            shades, components::Text("Sources")));
  };
  Application application = start(providers, Host{&probe, &build});
  EXPECT_TRUE(shows(application, "Viewed"));
  EXPECT_TRUE(shows(application, "Sources"));
  EXPECT_TRUE(shows(application, glyph("palette")));
  ASSERT_FALSE(probe.option_mixes.empty());
  EXPECT_EQ(probe.option_mixes.back(), (std::vector<bool>{true, true, false}));
  ASSERT_TRUE(application.root().activate(composition::ElementKey{"option-2"}));
  frames(application, 2);
  EXPECT_EQ(probe.left->get(), Shade::blue);
  EXPECT_EQ(probe.right->get(), Shade::blue);
  EXPECT_EQ(probe.option_mixes.back(),
            (std::vector<bool>{false, false, false}));
}

TEST_F(ControlsParityTest, TabsPickerIsWiderThanSegmentedByTheMeasuredGap) {
  provider::headless::HeadlessProviderFactory providers;
  fonts(providers);
  Probe probe;
  const Build build = [](Probe& seen, composition::Context& context) {
    const auto left = context.state(left_key);
    seen.left = left;
    const auto three = [] {
      return Element{
          components::Group(components::Text("One").tag(Shade::red),
                            components::Text("Two").tag(Shade::green),
                            components::Text("Three").tag(Shade::blue))};
    };
    return components::VStack(
        components::HStack(
            controls::Picker("Picker", composition::Binding<Shade>{left}, three)
                .picker_style(controls::PickerStyle::tabs())
                .labels_hidden()
                .fixed_size()
                .id("tabs")),
        components::HStack(
            controls::Picker("Picker", composition::Binding<Shade>{left}, three)
                .picker_style(controls::PickerStyle::segmented())
                .labels_hidden()
                .fixed_size()
                .id("segmented")));
  };
  Application application = start(providers, Host{&probe, &build});
  const float tabs = frame_of(application, "tabs").width;
  const float segmented = frame_of(application, "segmented").width;
  EXPECT_FLOAT_EQ(tabs - segmented,
                  3.0F * controls::PickerStyle::tabs_segment_padding);
  EXPECT_FLOAT_EQ(195.0F - 180.0F,
                  3.0F * controls::PickerStyle::tabs_segment_padding);
}

TEST_F(ControlsParityTest, PaletteSelectionEffectSwapsTheRingForAVariant) {
  using Effect = controls::PaletteSelectionEffect;
  EXPECT_EQ(Effect::automatic.kind(), Effect::Kind::automatic);
  EXPECT_EQ(Effect::custom.kind(), Effect::Kind::custom);
  const Effect filled =
      Effect::symbol_variant(composition::SymbolVariants::fill);
  EXPECT_EQ(filled.kind(), Effect::Kind::symbol_variant);
  EXPECT_EQ(filled.variant(), composition::SymbolVariants::fill);
  EXPECT_FALSE(filled == Effect::automatic);
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [filled](Probe& seen, composition::Context& context) {
    const auto left = context.state(left_key);
    seen.left = left;
    const auto stars = [] {
      return Element{components::Group(
          components::Image::system_name("star").tag(Shade::red),
          components::Image::system_name("heart").tag(Shade::green))};
    };
    return components::VStack(
        controls::Picker("Mark", composition::Binding<Shade>{left}, stars)
            .picker_style(controls::PickerStyle::palette())
            .palette_selection_effect(filled));
  };
  Application application = start(providers, Host{&probe, &build});
  const auto chosen = components::SystemSymbols::resolve(
      "star", composition::SymbolVariants::fill);
  ASSERT_TRUE(chosen.has_value());
  EXPECT_TRUE(shows(application, chosen->base));
  EXPECT_TRUE(shows(application, glyph("heart")));
}

TEST_F(ControlsParityTest, SliderTickContentBuildsFromTuplesAndForEach) {
  const controls::SliderTick half{0.5};
  EXPECT_EQ(half.id(), controls::SliderTick::ID{0.5});
  EXPECT_TRUE(controls::SliderTick{0.25} < half);
  EXPECT_EQ(half.body().size(), 1U);
  const controls::SliderTick titled{std::string{"Mid"}, 0.5F};
  ASSERT_TRUE(titled.labelled().has_value());
  const controls::SliderTick drawn{
      0.75, [] { return Element{components::Text("Hi")}; }};
  EXPECT_TRUE(drawn.labelled().has_value());
  const controls::TupleSliderTickContent tuple{controls::SliderTick{0.0},
                                               controls::SliderTick{1.0}, half};
  EXPECT_EQ(tuple.value().size(), 3U);
  const std::vector<double> stops{0.0, 0.25, 0.25, 0.5};
  const controls::SliderTickContentForEach each{
      stops, [](double at) { return controls::SliderTick{at}; }};
  EXPECT_EQ(each.body().size(), 4U);
  const controls::SliderTickContentForEach unique{
      stops, [](double at) { return at; },
      [](double at) { return controls::SliderTick{at}; }};
  EXPECT_EQ(unique.body().size(), 3U);
  const controls::SliderTickContent from_tuple{tuple};
  EXPECT_EQ(from_tuple.sorted().front().value(), 0.0);
  EXPECT_EQ(from_tuple.sorted().back().value(), 1.0);
}

TEST_F(ControlsParityTest, SlidersTakeLabelsTicksNeutralAndEnabledBounds) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe& seen, composition::Context& context) {
    const auto level = context.state(level_key);
    seen.level = level;
    Probe* target = &seen;
    return components::VStack(
        controls::Slider(
            composition::Binding<double>{level},
            controls::ClosedRange<double>{.lower_bound = 0.0,
                                          .upper_bound = 1.0},
            controls::Slider::Parts{
                .step = 0.1,
                .neutral_value = 0.5,
                .enabled_bounds =
                    controls::ClosedRange<double>{.lower_bound = 0.2,
                                                  .upper_bound = 0.7},
                .label = components::Text("Balance"),
                .current_value_label = components::Text("Now"),
                .minimum_value_label = components::Text("L"),
                .maximum_value_label = components::Text("R"),
                .ticks =
                    controls::TupleSliderTickContent{
                        controls::SliderTick{std::string{"Low"}, 0.0},
                        controls::SliderTick{std::string{"High"}, 1.0}},
                .tick = [](double at) -> std::optional<controls::SliderTick> {
                  return at > 0.45 && at < 0.55
                             ? std::optional{controls::SliderTick{at}}
                             : std::nullopt;
                },
                .on_editing_changed =
                    [target](bool editing) {
                      target->editing.push_back(editing);
                    }})
            .frame({.width = 400.0F})
            .id("balance"),
        controls::Slider(composition::Binding<double>{level},
                         controls::ClosedRange<double>{.lower_bound = 0.0,
                                                       .upper_bound = 1.0},
                         components::Text("Plain"))
            .slider_thumb_visibility(composition::Visibility::hidden)
            .id("plain"),
        controls::Slider(composition::Binding<double>{level},
                         controls::ClosedRange<double>{.lower_bound = 0.0,
                                                       .upper_bound = 1.0},
                         0.25, components::Text("Stepped")));
  };
  Application application = start(providers, Host{&probe, &build});
  for (const std::string_view text :
       {"Balance", "Now", "L", "R", "Low", "High", "Plain", "Stepped"}) {
    EXPECT_TRUE(shows(application, text)) << text;
  }
  const auto balance = node_named(application, "balance");
  ASSERT_TRUE(balance.has_value());
  const auto content = balance->get().content_as<controls::Slider::Content>();
  ASSERT_TRUE(content.has_value());
  EXPECT_EQ(content->get().ticks().size(), 3U);
  for (int step = 0; step < 6; ++step) {
    press(providers, application, Button::dpad_right);
  }
  EXPECT_LE(probe.level->get(), 0.7 + 1e-9);
  EXPECT_FALSE(probe.editing.empty());
  EXPECT_EQ(probe.editing.front(), true);
  EXPECT_EQ(probe.editing.back(), false);
}

TEST_F(ControlsParityTest, SteppersFormatStayUnboundedAndTakeLabelViews) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe& seen, composition::Context& context) {
    const auto count = context.state(count_key);
    seen.count = count;
    Probe* target = &seen;
    const composition::Binding<int> bound{count};
    const auto changed = [target](bool editing) {
      target->editing.push_back(editing);
    };
    return components::VStack(
        controls::Stepper(
            "Bounded", bound,
            controls::ClosedRange<int>{.lower_bound = 0, .upper_bound = 3}, 1,
            changed)
            .id("bounded"),
        controls::Stepper("Free", bound, 5).id("free"),
        controls::Stepper(
            "Formatted", bound,
            controls::ClosedRange<int>{.lower_bound = 0, .upper_bound = 100}, 1,
            controls::NumberFormat::number()),
        controls::Stepper("Open format", bound, 2,
                          controls::NumberFormat::number()),
        controls::Stepper(
            bound,
            controls::ClosedRange<int>{.lower_bound = 0, .upper_bound = 100}, 1,
            components::Text("Viewed")),
        controls::Stepper(bound, 1, components::Text("Open view")),
        controls::Stepper(
            bound,
            controls::ClosedRange<int>{.lower_bound = 0, .upper_bound = 100}, 1,
            controls::NumberFormat::number(), components::Text("Format view")),
        controls::Stepper(bound, 1, controls::NumberFormat::number(),
                          components::Text("Open format view")),
        controls::Stepper(
            components::Text("Callbacks"), [target] { ++target->actions; },
            [target] { --target->actions; }));
  };
  Application application = start(providers, Host{&probe, &build});
  for (const std::string_view text :
       {"Bounded", "Free", "Formatted", "Open format", "Viewed", "Open view",
        "Format view", "Open format view", "Callbacks"}) {
    EXPECT_TRUE(shows(application, text)) << text;
  }
  EXPECT_TRUE(shows(application, "0"));
  probe.count->set(3);
  frames(application, 2);
  EXPECT_TRUE(shows(application, "3"));
  EXPECT_EQ(controls::Stepper::unbounded<int>().upper_bound,
            std::numeric_limits<int>::max());
}

TEST_F(ControlsParityTest, ProgressViewsTakeLabelViewsCurrentValuesAndTimers) {
  using Clock = std::chrono::system_clock;
  const Clock::time_point origin = Clock::time_point{std::chrono::seconds{100}};
  const controls::ProgressView::TimerInterval interval{
      .lower_bound = origin, .upper_bound = origin + std::chrono::seconds{60}};
  EXPECT_DOUBLE_EQ(controls::ProgressView::timed(
                       interval, true, origin + std::chrono::seconds{15}),
                   0.75);
  EXPECT_DOUBLE_EQ(controls::ProgressView::timed(
                       interval, false, origin + std::chrono::seconds{15}),
                   0.25);
  EXPECT_EQ(controls::ProgressView::remaining(
                interval, true, origin + std::chrono::seconds{15}),
            "0:45");
  EXPECT_EQ(controls::ProgressView::remaining(
                interval, false, origin + std::chrono::seconds{75}),
            "1:00");
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [interval](Probe&, composition::Context&) {
    return components::VStack(
        controls::ProgressView(components::Text("Labelled")),
        controls::ProgressView(0.5, 1.0, components::Text("Halfway")),
        controls::ProgressView(0.5, 1.0, components::Text("Copying"),
                               components::Text("5 of 10")),
        controls::ProgressView(interval),
        controls::ProgressView(interval, false, components::Text("Timer")),
        controls::ProgressView(interval, true, components::Text("Countdown"),
                               components::Text("Soon")));
  };
  Application application = start(providers, Host{&probe, &build});
  for (const std::string_view text :
       {"Labelled", "Halfway", "Copying", "5 of 10", "Timer", "Countdown",
        "Soon"}) {
    EXPECT_TRUE(shows(application, text)) << text;
  }
}

TEST_F(ControlsParityTest, ControlSizeReachesProgressTextFieldsAndMenus) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe&, composition::Context& context) {
    const auto secret = context.state(secret_key);
    return components::VStack(
        components::HStack(controls::ProgressView()
                               .control_size(components::ControlSize::mini)
                               .id("mini-spinner")),
        components::HStack(controls::ProgressView().id("regular-spinner")),
        controls::TextField("Mini", composition::Binding<std::string>{secret})
            .control_size(components::ControlSize::mini)
            .id("mini-field"),
        controls::TextField("Regular",
                            composition::Binding<std::string>{secret})
            .id("regular-field"));
  };
  Application application = start(providers, Host{&probe, &build});
  EXPECT_FLOAT_EQ(frame_of(application, "mini-spinner").width, 10.0F);
  EXPECT_FLOAT_EQ(frame_of(application, "regular-spinner").width, 32.0F);
  EXPECT_LT(frame_of(application, "mini-field").height,
            frame_of(application, "regular-field").height);
  EXPECT_FLOAT_EQ(components::ControlMetrics::of(components::ControlSize::small)
                      .text_field_height,
                  21.0F);
}

TEST_F(ControlsParityTest, SecureAndTextFieldsShowLabelViewsInForms) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe&, composition::Context& context) {
    const auto secret = context.state(secret_key);
    return structure::Form(controls::SecureField(
                               composition::Binding<std::string>{secret},
                               controls::TextFieldOptions{.prompt = "Required"},
                               components::Text("Password")),
                           controls::TextField(
                               composition::Binding<std::string>{secret},
                               controls::TextFieldOptions{.prompt = "Optional"},
                               components::Text("Nickname")))
        .form_style(structure::FormStyle::columns());
  };
  Application application = start(providers, Host{&probe, &build});
  EXPECT_TRUE(shows(application, "Password"));
  EXPECT_TRUE(shows(application, "Nickname"));
  EXPECT_TRUE(shows(application, "Required"));
}

TEST_F(ControlsParityTest, ShareLinkOffersCopyAndAppDestinations) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe& seen, composition::Context&) {
    Probe* target = &seen;
    return components::VStack(
        controls::ShareLink(
            std::string{"Share note"}, std::string{"hello"},
            controls::ShareLinkOptions{
                .subject = "Subject line",
                .message = "Body text",
                .preview =
                    components::SharePreview{
                        "Note", components::Image::system_name("doc")}})
            .share_destination(components::ShareDestination{
                "Album", components::Image::SystemName{"photo"},
                [target](const components::ShareDestination::Request& sent) {
                  target->shared.push_back(sent.items.size());
                }})
            .id("share"),
        controls::ShareLink(std::vector<std::string>{"a", "b"}),
        controls::ShareLink(std::string{"x"}, controls::ShareLinkOptions{},
                            components::Text("Custom label")));
  };
  Application application = start(providers, Host{&probe, &build});
  EXPECT_TRUE(shows(application, "Share note"));
  EXPECT_TRUE(shows(application, "Custom label"));
  EXPECT_TRUE(shows(application, glyph("square.and.arrow.up")));
  press(providers, application, Button::a);
  settle(providers, application);
  ASSERT_TRUE(shows(application, "Copy"));
  EXPECT_TRUE(shows(application, "Album"));
  EXPECT_TRUE(shows(application, "Subject line"));
  EXPECT_TRUE(shows(application, "Note"));
  press(providers, application, Button::a);
  settle(providers, application);
  EXPECT_FALSE(shows(application, "Album"));
  const auto copied = providers.clipboard().read_text();
  ASSERT_TRUE(copied.has_value());
  EXPECT_EQ(*copied, "hello");
  press(providers, application, Button::a);
  settle(providers, application);
  press(providers, application, Button::dpad_down);
  press(providers, application, Button::a);
  settle(providers, application);
  ASSERT_EQ(probe.shared.size(), 1U);
  EXPECT_EQ(probe.shared.front(), 1U);
}

TEST_F(ControlsParityTest, TextFieldLinkSubmitsTheEnteredText) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe& seen, composition::Context&) {
    Probe* target = &seen;
    return components::VStack(
        controls::TextFieldLink("Rename", std::string{"New name"},
                                [target](std::string text) {
                                  target->submitted.push_back(std::move(text));
                                })
            .id("rename"),
        controls::TextFieldLink(std::nullopt, components::Text("Tag"),
                                [](std::string) {}));
  };
  Application application = start(providers, Host{&probe, &build});
  EXPECT_TRUE(shows(application, "Rename"));
  EXPECT_TRUE(shows(application, "Tag"));
  providers.text_input().push(std::string{"Kyogre"});
  press(providers, application, Button::a);
  frames(application, 8);
  ASSERT_EQ(probe.submitted.size(), 1U);
  EXPECT_EQ(probe.submitted.front(), "Kyogre");
  ASSERT_EQ(providers.text_input().requests().size(), 1U);
  EXPECT_EQ(providers.text_input().requests().front().hint, "New name");
}

TEST_F(ControlsParityTest, DatePickerFieldStylesShowAndStepTheDate) {
  using Field = controls::DatePickerStyle::Field;
  const std::chrono::sys_seconds noon{std::chrono::hours{12}};
  EXPECT_EQ(Field::stepped(noon, controls::DatePickerComponents::date, 1),
            noon + std::chrono::days{1});
  EXPECT_EQ(
      Field::stepped(noon, controls::DatePickerComponents::hour_and_minute, -1),
      noon - std::chrono::minutes{1});
  EXPECT_FALSE(controls::DatePickerStyle::field() ==
               controls::DatePickerStyle::stepper_field());
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [noon](Probe&, composition::Context&) {
    return components::VStack(
        controls::DatePicker(
            "Due",
            composition::Binding<std::chrono::sys_seconds>::constant(noon))
            .date_picker_style(controls::DatePickerStyle::field()),
        controls::DatePicker(
            "Start",
            composition::Binding<std::chrono::sys_seconds>::constant(noon))
            .date_picker_style(controls::DatePickerStyle::stepper_field()));
  };
  Application application = start(providers, Host{&probe, &build});
  EXPECT_TRUE(shows(application, "Due"));
  EXPECT_TRUE(shows(application, "Start"));
  EXPECT_TRUE(node_of(application, Field::field_key).has_value());
  EXPECT_TRUE(node_of(application, controls::Stepper::raise_key).has_value());
}

TEST_F(ControlsParityTest, CircularGaugeTakesATintAndSizesListEveryCase) {
  EXPECT_FALSE(controls::GaugeStyle::accessory_circular(
                   graphics::ShapeStyle{theme::ColourRole::green}) ==
               controls::GaugeStyle::accessory_circular());
  EXPECT_TRUE(controls::GaugeStyle::accessory_circular(
                  graphics::ShapeStyle{theme::ColourRole::green}) ==
              controls::GaugeStyle::accessory_circular(
                  graphics::ShapeStyle{theme::ColourRole::green}));
  EXPECT_EQ(components::all_control_sizes.size(), 5U);
  EXPECT_EQ(components::all_control_sizes.front(),
            components::ControlSize::mini);
  EXPECT_EQ(components::all_control_sizes.back(),
            components::ControlSize::extra_large);
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe&, composition::Context&) {
    return components::VStack(
        controls::Gauge(0.4, components::Text("Battery"))
            .gauge_style(controls::GaugeStyle::accessory_circular(
                graphics::ShapeStyle{theme::ColourRole::green})));
  };
  Application application = start(providers, Host{&probe, &build});
  EXPECT_TRUE(shows(application, "Battery"));
}

}
