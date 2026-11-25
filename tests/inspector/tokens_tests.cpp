#include <arisa/appearance/appearance_model.hpp>
#include <arisa/appearance/theme_file.hpp>
#include <arisa/appearance/theme_token.hpp>
#include <arisa/appearance/theme_vocabulary.hpp>
#include <arisa/components/text.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/modifiers.hpp>
#include <arisa/inspector/inspector.hpp>
#include <arisa/inspector/token_editor.hpp>
#include <arisa/inspector/tokens_panel.hpp>
#include <arisa/provider/diagnostics/allocation_meter.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/headless/memory_storage.hpp>

#include <gallery_fixture.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <expected>
#include <functional>
#include <optional>
#include <print>
#include <span>
#include <string>
#include <variant>
#include <vector>

namespace arisa::runtime {

class TokensTest : public GalleryFixture {
 protected:
  static constexpr int settle_frames = 8;
  static constexpr int switch_frames = 10;
  static constexpr std::chrono::milliseconds switch_step{200};
  static constexpr std::string_view base_theme = "arisa-theme-glass";

  auto options() -> AppearanceOptions {
    AppearanceOptions chosen{
        .policy = examples::GalleryScreen::policy(theme::ColorScheme::light),
        .storage = std::ref(storage_),
        .themes = std::ref(folder_),
        .hot_reload = std::nullopt};
    chosen.policy.defaults.theme = std::string{base_theme};
    return chosen;
  }

  auto start(provider::headless::HeadlessProviderFactory& providers,
             bool raster = true) -> Application {
    if (raster) {
      providers.gpu().rasterize(
          provider::headless::ScriptedSurface::default_size);
    }
    Application application = start_gallery(
        providers, examples::GalleryPage::theming, theme::ColorScheme::light,
        provider::headless::ScriptedSurface::default_size, options());
    run(providers, application, settle_frames);
    return application;
  }

  static auto token(std::string_view path) -> const appearance::ThemeToken& {
    const auto found = appearance::ThemeToken::find(path);
    EXPECT_TRUE(found.has_value()) << path;
    return found->get();
  }

  static auto pixels(provider::headless::HeadlessProviderFactory& providers)
      -> std::vector<std::byte> {
    const auto image = providers.gpu().capture();
    EXPECT_TRUE(image.has_value());
    if (!image.has_value()) {
      return {};
    }
    return std::vector<std::byte>(image->pixels().begin(),
                                  image->pixels().end());
  }

  static auto count(const std::vector<std::byte>& image, theme::Colour colour)
      -> std::size_t {
    const auto channel = [](float value) {
      return static_cast<std::byte>(std::lround(value * 255.0F));
    };
    const std::array<std::byte, 3> wanted{
        channel(colour.red), channel(colour.green), channel(colour.blue)};
    std::size_t found = 0;
    for (std::size_t index = 0; index + 3 < image.size(); index += 4) {
      if (image[index] == wanted[0] && image[index + 1] == wanted[1] &&
          image[index + 2] == wanted[2]) {
        ++found;
      }
    }
    return found;
  }

  static auto differing(const std::vector<std::byte>& left,
                        const std::vector<std::byte>& right) -> std::size_t {
    std::size_t changed = 0;
    for (std::size_t index = 0; index + 3 < left.size() && index < right.size();
         index += 4) {
      if (!std::equal(left.begin() + static_cast<std::ptrdiff_t>(index),
                      left.begin() + static_cast<std::ptrdiff_t>(index + 4),
                      right.begin() + static_cast<std::ptrdiff_t>(index))) {
        ++changed;
      }
    }
    return changed;
  }

  class FailingStorage final : public provider::StorageProvider {
   public:
    [[nodiscard]] auto read(const provider::AssetPath& path) const
        -> std::expected<provider::AssetBlob,
                         provider::ProviderError> override {
      static_cast<void>(path);
      return std::unexpected(
          provider::ProviderError{provider::ProviderErrorCode::not_found});
    }

    [[nodiscard]] auto write(const provider::AssetPath& path,
                             std::span<const std::byte> bytes)
        -> std::expected<void, provider::ProviderError> override {
      static_cast<void>(path);
      static_cast<void>(bytes);
      return std::unexpected(provider::ProviderError{
          provider::ProviderErrorCode::resource_exhausted, full});
    }

    [[nodiscard]] auto list(const provider::AssetPath& directory) const
        -> std::expected<std::vector<std::string>,
                         provider::ProviderError> override {
      static_cast<void>(directory);
      return std::vector<std::string>{};
    }

    [[nodiscard]] auto create_directories(const provider::AssetPath& directory)
        -> std::expected<void, provider::ProviderError> override {
      static_cast<void>(directory);
      return {};
    }

    static constexpr std::uint32_t full = 28;
  };

  static auto saturation(Application& application,
                         provider::headless::HeadlessProviderFactory& providers)
      -> std::optional<provider::PixelPoint> {
    composition::Tree& tree = application.root().tree();
    const auto found = tree.find(composition::ElementKey{
        std::string{inspector::TokenEditor::saturation_key}});
    if (!found.has_value()) {
      return std::nullopt;
    }
    const composition::Node& node = tree.node(*found)->get();
    const composition::Rect bounds =
        node.presented().has_value() ? node.presented()->bounds : node.frame();
    const float scale =
        static_cast<float>(providers.surface().size().width.value()) /
        tree.root().frame().width;
    return provider::PixelPoint{
        .x = bounds.x * scale,
        .y = (bounds.y + (bounds.height * 0.5F)) * scale};
  }

  static auto slider_width(Application& application) -> float {
    composition::Tree& tree = application.root().tree();
    const auto found = tree.find(composition::ElementKey{
        std::string{inspector::TokenEditor::saturation_key}});
    return found.has_value() ? tree.node(*found)->get().frame().width : 0.0F;
  }

  static void touch(provider::headless::HeadlessProviderFactory& providers,
                    provider::PixelPoint point, provider::TouchPhase phase) {
    provider::InputFrame frame;
    frame.touches.push_back(provider::Touch{
        .id = provider::TouchId{1}, .position = point, .phase = phase});
    providers.input().push(std::move(frame));
  }

  struct DragCost {
    double recorded{0.0};
    double synced{0.0};
    std::size_t nodes{0};
  };

  static auto drag(provider::headless::HeadlessProviderFactory& providers,
                   Application& application, int steps) -> DragCost {
    const auto start = saturation(application, providers);
    EXPECT_TRUE(start.has_value());
    if (!start.has_value()) {
      return {};
    }
    const float scale =
        static_cast<float>(providers.surface().size().width.value()) /
        application.root().tree().root().frame().width;
    const float span = slider_width(application) * scale;
    const auto at = [&](float along) {
      return provider::PixelPoint{.x = start->x + (span * along),
                                  .y = start->y};
    };
    DragCost cost{.nodes = application.root().tree().size()};
    touch(providers, at(0.1F), provider::TouchPhase::began);
    run(providers, application, 1);
    for (int step = 0; step < steps; ++step) {
      touch(providers,
            at(0.1F +
               (0.8F * static_cast<float>(step) / static_cast<float>(steps))),
            provider::TouchPhase::moved);
      const std::vector<Introspection::CostRecord> before =
          application.introspection().costs();
      run(providers, application, 1);
      std::uint64_t synced = 0;
      for (const Introspection::CostRecord& after :
           application.introspection().costs()) {
        const auto was =
            std::ranges::find(before, after.id, &Introspection::CostRecord::id);
        synced +=
            after.cost.layouts - (was == before.end() ? 0U : was->cost.layouts);
      }
      cost.recorded += static_cast<double>(application.last_frame().recorded);
      cost.synced += static_cast<double>(synced);
    }
    touch(providers, at(0.9F), provider::TouchPhase::ended);
    run(providers, application, settle_frames);
    cost.recorded /= steps;
    cost.synced /= steps;
    return cost;
  }

  provider::headless::MemoryStorage storage_;
  provider::headless::MemoryStorage folder_;
};

TEST_F(TokensTest, EditingTheAccentRepaintsControlsAndUndoRestoresExactly) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers);
  ASSERT_TRUE(application.appearance().has_value());
  const appearance::AppearanceModel& model = *application.appearance();
  const theme::Theme before_theme = model.theme();
  const std::vector<std::byte> before = pixels(providers);
  const theme::Colour green = theme::Colour::hex(0x00C800);
  ASSERT_EQ(count(before, green), 0U);
  ASSERT_TRUE(model.edit(token("colours.accent.light"), green));
  run(providers, application, settle_frames);
  const std::vector<std::byte> edited = pixels(providers);
  EXPECT_GT(count(edited, green), 50U);
  EXPECT_GT(differing(before, edited), 50U);
  EXPECT_TRUE(model.edits().edited(token("colours.accent.light")));
  EXPECT_TRUE(model.undo(token("colours.accent.light")));
  run(providers, application, settle_frames);
  EXPECT_EQ(model.theme(), before_theme);
  EXPECT_EQ(differing(before, pixels(providers)), 0U);
  std::println("accent edit: {} green pixels, {} changed, 0 after undo",
               count(edited, green), differing(before, edited));
}

TEST_F(TokensTest, UndoRevertsOnlyThatToken) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, false);
  const appearance::AppearanceModel& model = *application.appearance();
  const appearance::ThemeToken& radius = token("radii.control");
  const appearance::ThemeToken& gap = token("spacing.sm");
  const float original = std::get<float>(radius.read(model.theme()));
  ASSERT_TRUE(model.edit(radius, 4.0F));
  ASSERT_TRUE(model.edit(radius, 9.5F));
  ASSERT_TRUE(model.edit(gap, 13.0F));
  EXPECT_FALSE(model.edit(gap, 13.0F));
  EXPECT_EQ(std::get<float>(radius.read(model.theme())), 9.5F);
  ASSERT_TRUE(model.undo(radius));
  EXPECT_EQ(std::get<float>(radius.read(model.theme())), original);
  EXPECT_EQ(std::get<float>(gap.read(model.theme())), 13.0F);
  EXPECT_FALSE(model.undo(radius));
  EXPECT_EQ(model.edits().count(), 1U);
}

TEST_F(TokensTest, ResetAllRestoresTheLoadedTheme) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers);
  const appearance::AppearanceModel& model = *application.appearance();
  const theme::Theme loaded = model.theme();
  const std::vector<std::byte> before = pixels(providers);
  ASSERT_TRUE(
      model.edit(token("colours.accent.light"), theme::Colour::hex(0xD02090)));
  ASSERT_TRUE(model.edit(token("radii.control"), 2.0F));
  ASSERT_TRUE(model.edit(token("spacing.md"), 20.0F));
  ASSERT_TRUE(model.edit(token("type.body.weight"), 700.0F));
  run(providers, application, settle_frames);
  EXPECT_GT(differing(before, pixels(providers)), 0U);
  EXPECT_EQ(model.edits().count(), 4U);
  model.reset_edits();
  run(providers, application, settle_frames);
  EXPECT_TRUE(model.edits().empty());
  EXPECT_EQ(model.theme(), loaded);
  EXPECT_EQ(differing(before, pixels(providers)), 0U);
}

TEST_F(TokensTest, ExportedFileReloadsToTheEditedRenderPixelForPixel) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers);
  const appearance::AppearanceModel& model = *application.appearance();
  ASSERT_TRUE(
      model.edit(token("colours.accent.light"), theme::Colour::hex(0x00A36C)));
  ASSERT_TRUE(model.edit(token("colours.label.light"),
                         theme::Colour::hex(0x203040, 0.9F)));
  ASSERT_TRUE(model.edit(token("radii.control"), 3.5F));
  ASSERT_TRUE(model.edit(token("type.body.size"), 19.0F));
  ASSERT_TRUE(model.edit(token("type.headline.design"), std::size_t{1}));
  ASSERT_TRUE(model.edit(token("materials.thin.saturation"), 1.07F));
  ASSERT_TRUE(model.edit(token("liquid_glass.rim"), 0.55F));
  ASSERT_TRUE(model.edit(token("glass.control.variant"), std::size_t{0}));
  ASSERT_TRUE(model.edit(token("glass.control.interactive"), true));
  ASSERT_TRUE(model.edit(token("motion.focus.response"), 0.42F));
  ASSERT_TRUE(model.edit(token("radii.continuous"), false));
  run(providers, application, settle_frames);
  const theme::Theme edited_theme = model.theme();
  const std::vector<std::byte> edited = pixels(providers);
  auto exported = model.export_edits();
  ASSERT_TRUE(exported.has_value()) << exported.error().description();
  EXPECT_EQ(exported->name, std::string{base_theme} + "-edited");
  EXPECT_EQ(exported->file, "themes/arisa-theme-glass-edited.toml");
  EXPECT_FALSE(
      folder_.text(*provider::AssetPath::create(exported->file)).empty());
  EXPECT_EQ(appearance::ThemeFile::base_of(exported->text), base_theme);
  model.reset_edits();
  run(providers, application, settle_frames);
  EXPECT_GT(differing(edited, pixels(providers)), 0U);
  model.update([&exported](appearance::AppearanceOverrides& overrides) {
    overrides.theme = exported->name;
  });
  for (int frame = 0; frame < switch_frames; ++frame) {
    ASSERT_TRUE(providers.clock().advance(switch_step).has_value());
    ASSERT_TRUE(application.frame().has_value());
  }
  EXPECT_FALSE(model.load_error().has_value());
  EXPECT_EQ(model.settings().get().theme, exported->name);
  for (const appearance::ThemeToken& each : appearance::ThemeToken::all()) {
    EXPECT_EQ(each.read(model.theme()), each.read(edited_theme)) << each.path();
  }
  EXPECT_EQ(differing(edited, pixels(providers)), 0U);
  std::println("export: {} bytes, {} lines", exported->text.size(),
               std::ranges::count(exported->text, '\n'));
}

TEST_F(TokensTest, ExportingAnEditedThemeFlattensIntoItsBase) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, false);
  const appearance::AppearanceModel& model = *application.appearance();
  ASSERT_TRUE(model.edit(token("radii.card"), 7.0F));
  auto first = model.export_edits();
  ASSERT_TRUE(first.has_value());
  model.reset_edits();
  model.update([&first](appearance::AppearanceOverrides& overrides) {
    overrides.theme = first->name;
  });
  ASSERT_TRUE(model.edit(token("radii.sheet"), 11.0F));
  const theme::Theme twice = model.theme();
  auto second = model.export_edits();
  ASSERT_TRUE(second.has_value());
  EXPECT_EQ(second->name, first->name);
  EXPECT_EQ(appearance::ThemeFile::base_of(second->text), base_theme);
  model.reset_edits();
  ASSERT_TRUE(model.reload().has_value());
  EXPECT_EQ(std::get<float>(token("radii.card").read(model.theme())), 7.0F);
  EXPECT_EQ(std::get<float>(token("radii.sheet").read(model.theme())), 11.0F);
  EXPECT_EQ(model.theme().radii(), twice.radii());
}

TEST_F(TokensTest, SliderRangesClampAndSnap) {
  const appearance::ThemeToken& metric = token("metrics.sidebar_width");
  EXPECT_EQ(std::get<float>(*metric.clamp(1000.0F)), 400.0F);
  EXPECT_EQ(std::get<float>(*metric.clamp(-20.0F)), 0.0F);
  EXPECT_EQ(std::get<float>(*metric.clamp(12.3F)), 12.5F);
  const appearance::ThemeToken& weight = token("type.body.weight");
  EXPECT_EQ(std::get<float>(*weight.clamp(640.0F)), 600.0F);
  EXPECT_EQ(std::get<float>(*weight.clamp(50.0F)), 100.0F);
  const appearance::ThemeToken& damping =
      token("motion.standard.damping_ratio");
  EXPECT_EQ(std::get<float>(*damping.clamp(0.0F)), 0.05F);
  EXPECT_FALSE(metric.clamp(true).has_value());
  EXPECT_FALSE(token("type.body.design").clamp(std::size_t{5}).has_value());
  const appearance::ThemeToken& accent = token("colours.accent.light");
  const theme::Colour odd{
      .red = 0.123F, .green = 0.5F, .blue = 0.9F, .alpha = 1.0F};
  EXPECT_EQ(std::get<theme::Colour>(*accent.clamp(odd)),
            appearance::ThemeToken::quantised(odd));
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, false);
  const appearance::AppearanceModel& model = *application.appearance();
  ASSERT_TRUE(model.edit(metric, 5000.0F));
  EXPECT_EQ(model.theme().metric(theme::MetricToken::sidebar_width), 400.0F);
}

TEST_F(TokensTest, TokensPanelEditsTheAccentWithTheDpad) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, false);
  const appearance::AppearanceModel& model = *application.appearance();
  inspector::Inspector inspector = inspector::Inspector::attach(application);
  inspector.open();
  inspector.show(inspector::InspectorPanel::tokens);
  const appearance::ThemeToken& accent = token("colours.accent.light");
  inspector.model().select_token(accent.index());
  run(providers, application, 12);
  const composition::Tree& tree = application.root().tree();
  const auto hue = tree.find(
      composition::ElementKey{std::string{inspector::TokenEditor::hue_key}});
  ASSERT_TRUE(hue.has_value());
  const auto row = tree.find(
      composition::ElementKey{inspector::TokensPanel::row_key(accent)});
  ASSERT_TRUE(row.has_value());
  const auto inside = [&tree](composition::NodeId node,
                              composition::NodeId ancestor) {
    for (std::optional<composition::NodeId> at = node; at.has_value();) {
      if (*at == ancestor) {
        return true;
      }
      const auto found = tree.node(*at);
      at = found.has_value() ? found->get().parent() : std::nullopt;
    }
    return false;
  };
  int presses = 0;
  while (presses < 160) {
    const auto focused = application.focused();
    if (focused.has_value() && inside(*focused, *hue)) {
      break;
    }
    providers.input().push(pad({provider::Button::dpad_down}));
    run(providers, application, 1);
    providers.input().push(pad({}));
    run(providers, application, 1);
    ++presses;
  }
  ASSERT_TRUE(application.focused().has_value());
  ASSERT_TRUE(inside(*application.focused(), *hue)) << presses << " presses";
  const theme::Colour before =
      std::get<theme::Colour>(accent.read(model.theme()));
  for (int step = 0; step < 30; ++step) {
    providers.input().push(pad({provider::Button::dpad_right}));
    run(providers, application, 1);
    providers.input().push(pad({}));
    run(providers, application, 1);
  }
  run(providers, application, 6);
  const theme::Colour after =
      std::get<theme::Colour>(accent.read(model.theme()));
  EXPECT_NE(after, before);
  EXPECT_TRUE(model.edits().edited(accent));
  EXPECT_GT(theme::Hsb::from(after).hue, theme::Hsb::from(before).hue);
  inspector.model().undo_token(accent.index());
  run(providers, application, 6);
  EXPECT_EQ(std::get<theme::Colour>(accent.read(model.theme())), before);
  EXPECT_FALSE(model.edits().edited(accent));
  std::println("d-pad: focus reached hue after {} presses, {} -> {}", presses,
               appearance::ThemeVocabulary::hex(before),
               appearance::ThemeVocabulary::hex(after));
}

TEST_F(TokensTest, TokensPanelListsGroupsAndExportsFromTheButton) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, false);
  const appearance::AppearanceModel& model = *application.appearance();
  inspector::Inspector inspector = inspector::Inspector::attach(application);
  inspector.open();
  inspector.show(inspector::InspectorPanel::tokens);
  run(providers, application, 6);
  const composition::Tree& tree = application.root().tree();
  for (const appearance::TokenGroup group : appearance::ThemeToken::groups) {
    EXPECT_TRUE(tree.find(composition::ElementKey{
                              inspector::TokensPanel::group_key(group)})
                    .has_value());
  }
  EXPECT_TRUE(tree.find(composition::ElementKey{inspector::TokensPanel::row_key(
                            token("colours.accent.light"))})
                  .has_value());
  EXPECT_FALSE(
      tree.find(composition::ElementKey{inspector::TokensPanel::row_key(
                    token("colours.accent.dark"))})
          .has_value());
  ASSERT_TRUE(model.edit(token("spacing.lg"), 30.0F));
  run(providers, application, 6);
  EXPECT_TRUE(application.root().activate(composition::ElementKey{
      std::string{inspector::TokensPanel::export_key}}));
  run(providers, application, 6);
  EXPECT_EQ(inspector.model().export_status(),
            "themes/arisa-theme-glass-edited.toml");
  EXPECT_TRUE(folder_
                  .text(*provider::AssetPath::create(
                      "themes/arisa-theme-glass-edited.toml"))
                  .contains("lg = 30"));
}

TEST_F(TokensTest, ExportCreatesTheThemesFolder) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, false);
  const appearance::AppearanceModel& model = *application.appearance();
  inspector::Inspector inspector = inspector::Inspector::attach(application);
  inspector.open();
  inspector.show(inspector::InspectorPanel::tokens);
  const provider::AssetPath themes = *provider::AssetPath::create("themes");
  ASSERT_FALSE(folder_.has_directory(themes));
  ASSERT_TRUE(model.edit(token("radii.card"), 9.0F));
  run(providers, application, 6);
  EXPECT_TRUE(application.root().activate(composition::ElementKey{
      std::string{inspector::TokensPanel::export_key}}));
  run(providers, application, 6);
  EXPECT_TRUE(folder_.has_directory(themes));
  EXPECT_EQ(inspector.model().export_status(),
            "themes/arisa-theme-glass-edited.toml");
  EXPECT_FALSE(folder_
                   .text(*provider::AssetPath::create(
                       "themes/arisa-theme-glass-edited.toml"))
                   .empty());
}

TEST_F(TokensTest, AFailedExportShowsTheErrorInTheStatusLine) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, false);
  const appearance::AppearanceModel& model = *application.appearance();
  FailingStorage failing;
  inspector::Inspector inspector = inspector::Inspector::attach(
      application, inspector::InspectorOptions{.exports = std::ref(failing)});
  inspector.open();
  inspector.show(inspector::InspectorPanel::tokens);
  ASSERT_TRUE(model.edit(token("radii.card"), 9.0F));
  run(providers, application, 6);
  EXPECT_TRUE(application.root().activate(composition::ElementKey{
      std::string{inspector::TokensPanel::export_key}}));
  run(providers, application, 6);
  EXPECT_EQ(inspector.model().export_status(),
            "themes/arisa-theme-glass-edited.toml:0: the theme could not be "
            "written: resource exhausted (error 28)");
  const composition::Tree& tree = application.root().tree();
  bool shown = false;
  walk(tree.root(), [&shown](const composition::Node& node) {
    if (const auto text = node.content_as<components::Text::Content>()) {
      shown = shown || text->get().text().find("error 28") != std::string::npos;
    }
  });
  EXPECT_TRUE(shown);
}

TEST_F(TokensTest, EditsWithinOneFrameApplyTheThemeOnce) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, false);
  const appearance::AppearanceModel& model = *application.appearance();
  const appearance::ThemeToken& accent = token("colours.accent.light");
  ASSERT_TRUE(model.edit(accent, theme::Colour::hex(0x102030)));
  run(providers, application, 1);
  const std::uint64_t once = application.last_frame().evaluations;
  const composition::ObservableVersion before = model.settings().version();
  for (const std::uint32_t shade :
       {0x203040U, 0x304050U, 0x405060U, 0x506070U}) {
    ASSERT_TRUE(model.edit(accent, theme::Colour::hex(shade)));
  }
  EXPECT_FALSE(model.edit(accent, theme::Colour::hex(0x506070)));
  run(providers, application, 1);
  EXPECT_NE(model.settings().version(), before);
  EXPECT_EQ(application.last_frame().evaluations, once);
  EXPECT_EQ(std::get<theme::Colour>(accent.read(model.theme())),
            appearance::ThemeToken::quantised(theme::Colour::hex(0x506070)));
}

TEST_F(TokensTest, ColourSliderDragRepaintsAndRelaysOutOnlyWhatChanged) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start_gallery(
      providers, examples::GalleryPage::controls, theme::ColorScheme::light,
      provider::headless::ScriptedSurface::default_size, options());
  run(providers, application, settle_frames);
  inspector::Inspector inspector = inspector::Inspector::attach(application);
  inspector.open();
  inspector.show(inspector::InspectorPanel::tokens);
  const appearance::ThemeToken& accent = token("colours.accent.light");
  inspector.model().select_token(accent.index());
  run(providers, application, 30);
  const theme::Colour before =
      std::get<theme::Colour>(accent.read(application.appearance()->theme()));
  static_cast<void>(drag(providers, application, 30));
  const DragCost cost = drag(providers, application, 60);
  EXPECT_NE(
      std::get<theme::Colour>(accent.read(application.appearance()->theme())),
      before);
  EXPECT_LT(cost.recorded * 4.0, static_cast<double>(cost.nodes));
  EXPECT_LT(cost.synced * 4.0, static_cast<double>(cost.nodes));
  std::println(
      "token drag: {} nodes, {:.0f} recorded and {:.0f} laid out per frame",
      cost.nodes, cost.recorded, cost.synced);
}

TEST_F(TokensTest, AnOpenIdleInspectorDoesNoWork) {
  provider::headless::HeadlessProviderFactory providers;
  load_fonts(providers);
  auto created = Application::create(
      providers,
      composition::Element{components::Text("Idle")}.padding(theme::space::lg),
      ApplicationOptions{
          .executors = std::nullopt,
          .fonts = examples::GalleryFonts::load(providers.assets()),
          .atlas = {},
          .interaction = {},
          .appearance = options()});
  ASSERT_TRUE(created.has_value());
  Application application = std::move(*created);
  run(providers, application, settle_frames);
  inspector::Inspector inspector = inspector::Inspector::attach(application);
  inspector.open();
  inspector.show(inspector::InspectorPanel::tokens);
  inspector.model().select_token(token("colours.accent.light").index());
  run(providers, application, 30);
  const composition::Tree& tree = application.root().tree();
  const auto slider = tree.find(composition::ElementKey{
      std::string{inspector::TokenEditor::saturation_key}});
  ASSERT_TRUE(slider.has_value());
  const auto inside = [&tree](std::optional<composition::NodeId> node,
                              composition::NodeId ancestor) {
    for (std::optional<composition::NodeId> at = node; at.has_value();) {
      if (*at == ancestor) {
        return true;
      }
      const auto found = tree.node(*at);
      at = found.has_value() ? found->get().parent() : std::nullopt;
    }
    return false;
  };
  for (int presses = 0;
       presses < 160 && !inside(application.focused(), *slider); ++presses) {
    providers.input().push(pad({provider::Button::dpad_down}));
    run(providers, application, 1);
    providers.input().push(pad({}));
    run(providers, application, 1);
  }
  ASSERT_TRUE(inside(application.focused(), *slider));
  for (int step = 0; step < 12; ++step) {
    providers.input().push(pad({provider::Button::dpad_right}));
    run(providers, application, 1);
    providers.input().push(pad({}));
    run(providers, application, 1);
  }
  run(providers, application, 120);
  ASSERT_TRUE(inside(application.focused(), *slider));
  const std::uint64_t examined = tree.examined_transactions();
  const std::uint64_t frames = inspector.model().frames().get();
  const std::uint64_t own = inspector.model().own_frames();
  std::uint64_t work = 0;
  std::uint64_t allocations = 0;
  for (int frame = 0; frame < 300; ++frame) {
    ASSERT_TRUE(providers.clock().advance(frame_step).has_value());
    const std::optional<std::uint64_t> counted =
        provider::AllocationMeter::count();
    ASSERT_TRUE(application.frame().has_value());
    if (counted.has_value()) {
      allocations += *provider::AllocationMeter::count() - *counted;
    }
    const FrameTimings& timings = application.last_frame();
    EXPECT_EQ(timings.kind, FrameKind::idle) << frame;
    work += (timings.presented ? 1U : 0U) + timings.evaluations +
            timings.debug_evaluations + (timings.laid_out ? 1U : 0U) +
            timings.focus_collections + timings.focus_refreshes +
            (application.waiting() ? 0U : 1U);
  }
  EXPECT_EQ(work, 0U);
  EXPECT_EQ(allocations, 0U);
  EXPECT_EQ(tree.examined_transactions(), examined);
  EXPECT_EQ(inspector.model().frames().get(), frames);
  EXPECT_EQ(inspector.model().own_frames(), own);
}

TEST(ThemeTokenCatalogue, EveryTokenRoundTripsThroughAThemeFile) {
  const theme::Theme base = theme::Theme::defaults();
  for (const appearance::ThemeToken& token : appearance::ThemeToken::all()) {
    appearance::TokenValue changed = token.read(base);
    if (auto* colour = std::get_if<theme::Colour>(&changed)) {
      *colour = theme::Colour::hex(0x3A7BD5, 0.8F);
    } else if (auto* number = std::get_if<float>(&changed)) {
      const appearance::TokenRange range = *token.range();
      *number = std::get<float>(*token.clamp(static_cast<float>(
          range.minimum + ((range.maximum - range.minimum) * 0.37))));
    } else if (auto* on = std::get_if<bool>(&changed)) {
      *on = !*on;
    } else {
      auto& choice = std::get<std::size_t>(changed);
      choice = (choice + 1) % token.choices().size();
    }
    const theme::Theme edited = token.write(base, changed);
    const std::string text =
        appearance::ThemeFile::serialise(edited, base, "probe", "default");
    auto parsed = appearance::ThemeFile::parse(
        text, "probe.toml",
        [&base](std::string_view)
            -> std::expected<theme::Theme, appearance::ThemeError> {
          return base;
        });
    ASSERT_TRUE(parsed.has_value())
        << token.path() << ": " << parsed.error().description() << "\n"
        << text;
    EXPECT_EQ(token.read(*parsed), token.read(edited)) << token.path();
  }
  EXPECT_GT(appearance::ThemeToken::all().size(), 300U);
}

}
