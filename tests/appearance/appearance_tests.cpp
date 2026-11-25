#include <arisa/appearance/appearance_file.hpp>
#include <arisa/appearance/appearance_model.hpp>
#include <arisa/appearance/appearance_policy.hpp>
#include <arisa/appearance/theme_file.hpp>
#include <arisa/appearance/theme_library.hpp>
#include <arisa/appearance/theme_vocabulary.hpp>
#include <arisa/provider/headless/memory_assets.hpp>
#include <arisa/provider/headless/memory_storage.hpp>
#include <arisa/provider/headless/recording_diagnostics.hpp>
#include <arisa/theme/dynamic_type.hpp>
#include <arisa/theme/oklab.hpp>
#include <arisa/theme/theme.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace arisa::appearance {

class AppearanceTest : public ::testing::Test {
 protected:
  static auto fixture(std::string_view name) -> std::string {
    std::ifstream stream{std::filesystem::path{ARISA_THEME_FIXTURES} /
                         std::string{name}};
    return std::string{std::istreambuf_iterator<char>{stream},
                       std::istreambuf_iterator<char>{}};
  }

  static auto path(std::string_view text) -> provider::AssetPath {
    auto created = provider::AssetPath::create(text);
    EXPECT_TRUE(created.has_value());
    return std::move(*created);
  }

  void install(std::string_view name) {
    storage_.add(path(std::string{"themes/"} + std::string{name} + ".toml"),
                 fixture(std::string{name} + ".toml"));
  }

  auto library() -> ThemeLibrary {
    return ThemeLibrary{assets_, std::ref(storage_),
                        std::vector<std::string>{"pack"}};
  }

  auto load(std::string_view name) -> std::expected<theme::Theme, ThemeError> {
    install(name);
    return library().load(name);
  }

  provider::headless::MemoryAssets assets_;
  provider::headless::MemoryStorage storage_;
  provider::headless::RecordingDiagnostics diagnostics_;
};

TEST_F(AppearanceTest, ThemeFilesSetEveryTokenGroup) {
  install("base");
  auto loaded = load("child");
  ASSERT_TRUE(loaded.has_value()) << loaded.error().description();
  const theme::Theme& child = *loaded;
  EXPECT_EQ(child.name(), "child");
  EXPECT_EQ(child.seed().accent, theme::Colour::hex(0xFF9500));
  EXPECT_FLOAT_EQ(child.radius(theme::radius::card), 20.0F);
  EXPECT_FLOAT_EQ(child.metric(theme::MetricToken::button_horizontal_padding),
                  16.0F);
  EXPECT_FLOAT_EQ(child.space(theme::space::md), 14.0F);
  EXPECT_FLOAT_EQ(child.type(theme::TextRole::body).size, 15.0F);
  EXPECT_FLOAT_EQ(child.type(theme::TextRole::body).line_height, 19.0F);
  EXPECT_EQ(child.palette(theme::ColourRole::label).light,
            theme::Colour::hex(0x102030));
  EXPECT_EQ(child.palette(theme::ColourRole::label).dark,
            theme::Colour::hex(0xE0E0F0));
  EXPECT_EQ(child.palette(theme::ColourRole::separator).light,
            theme::Colour::hex(0x808080, 128.0F / 255.0F));
  ASSERT_TRUE(child.glass().sidebar.has_value());
  EXPECT_EQ(child.glass().sidebar->variant(), theme::Glass::Variant::clear);
  EXPECT_TRUE(child.glass().sidebar->is_interactive());
  EXPECT_EQ(child.glass().sidebar->tint_colour(),
            std::optional<theme::ColourValue>{theme::ColourRole::accent});
  EXPECT_FALSE(child.glass().toolbar.has_value());
  EXPECT_EQ(child.preferences().dynamic_type_size,
            theme::DynamicTypeSize::x_large);
  EXPECT_EQ(child.preferences().handheld_scale, 1.5F);
}

TEST_F(AppearanceTest, ExtendsInheritsAndDerivesTheAccentFamily) {
  auto base = load("base");
  ASSERT_TRUE(base.has_value());
  const theme::Theme reference =
      theme::Theme::from(theme::Seed{.accent = theme::Colour::hex(0xFF9500)});
  EXPECT_EQ(base->palette(theme::ColourRole::accent_pressed),
            reference.palette(theme::ColourRole::accent_pressed));
  EXPECT_EQ(base->palette(theme::ColourRole::label),
            theme::Theme::defaults().palette(theme::ColourRole::label));
  EXPECT_FLOAT_EQ(base->radius(theme::radius::sheet),
                  theme::Theme::defaults().radius(theme::radius::sheet));
}

TEST_F(AppearanceTest, ErrorsCarryFileLineAndKeyPath) {
  install("base");
  const auto misspelt = load("misspelt");
  ASSERT_FALSE(misspelt.has_value());
  EXPECT_EQ(misspelt.error().code(), ThemeErrorCode::unknown_key);
  EXPECT_EQ(misspelt.error().file(), "themes/misspelt.toml");
  EXPECT_EQ(misspelt.error().line(), 6U);
  EXPECT_EQ(misspelt.error().key(), "colours.labell");
  EXPECT_EQ(misspelt.error().description(),
            "themes/misspelt.toml:6: colours.labell: not a semantic colour "
            "role");
  const auto colour = load("bad_colour");
  ASSERT_FALSE(colour.has_value());
  EXPECT_EQ(colour.error().code(), ThemeErrorCode::invalid_value);
  EXPECT_EQ(colour.error().line(), 8U);
  EXPECT_EQ(colour.error().key(), "colours.accent");
  const auto syntax = load("syntax");
  ASSERT_FALSE(syntax.has_value());
  EXPECT_EQ(syntax.error().code(), ThemeErrorCode::syntax);
  EXPECT_EQ(syntax.error().line(), 3U);
}

TEST_F(AppearanceTest, SchemaCycleAndMissingErrorsCarryTheirCode) {
  install("base");
  const auto future = load("future");
  ASSERT_FALSE(future.has_value());
  EXPECT_EQ(future.error().code(), ThemeErrorCode::unsupported_schema);
  EXPECT_EQ(future.error().key(), "schema");
  install("cycle_b");
  const auto cycle = load("cycle_a");
  ASSERT_FALSE(cycle.has_value());
  EXPECT_EQ(cycle.error().code(), ThemeErrorCode::cyclic_extends);
  const auto missing = library().load("absent");
  ASSERT_FALSE(missing.has_value());
  EXPECT_EQ(missing.error().code(), ThemeErrorCode::not_found);
}

TEST_F(AppearanceTest, ABadUserFileFallsBackToItsBaseAndReports) {
  install("base");
  install("misspelt");
  const theme::Theme resolved = library().resolve("misspelt", diagnostics_);
  EXPECT_EQ(resolved.name(), "base");
  EXPECT_FLOAT_EQ(resolved.radius(theme::radius::card), 20.0F);
  const std::vector<provider::Diagnostic> reported = diagnostics_.snapshot();
  ASSERT_EQ(reported.size(), 1U);
  EXPECT_EQ(reported.front().category, "theme");
  EXPECT_NE(reported.front().message.find("themes/misspelt.toml:6"),
            std::string::npos);
  install("syntax");
  EXPECT_EQ(library().resolve("syntax", diagnostics_),
            theme::Theme::defaults());
}

TEST_F(AppearanceTest, LibraryFindsCompiledPackAndUserThemes) {
  assets_.add(path("arisa/themes/pack.toml"),
              "schema = 1\nname = \"pack\"\n[radii]\ncard = 9\n");
  install("base");
  install("child");
  const std::vector<std::string> names = library().names();
  EXPECT_EQ(names, (std::vector<std::string>{"default", "high-contrast", "pack",
                                             "base", "child"}));
  auto pack = library().load("pack");
  ASSERT_TRUE(pack.has_value());
  EXPECT_FLOAT_EQ(pack->radius(theme::radius::card), 9.0F);
  storage_.add(path("themes/pack.toml"),
               "schema = 1\nname = \"pack\"\n[radii]\ncard = 11\n");
  EXPECT_FLOAT_EQ(library().load("pack")->radius(theme::radius::card), 11.0F);
}

TEST_F(AppearanceTest, TheShippedGlassPackLoadsWithChiakiNativeValues) {
  std::ifstream stream{std::filesystem::path{ARISA_SOURCE_ASSETS} /
                       "arisa/themes/arisa-theme-glass.toml"};
  const std::string text{std::istreambuf_iterator<char>{stream},
                         std::istreambuf_iterator<char>{}};
  ASSERT_FALSE(text.empty());
  assets_.add(path("arisa/themes/arisa-theme-glass.toml"), text);
  auto glass =
      ThemeLibrary{assets_, std::nullopt, {}}.load("arisa-theme-glass");
  ASSERT_TRUE(glass.has_value()) << glass.error().description();
  EXPECT_EQ(glass->seed().accent, theme::Theme::defaults().seed().accent);
  EXPECT_FLOAT_EQ(glass->radius(theme::radius::control), 12.0F);
  EXPECT_FLOAT_EQ(glass->radius(theme::radius::card), 18.0F);
  EXPECT_FLOAT_EQ(glass->radius(theme::radius::sheet), 28.0F);
  ASSERT_TRUE(glass->glass().sidebar.has_value());
  ASSERT_TRUE(glass->glass().toolbar.has_value());
  ASSERT_TRUE(glass->glass().sheet.has_value());
  EXPECT_EQ(glass->glass().control.variant(), theme::Glass::Variant::clear);
  EXPECT_FALSE(glass->glass().panel.tint_colour().has_value());
  EXPECT_FALSE(glass->glass().sidebar->tint_colour().has_value());
  EXPECT_FLOAT_EQ(glass->liquid_glass().refraction, 43.0F);
  EXPECT_FLOAT_EQ(glass->liquid_glass().clear_blur, 1.0F);
  EXPECT_EQ(glass->palette(theme::ColourRole::accent),
            theme::Theme::defaults().palette(theme::ColourRole::accent));
}

TEST_F(AppearanceTest, AppearanceFilesRoundTrip) {
  const auto parsed =
      AppearanceFile::parse(fixture("appearance.toml"), "appearance.toml");
  ASSERT_TRUE(parsed.has_value()) << parsed.error().description();
  EXPECT_EQ(parsed->theme, "child");
  EXPECT_EQ(parsed->color_scheme, ColorSchemePreference::dark);
  EXPECT_EQ(parsed->accent, theme::Colour::hex(0xAF52DE));
  EXPECT_EQ(parsed->dynamic_type_size, theme::DynamicTypeSize::accessibility3);
  EXPECT_EQ(parsed->reduce_motion, true);
  EXPECT_EQ(parsed->reduce_transparency, true);
  EXPECT_EQ(parsed->increase_contrast, true);
  EXPECT_EQ(parsed->sensory_feedback, false);
  EXPECT_EQ(parsed->haptic_strength,
            provider::HapticStrength::from_percent(65));
  EXPECT_EQ(parsed->sidebar,
            composition::NavigationSplitViewVisibility::icons_only);
  EXPECT_EQ(parsed->ui_scale.handheld, 1.4F);
  EXPECT_EQ(parsed->ui_scale.docked, 1.1F);
  EXPECT_FALSE(parsed->ui_scale.desktop.has_value());
  const std::string written = AppearanceFile::serialise(*parsed);
  const auto again = AppearanceFile::parse(written, "written.toml");
  ASSERT_TRUE(again.has_value()) << again.error().description() << written;
  EXPECT_EQ(*again, *parsed);
  const auto bad = AppearanceFile::parse(fixture("appearance_bad.toml"),
                                         "appearance_bad.toml");
  ASSERT_FALSE(bad.has_value());
  EXPECT_EQ(bad.error().line(), 3U);
  EXPECT_EQ(bad.error().key(), "dynamic_type_size");
}

TEST_F(AppearanceTest, ModelPersistsUserChoicesAndRestoresThem) {
  install("base");
  {
    const AppearanceModel model{AppearancePolicy{}, library(),
                                std::ref(storage_), diagnostics_};
    EXPECT_EQ(model.settings().get(), AppearanceSettings{});
    model.update([](AppearanceOverrides& overrides) {
      overrides.theme = "base";
      overrides.reduce_motion = true;
      overrides.dynamic_type_size = theme::DynamicTypeSize::small;
    });
    EXPECT_EQ(storage_.writes(), 1U);
    EXPECT_EQ(model.theme().name(), "base");
  }
  const std::string saved = storage_.text(path("appearance.toml"));
  EXPECT_NE(saved.find("theme = "), std::string::npos) << saved;
  EXPECT_NE(saved.find("reduce_motion = true"), std::string::npos) << saved;
  const AppearanceModel restored{AppearancePolicy{}, library(),
                                 std::ref(storage_), diagnostics_};
  EXPECT_EQ(restored.settings().get().theme, "base");
  EXPECT_TRUE(restored.settings().get().reduce_motion);
  EXPECT_EQ(restored.settings().get().dynamic_type_size,
            theme::DynamicTypeSize::small);
  EXPECT_FLOAT_EQ(restored.theme().radius(theme::radius::card), 20.0F);
}

TEST_F(AppearanceTest, ThemePreferencesSitBetweenAppDefaultsAndTheUser) {
  install("base");
  AppearancePolicy policy;
  policy.defaults.theme = "base";
  const AppearanceModel model{policy, library(), std::ref(storage_),
                              diagnostics_};
  EXPECT_EQ(model.settings().get().dynamic_type_size,
            theme::DynamicTypeSize::x_large);
  EXPECT_FLOAT_EQ(
      model.settings().get().ui_scale.resolve(provider::DisplayMode::handheld),
      1.5F);
  model.update([](AppearanceOverrides& overrides) {
    overrides.dynamic_type_size = theme::DynamicTypeSize::medium;
  });
  EXPECT_EQ(model.settings().get().dynamic_type_size,
            theme::DynamicTypeSize::medium);
}

TEST_F(AppearanceTest, ThemeFilesTakeNegativeTrackingAndNamedWeights) {
  auto loaded = load("pinned");
  ASSERT_TRUE(loaded.has_value()) << loaded.error().description();
  EXPECT_EQ(loaded->base_type(theme::TextRole::large_title).weight, 600);
  EXPECT_FLOAT_EQ(loaded->base_type(theme::TextRole::large_title).tracking,
                  -0.4F);
  EXPECT_EQ(loaded->base_type(theme::TextRole::headline).weight, 650);
  EXPECT_FLOAT_EQ(loaded->base_type(theme::TextRole::headline).tracking, -0.2F);
  EXPECT_EQ(loaded->preferences().color_scheme, theme::ColorScheme::dark);
}

TEST_F(AppearanceTest, ThemeFilesRejectAWeightOutsideTheRange) {
  storage_.add(path("themes/heavy.toml"),
               "schema = 1\nname = \"heavy\"\nextends = \"default\"\n"
               "[type.body]\nweight = 1200\n");
  EXPECT_FALSE(library().load("heavy").has_value());
  storage_.add(path("themes/named.toml"),
               "schema = 1\nname = \"named\"\nextends = \"default\"\n"
               "[type.body]\nweight = \"chunky\"\n");
  EXPECT_FALSE(library().load("named").has_value());
}

TEST_F(AppearanceTest, AThemeThatPinsAColourSchemeWinsOverTheUserChoice) {
  install("pinned");
  AppearancePolicy policy;
  policy.defaults.theme = "pinned";
  const AppearanceModel model{policy, library(), std::ref(storage_),
                              diagnostics_};
  EXPECT_EQ(model.settings().get().color_scheme, ColorSchemePreference::dark);
  model.update([](AppearanceOverrides& overrides) {
    overrides.color_scheme = ColorSchemePreference::light;
  });
  EXPECT_EQ(model.settings().get().color_scheme, ColorSchemePreference::dark);
}

TEST_F(AppearanceTest, ABadSettingsFileFallsBackToTheAppDefaults) {
  storage_.add(path("appearance.toml"), fixture("appearance_bad.toml"));
  AppearancePolicy policy;
  policy.defaults.reduce_transparency = true;
  const AppearanceModel model{policy, library(), std::ref(storage_),
                              diagnostics_};
  ASSERT_TRUE(model.load_error().has_value());
  EXPECT_EQ(model.load_error()->key(), "dynamic_type_size");
  EXPECT_EQ(model.settings().get(), policy.defaults);
  const std::vector<provider::Diagnostic> reported = diagnostics_.snapshot();
  ASSERT_FALSE(reported.empty());
  EXPECT_EQ(reported.back().category, "appearance");
}

TEST_F(AppearanceTest, AppLocksBeatUserChoices) {
  storage_.add(path("appearance.toml"), fixture("appearance.toml"));
  install("base");
  install("child");
  AppearancePolicy policy;
  policy.defaults.color_scheme = ColorSchemePreference::light;
  policy.defaults.dynamic_type_size = theme::DynamicTypeSize::large;
  policy.locks = AppearanceLocks{AppearanceField::color_scheme,
                                 AppearanceField::dynamic_type_size,
                                 AppearanceField::sensory_feedback};
  const AppearanceModel model{policy, library(), std::ref(storage_),
                              diagnostics_};
  const AppearanceSettings& settings = model.settings().get();
  EXPECT_TRUE(settings.sensory_feedback);
  EXPECT_EQ(settings.color_scheme, ColorSchemePreference::light);
  EXPECT_EQ(settings.dynamic_type_size, theme::DynamicTypeSize::large);
  EXPECT_TRUE(settings.reduce_motion);
  EXPECT_EQ(settings.theme, "child");
  EXPECT_TRUE(model.is_locked(AppearanceField::color_scheme));
  EXPECT_FALSE(model.is_locked(AppearanceField::theme));
  model.update([](AppearanceOverrides& overrides) {
    overrides.color_scheme = ColorSchemePreference::dark;
  });
  EXPECT_EQ(model.settings().get().color_scheme, ColorSchemePreference::light);
}

TEST_F(AppearanceTest, HapticStrengthPersistsAndHonoursItsLock) {
  {
    const AppearanceModel model{AppearancePolicy{}, library(),
                                std::ref(storage_), diagnostics_};
    EXPECT_EQ(model.settings().get().haptic_strength,
              provider::HapticStrength{});
    model.update([](AppearanceOverrides& overrides) {
      overrides.haptic_strength = provider::HapticStrength::from_percent(35);
    });
  }
  const std::string saved = storage_.text(path("appearance.toml"));
  EXPECT_NE(saved.find("haptic_strength = 0.35"), std::string::npos) << saved;
  const AppearanceModel restored{AppearancePolicy{}, library(),
                                 std::ref(storage_), diagnostics_};
  EXPECT_EQ(restored.settings().get().haptic_strength,
            provider::HapticStrength::from_percent(35));
  const auto whole =
      AppearanceFile::parse("schema = 1\nhaptic_strength = 2\n", "whole.toml");
  ASSERT_TRUE(whole.has_value()) << whole.error().description();
  EXPECT_EQ(whole->haptic_strength,
            provider::HapticStrength::from_percent(200));
  const auto over =
      AppearanceFile::parse("schema = 1\nhaptic_strength = 2.5\n", "over.toml");
  ASSERT_FALSE(over.has_value());
  EXPECT_EQ(over.error().key(), "haptic_strength");
  EXPECT_EQ(over.error().line(), 2U);
  const auto word = AppearanceFile::parse(
      "schema = 1\nhaptic_strength = \"loud\"\n", "word.toml");
  ASSERT_FALSE(word.has_value());
  EXPECT_EQ(word.error().key(), "haptic_strength");
  AppearancePolicy policy;
  policy.defaults.haptic_strength = provider::HapticStrength::from_percent(80);
  policy.locks = AppearanceLocks{AppearanceField::haptic_strength};
  const AppearanceModel locked{policy, library(), std::ref(storage_),
                               diagnostics_};
  EXPECT_TRUE(locked.is_locked(AppearanceField::haptic_strength));
  EXPECT_EQ(locked.settings().get().haptic_strength,
            provider::HapticStrength::from_percent(80));
  locked.update([](AppearanceOverrides& overrides) {
    overrides.haptic_strength = provider::HapticStrength::from_percent(150);
  });
  EXPECT_EQ(locked.settings().get().haptic_strength,
            provider::HapticStrength::from_percent(80));
}

TEST_F(AppearanceTest, UiScalesFallBackToThePlatformDefaultPerMode) {
  const UiScales scales{
      .handheld = std::nullopt, .docked = 1.1F, .desktop = std::nullopt};
  EXPECT_FLOAT_EQ(scales.resolve(provider::DisplayMode::handheld),
                  UiScales::handheld_default);
  EXPECT_FLOAT_EQ(scales.resolve(provider::DisplayMode::docked), 1.1F);
  EXPECT_FLOAT_EQ(scales.resolve(provider::DisplayMode::desktop), 1.0F);
  EXPECT_FLOAT_EQ(scales.with(provider::DisplayMode::desktop, 9.0F)
                      .resolve(provider::DisplayMode::desktop),
                  UiScales::maximum);
}

class DynamicTypeTest : public ::testing::Test {
 protected:
  struct Row {
    float size;
    std::uint16_t weight;
    float line_height;
  };

  static auto dump() -> std::map<std::pair<std::string, std::string>, Row> {
    std::ifstream stream{ARISA_DYNAMIC_TYPE_REFERENCE};
    std::map<std::pair<std::string, std::string>, Row> rows;
    std::string line;
    while (std::getline(stream, line)) {
      if (line.empty() || line.starts_with('#') || line.starts_with("style")) {
        continue;
      }
      std::istringstream fields{line};
      std::string style;
      std::string category;
      Row row{};
      float leading = 0.0F;
      fields >> style >> category >> row.size >> row.weight >>
          row.line_height >> leading;
      rows.emplace(std::pair{style, category}, row);
    }
    return rows;
  }
};

TEST_F(DynamicTypeTest, TheTableMatchesTheSimulatorDump) {
  const auto rows = dump();
  ASSERT_EQ(rows.size(),
            theme::text_role_count * theme::dynamic_type_size_count);
  for (std::size_t role = 0; role < theme::text_role_count; ++role) {
    const auto text = static_cast<theme::TextRole>(role);
    const std::string style{ThemeVocabulary::text_roles()[role]};
    for (std::size_t index = 0; index < theme::dynamic_type_size_count;
         ++index) {
      const auto size = static_cast<theme::DynamicTypeSize>(index);
      const std::string category{theme::DynamicType::name(size)};
      const Row& row = rows.at({style, category});
      EXPECT_FLOAT_EQ(theme::DynamicType::reference_size(text, size), row.size)
          << style << " " << category;
      EXPECT_NEAR(theme::DynamicType::reference_line_height(text, size),
                  row.line_height, 1.0e-3F)
          << style << " " << category;
      EXPECT_EQ(theme::DynamicType::reference_weight(text), row.weight);
    }
  }
}

TEST_F(DynamicTypeTest, LargeIsAnchoredToTheMacSizesAndStepsFollowIos) {
  const theme::Theme theme = theme::Theme::defaults();
  EXPECT_FLOAT_EQ(theme.type(theme::TextRole::body).size, 13.0F);
  const theme::Theme huge = theme.sized(theme::DynamicTypeSize::accessibility3);
  EXPECT_NEAR(huge.type(theme::TextRole::body).size, 13.0F * 40.0F / 17.0F,
              1.0e-4F);
  EXPECT_NEAR(huge.type(theme::TextRole::large_title).size,
              26.0F * 52.0F / 34.0F, 1.0e-4F);
  const theme::Theme tiny = theme.sized(theme::DynamicTypeSize::x_small);
  EXPECT_NEAR(tiny.type(theme::TextRole::caption2).size, 10.0F, 1.0e-4F);
  EXPECT_NEAR(tiny.type(theme::TextRole::body).line_height,
              16.0F * 14.0F / 17.0F, 1.0e-4F);
  EXPECT_EQ(theme::DynamicType::parse("accessibility3"),
            theme::DynamicTypeSize::accessibility3);
  EXPECT_FALSE(theme::DynamicType::parse("huge").has_value());
}

TEST_F(DynamicTypeTest, ScaledMetricsGrowRelativeToTheirTextStyle) {
  const theme::Theme theme =
      theme::Theme::defaults().sized(theme::DynamicTypeSize::xxx_large);
  EXPECT_NEAR(theme.scaled(theme::ScaledMetric{10.0F}), 10.0F * 23.0F / 17.0F,
              1.0e-4F);
  EXPECT_NEAR(theme.scaled(theme::ScaledMetric{10.0F, theme::TextRole::title}),
              10.0F * 34.0F / 28.0F, 1.0e-4F);
  EXPECT_NEAR(theme.scaled(theme::metric::sidebar_width),
              144.0F * 23.0F / 17.0F, 1.0e-3F);
  EXPECT_NEAR(theme.space(theme::metric::button_vertical_padding),
              4.0F * 23.0F / 17.0F, 1.0e-4F);
  EXPECT_FLOAT_EQ(theme::Theme::defaults().scaled(theme::metric::sidebar_width),
                  144.0F);
}

TEST_F(DynamicTypeTest, RangesClampTheEnvironmentSize) {
  const auto range =
      theme::DynamicTypeRange::up_to(theme::DynamicTypeSize::xx_large);
  EXPECT_EQ(range.clamp(theme::DynamicTypeSize::accessibility5),
            theme::DynamicTypeSize::xx_large);
  EXPECT_EQ(range.clamp(theme::DynamicTypeSize::small),
            theme::DynamicTypeSize::small);
  const theme::DynamicTypeRange between{theme::DynamicTypeSize::large,
                                        theme::DynamicTypeSize::medium};
  EXPECT_EQ(between.lower(), theme::DynamicTypeSize::medium);
  EXPECT_EQ(between.clamp(theme::DynamicTypeSize::x_small),
            theme::DynamicTypeSize::medium);
}

TEST_F(AppearanceTest, RomfsThemeLoadsAndAStaleUserCopyFallsBackToIt) {
  const std::string orchard =
      "schema = 1\nname = \"orchard\"\nextends = \"default\"\n"
      "accent = \"#34C759\"\n[radii]\ncontrol = 12\ncard = 20\n"
      "[colours]\nsystem_grouped_background = { light = \"#DDF2E2\", dark = "
      "\"#173320\" }\n";
  assets_.add(path("arisa/themes/orchard.toml"), orchard);
  auto bundled =
      ThemeLibrary{assets_, std::nullopt, {"orchard"}}.load("orchard");
  ASSERT_TRUE(bundled.has_value()) << bundled.error().description();
  EXPECT_EQ(bundled->seed().accent, theme::Colour::hex(0x34C759));
  EXPECT_FLOAT_EQ(bundled->radius(theme::radius::card), 20.0F);
  storage_.add(path("themes/orchard.toml"),
               "schema = 0\nname = \"orchard\"\naccent = \"#000000\"\n");
  auto preferred = library().load("orchard");
  ASSERT_TRUE(preferred.has_value()) << preferred.error().description();
  EXPECT_EQ(preferred->seed().accent, theme::Colour::hex(0x34C759));
  storage_.add(path("themes/broken.toml"),
               "schema = 1\nextends = \"default\"\n[colours]\nlabell = "
               "\"#000000\"\n");
  const auto broken = library().load("broken");
  ASSERT_FALSE(broken.has_value());
  EXPECT_EQ(broken.error().file(), "themes/broken.toml");
}

TEST_F(AppearanceTest, FuzzedThemeFilesFailWithASyntaxError) {
  const std::filesystem::path regressions =
      std::filesystem::path{ARISA_THEME_FIXTURES}.parent_path().parent_path() /
      "fuzz" / "regressions" / "theme";
  const ThemeFile::Resolver bases =
      [](std::string_view) -> std::expected<theme::Theme, ThemeError> {
    return theme::Theme::defaults();
  };
  int replayed = 0;
  for (const auto& entry : std::filesystem::directory_iterator{regressions}) {
    std::ifstream file{entry.path(), std::ios::binary};
    const std::string text{std::istreambuf_iterator<char>{file},
                           std::istreambuf_iterator<char>{}};
    const auto parsed = ThemeFile::parse(text, "fuzz.toml", bases);
    ASSERT_FALSE(parsed.has_value()) << entry.path();
    EXPECT_EQ(parsed.error().code(), ThemeErrorCode::syntax) << entry.path();
    EXPECT_FALSE(AppearanceFile::parse(text, "fuzz.toml").has_value())
        << entry.path();
    ++replayed;
  }
  EXPECT_GE(replayed, 3);
}

TEST_F(AppearanceTest, AFocusSectionPicksTheHighlightStyle) {
  const ThemeFile::Resolver bases =
      [](std::string_view) -> std::expected<theme::Theme, ThemeError> {
    return theme::Theme::defaults();
  };
  const auto parsed = ThemeFile::parse(
      "schema = 1\nname = \"focus\"\n[focus]\nstyle = \"highlight\"\n",
      "focus.toml", bases);
  ASSERT_TRUE(parsed.has_value());
  EXPECT_EQ(parsed->focus_ring().style,
            theme::Theme::FocusRing::Style::highlight);
  EXPECT_EQ(theme::Theme::defaults().focus_ring().style,
            theme::Theme::FocusRing::Style::ring);
  const auto refused = ThemeFile::parse(
      "schema = 1\nname = \"focus\"\n[focus]\nstyle = \"glow\"\n",
      "focus.toml", bases);
  ASSERT_FALSE(refused.has_value());
  EXPECT_EQ(refused.error().code(), ThemeErrorCode::invalid_value);
}

}
