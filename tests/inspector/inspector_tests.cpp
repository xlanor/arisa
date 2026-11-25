#include <arisa/components/button.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/observable.hpp>
#include <arisa/inspector/inspector.hpp>
#include <arisa/inspector/inspector_view.hpp>
#include <arisa/inspector/timeline_scale.hpp>
#include <arisa/inspector/tree_panel.hpp>
#include <arisa/provider/diagnostics/allocation_meter.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/headless/software_raster.hpp>
#include <arisa/runtime/frame_breakdown.hpp>
#include <arisa/support/sparse_recycling.hpp>

#include <controls_page.hpp>
#include <gallery_fixture.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <format>
#include <memory>
#include <optional>
#include <print>
#include <string>
#include <utility>
#include <vector>

namespace arisa::runtime {

class InspectorTest : public GalleryFixture {
 protected:
  struct Counter {
    int presses{0};
  };

  class Probe final : public composition::Component {
   public:
    Probe(std::shared_ptr<composition::Observable<int>> ticks,
          std::shared_ptr<Counter> counter) noexcept
        : ticks_(std::move(ticks)), counter_(std::move(counter)) {}

    auto operator==(const Probe&) const -> bool = default;

    auto body(composition::Context& context) const
        -> composition::Element override {
      const int tick = context.read(*ticks_);
      return components::VStack(
          composition::Element{
              components::Button("Press",
                                 [counter = counter_] { ++counter->presses; })}
              .frame({.width = 200.0F, .height = 60.0F})
              .id("probe.press"),
          composition::Element{
              components::Text(std::format(
                  "tick {} {}", tick,
                  std::string(static_cast<std::size_t>((tick % 7) + 1), 'x')))}
              .id("probe.expensive"),
          composition::Element{components::Text("still")}.id("probe.cheap"));
    }

   private:
    std::shared_ptr<composition::Observable<int>> ticks_;
    std::shared_ptr<Counter> counter_;
  };

  class Deep final : public composition::Component {
   public:
    static constexpr int levels = 30;

    auto operator==(const Deep&) const -> bool = default;

    auto body(composition::Context&) const -> composition::Element override {
      composition::Element nested =
          composition::Element{
              components::Text("a deeply nested leaf with a long name")}
              .id("deep.leaf");
      for (int level = 0; level < levels; ++level) {
        nested = components::VStack(std::move(nested));
      }
      return nested;
    }
  };

  static auto deep(provider::headless::HeadlessProviderFactory& providers)
      -> Application {
    load_fonts(providers);
    auto application = Application::create(
        providers, Deep{},
        ApplicationOptions{
            .executors = std::nullopt,
            .fonts = examples::GalleryFonts::load(providers.assets()),
            .atlas = {},
            .interaction = {},
            .appearance = std::nullopt});
    EXPECT_TRUE(application.has_value());
    Application started = std::move(*application);
    run(providers, started, 4);
    return started;
  }

  static auto probe(provider::headless::HeadlessProviderFactory& providers,
                    std::shared_ptr<composition::Observable<int>> ticks,
                    std::shared_ptr<Counter> counter) -> Application {
    load_fonts(providers);
    auto application = Application::create(
        providers, Probe{std::move(ticks), std::move(counter)},
        ApplicationOptions{
            .executors = std::nullopt,
            .fonts = examples::GalleryFonts::load(providers.assets()),
            .atlas = {},
            .interaction = {},
            .appearance = std::nullopt});
    EXPECT_TRUE(application.has_value());
    Application started = std::move(*application);
    run(providers, started, 4);
    return started;
  }

  static void settle(provider::headless::HeadlessProviderFactory& providers,
                     Application& application, int count = 6) {
    run(providers, application, count);
  }

  static auto inside_debug(Application& application, composition::NodeId id)
      -> bool {
    const composition::Tree& tree = application.root().tree();
    const auto node = tree.node(id);
    return node.has_value() && tree.debugs(node->get());
  }

  static auto debug_texts(Application& application)
      -> std::vector<std::string> {
    std::vector<std::string> found;
    const composition::Tree& tree = application.root().tree();
    const auto layer = tree.debug_layer();
    if (!layer.has_value()) {
      return found;
    }
    walk(tree.node(*layer)->get(), [&found](const composition::Node& node) {
      if (const auto text = node.content_as<components::Text::Content>()) {
        found.emplace_back(text->get().text());
      }
      if (const auto button = node.content_as<components::Button::Content>()) {
        found.emplace_back(button->get().label());
      }
    });
    return found;
  }

  static auto shows(Application& application, std::string_view needle) -> bool {
    return std::ranges::any_of(
        debug_texts(application),
        [needle](const std::string& text) { return text.contains(needle); });
  }

  static auto reachable(const composition::Tree& tree, composition::NodeId from,
                        composition::NodeId ancestor) -> bool {
    for (std::optional<composition::NodeId> at = from; at.has_value();) {
      if (*at == ancestor) {
        return true;
      }
      const auto node = tree.node(*at);
      if (!node.has_value()) {
        return false;
      }
      at = node->get().parent();
    }
    return false;
  }

  static auto left_target(Application& application,
                          const inspector::Inspector& inspector)
      -> std::optional<Introspection::NodeRecord> {
    const Introspection& introspection = application.introspection();
    const float limit = introspection.viewport().width -
                        inspector.model().options().panel_width - 32.0F;
    if (const auto preferred = application.root().tree().find(
            composition::ElementKey{examples::ControlsPage::first_button})) {
      const auto record = introspection.node(*preferred);
      if (record.has_value() && record->bounds.x > 8.0F &&
          record->bounds.x + record->bounds.width < limit &&
          record->bounds.y > 8.0F && record->bounds.width >= 24.0F &&
          record->bounds.height >= 24.0F) {
        return record;
      }
    }
    for (const Introspection::NodeRecord& record : introspection.nodes()) {
      if (!record.key.empty() && record.bounds.x > 8.0F &&
          record.bounds.x + record.bounds.width < limit &&
          record.bounds.y > 8.0F && record.bounds.width >= 24.0F &&
          record.bounds.height >= 24.0F) {
        return record;
      }
    }
    return std::nullopt;
  }

  static auto press_pad(provider::headless::HeadlessProviderFactory& providers,
                        Application& application, provider::Buttons buttons) {
    providers.input().push(pad(buttons));
    settle(providers, application, 1);
    providers.input().push(pad({}));
    settle(providers, application, 3);
  }

  struct Step {
    bool presented{false};
    std::uint64_t evaluations{0};
    std::uint64_t damaged{0};
    bool laid_out{false};
    std::uint64_t allocations{0};

    friend auto operator==(const Step&, const Step&) -> bool = default;
  };

  static auto step_once(provider::headless::HeadlessProviderFactory& providers,
                        Application& application,
                        std::optional<provider::InputFrame> input) -> Step {
    if (input.has_value()) {
      providers.input().push(std::move(*input));
    }
    EXPECT_TRUE(providers.clock().advance(frame_step).has_value());
    const std::uint64_t before = provider::AllocationMeter::count().value_or(0);
    EXPECT_TRUE(application.frame().has_value());
    const std::uint64_t used =
        provider::AllocationMeter::count().value_or(0) - before;
    const FrameTimings& timings = application.last_frame();
    return Step{.presented = timings.presented,
                .evaluations = timings.evaluations,
                .damaged = timings.damaged_pixels,
                .laid_out = timings.laid_out,
                .allocations = used};
  }
};

TEST_F(InspectorTest, ClosedInspectorMatchesABuildWithoutIt) {
  const support::SparseRecycling::Suspension unshared;
  provider::headless::HeadlessProviderFactory plain_providers;
  provider::headless::HeadlessProviderFactory watched_providers;
  Application plain =
      start_gallery(plain_providers, examples::GalleryPage::controls);
  Application watched =
      start_gallery(watched_providers, examples::GalleryPage::controls);
  inspector::Inspector inspector = inspector::Inspector::attach(watched);
  run(plain_providers, plain, 240);
  run(watched_providers, watched, 240);
  const provider::PixelPoint from{.x = 760.0F, .y = 560.0F};
  std::vector<std::optional<provider::InputFrame>> script;
  script.emplace_back(touching(from, provider::TouchPhase::began));
  for (int index = 1; index <= 24; ++index) {
    script.emplace_back(touching(
        provider::PixelPoint{.x = from.x,
                             .y = from.y - (12.0F * static_cast<float>(index))},
        provider::TouchPhase::moved));
  }
  script.emplace_back(
      touching(provider::PixelPoint{.x = from.x, .y = from.y - 288.0F},
               provider::TouchPhase::ended));
  for (int index = 0; index < 90; ++index) {
    script.emplace_back(std::nullopt);
  }
  script.emplace_back(pad({provider::Button::dpad_down}));
  script.emplace_back(pad({}));
  for (int index = 0; index < 60; ++index) {
    script.emplace_back(std::nullopt);
  }
  const std::size_t plain_presented = plain_providers.gpu().frames_presented();
  const std::size_t watched_presented =
      watched_providers.gpu().frames_presented();
  std::uint64_t plain_allocations = 0;
  std::uint64_t watched_allocations = 0;
  std::size_t presented = 0;
  for (std::size_t index = 0; index < script.size(); ++index) {
    const Step expected = step_once(plain_providers, plain, script[index]);
    const Step observed = step_once(watched_providers, watched, script[index]);
    plain_allocations += expected.allocations;
    watched_allocations += observed.allocations;
    presented += expected.presented ? 1U : 0U;
    EXPECT_EQ(observed.presented, expected.presented) << "frame " << index;
    EXPECT_EQ(observed.evaluations, expected.evaluations) << "frame " << index;
    EXPECT_EQ(observed.damaged, expected.damaged) << "frame " << index;
    EXPECT_EQ(observed.laid_out, expected.laid_out) << "frame " << index;
  }
  EXPECT_GT(presented, 10U);
  EXPECT_EQ(watched_providers.gpu().frames_presented() - watched_presented,
            plain_providers.gpu().frames_presented() - plain_presented);
  EXPECT_FALSE(inspector.is_open());
  EXPECT_FALSE(watched.root().tree().debug_layer().has_value());
  if (provider::AllocationMeter::counting()) {
    EXPECT_EQ(watched_allocations, plain_allocations);
  }
  std::println(
      "closed inspector: {} frames, {} presented, allocations {} vs {}",
      script.size(), presented, watched_allocations, plain_allocations);
}

TEST_F(InspectorTest, TreeListsTheGalleryPageWithKeysAndNotItself) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start_gallery(providers, examples::GalleryPage::controls);
  inspector::Inspector inspector = inspector::Inspector::attach(application);
  inspector.open();
  settle(providers, application);
  ASSERT_TRUE(application.root().tree().debug_layer().has_value());
  const std::vector<Introspection::NodeRecord> nodes =
      application.introspection().nodes();
  EXPECT_TRUE(std::ranges::any_of(nodes, [](const auto& record) {
    return record.key == examples::ControlsPage::first_button;
  }));
  EXPECT_TRUE(std::ranges::any_of(nodes, [](const auto& record) {
    return record.key == structure::Scaffold::content_key;
  }));
  EXPECT_FALSE(std::ranges::any_of(nodes, [](const auto& record) {
    return record.key == inspector::InspectorView::panel_key;
  }));
  const std::vector<inspector::InspectorModel::Row> rows =
      inspector.model().rows();
  EXPECT_GT(rows.size(), 3U);
  EXPECT_EQ(rows.front().record.name, "Root");
  EXPECT_TRUE(std::ranges::any_of(rows, [](const auto& row) {
    return row.record.name == "GalleryScreen";
  }));
  const auto button = application.root().tree().find(
      composition::ElementKey{examples::ControlsPage::first_button});
  ASSERT_TRUE(button.has_value());
  inspector.model().select(*button);
  settle(providers, application);
  EXPECT_TRUE(std::ranges::any_of(
      inspector.model().rows(),
      [&](const auto& row) { return row.record.id == *button; }));
  EXPECT_TRUE(
      shows(application, std::string{examples::ControlsPage::first_button}));
  std::println("tree: {} nodes, {} rows shown", nodes.size(),
               inspector.model().rows().size());
}

TEST_F(InspectorTest, SelectionOutlineSitsExactlyOnTheLayoutBounds) {
  provider::headless::HeadlessProviderFactory providers;
  providers.gpu().rasterize(provider::headless::ScriptedSurface::default_size);
  Application application =
      start_gallery(providers, examples::GalleryPage::controls);
  inspector::Inspector inspector = inspector::Inspector::attach(application);
  inspector.open();
  settle(providers, application, 12);
  const auto target = left_target(application, inspector);
  ASSERT_TRUE(target.has_value());
  const auto before = providers.gpu().capture();
  ASSERT_TRUE(before.has_value());
  inspector.model().select(target->id);
  settle(providers, application, 12);
  const auto after = providers.gpu().capture();
  ASSERT_TRUE(after.has_value());
  const float scale =
      1.0F / application.introspection()
                 .points(provider::PixelPoint{.x = 1.0F, .y = 0.0F})
                 .x;
  const composition::Rect& bounds = target->bounds;
  const auto changed = [&](float x, float y) {
    const auto px = static_cast<std::uint32_t>(std::lround(x * scale));
    const auto py = static_cast<std::uint32_t>(std::lround(y * scale));
    return before->pixel(px, py) != after->pixel(px, py);
  };
  const float middle_y = bounds.y + (bounds.height * 0.5F);
  const float middle_x = bounds.x + (bounds.width * 0.5F);
  EXPECT_TRUE(changed(bounds.x, middle_y));
  EXPECT_TRUE(changed(bounds.x + bounds.width, middle_y));
  EXPECT_TRUE(changed(middle_x, bounds.y));
  EXPECT_TRUE(changed(middle_x, bounds.y + bounds.height));
  EXPECT_FALSE(changed(bounds.x + 6.0F, middle_y));
  EXPECT_FALSE(changed(bounds.x - 6.0F, middle_y));
  EXPECT_FALSE(changed(middle_x, bounds.y + 6.0F));
  EXPECT_FALSE(changed(middle_x, bounds.y - 6.0F));
  const auto outline = application.root().tree().find(
      composition::ElementKey{inspector::InspectorView::outline_key});
  ASSERT_TRUE(outline.has_value());
  const auto drawn = application.root().tree().node(*outline);
  ASSERT_TRUE(drawn->get().presented().has_value());
  const composition::Rect shown = drawn->get().presented()->bounds;
  EXPECT_NEAR(shown.x, bounds.x, 0.01F);
  EXPECT_NEAR(shown.y, bounds.y, 0.01F);
  EXPECT_NEAR(shown.width, bounds.width, 0.01F);
  EXPECT_NEAR(shown.height, bounds.height, 0.01F);
  std::println("outline at {:.1f},{:.1f} {:.1f}x{:.1f} on {}", bounds.x,
               bounds.y, bounds.width, bounds.height, target->name);
}

TEST_F(InspectorTest, PickSelectsTheNodeUnderATouch) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start_gallery(providers, examples::GalleryPage::controls);
  inspector::Inspector inspector = inspector::Inspector::attach(application);
  inspector.open();
  settle(providers, application);
  const auto target = left_target(application, inspector);
  ASSERT_TRUE(target.has_value());
  inspector.show(inspector::InspectorPanel::pick);
  settle(providers, application);
  const composition::Point point{
      .x = target->bounds.x + (target->bounds.width * 0.5F),
      .y = target->bounds.y + (target->bounds.height * 0.5F)};
  const float scale =
      1.0F / application.introspection()
                 .points(provider::PixelPoint{.x = 1.0F, .y = 0.0F})
                 .x;
  tap(providers, application,
      provider::PixelPoint{.x = point.x * scale, .y = point.y * scale});
  const auto selected = inspector.model().selected();
  ASSERT_TRUE(selected.has_value());
  EXPECT_EQ(*selected, *application.introspection().hit(point));
  EXPECT_TRUE(reachable(application.root().tree(), *selected, target->id));
  const auto record = application.introspection().node(*selected);
  ASSERT_TRUE(record.has_value());
  EXPECT_TRUE(record->bounds.contains(point.x, point.y));
  EXPECT_EQ(inspector.model().panel(), inspector::InspectorPanel::tree);
  std::println("picked {} under {:.1f},{:.1f}",
               inspector::InspectorModel::label(*record), point.x, point.y);
}

TEST_F(InspectorTest, PickCursorMovesWithTheDpadAndSelectsWithA) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start_gallery(providers, examples::GalleryPage::controls);
  inspector::Inspector inspector = inspector::Inspector::attach(application);
  inspector.open();
  settle(providers, application);
  inspector.show(inspector::InspectorPanel::pick);
  settle(providers, application);
  const composition::Point start = inspector.model().cursor();
  providers.input().push(pad({provider::Button::dpad_left}));
  settle(providers, application, 1);
  providers.input().push(pad({}));
  settle(providers, application, 1);
  EXPECT_FLOAT_EQ(inspector.model().cursor().x,
                  start.x - inspector::InspectorModel::cursor_step);
  const auto under =
      application.introspection().hit(inspector.model().cursor());
  press_pad(providers, application, {provider::Button::a});
  EXPECT_EQ(inspector.model().selected(), under);
}

TEST_F(InspectorTest, FocusPanelMatchesTheRuntimeFocusChain) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start_gallery(providers, examples::GalleryPage::focus);
  press_pad(providers, application, {provider::Button::dpad_down});
  const auto focused = application.focused();
  ASSERT_TRUE(focused.has_value());
  inspector::Inspector inspector = inspector::Inspector::attach(application);
  inspector.open();
  inspector.show(inspector::InspectorPanel::focus);
  settle(providers, application);
  const Introspection::FocusRecord& snapshot =
      inspector.model().focus_snapshot();
  ASSERT_EQ(snapshot.focused, focused);
  ASSERT_FALSE(snapshot.chain.empty());
  EXPECT_EQ(snapshot.chain.back(), *focused);
  const composition::Tree& tree = application.root().tree();
  for (std::size_t index = 1; index < snapshot.chain.size(); ++index) {
    EXPECT_EQ(tree.node(snapshot.chain[index])->get().parent(),
              snapshot.chain[index - 1]);
  }
  EXPECT_EQ(tree.node(snapshot.chain.front())->get().depth(), 1U);
  for (const composition::NodeId scope : snapshot.scopes) {
    EXPECT_TRUE(std::ranges::contains(snapshot.chain, scope));
  }
  EXPECT_TRUE(shows(application, inspector.model().describe(*focused)));
  const std::optional<composition::NodeId> below = snapshot.next.at(1);
  inspector.close();
  settle(providers, application);
  EXPECT_EQ(application.focused(), focused);
  press_pad(providers, application, {provider::Button::dpad_down});
  EXPECT_EQ(application.focused(), below.value_or(*focused));
}

TEST_F(InspectorTest, TimelineHoldsThreeHundredFramesAndDescribesTheChosenOne) {
  provider::headless::HeadlessProviderFactory providers;
  auto ticks = std::make_shared<composition::Observable<int>>(0);
  auto counter = std::make_shared<Counter>();
  Application application = probe(providers, ticks, counter);
  inspector::Inspector inspector = inspector::Inspector::attach(application);
  inspector.open();
  inspector.show(inspector::InspectorPanel::timeline);
  settle(providers, application);
  for (int index = 0; index < 340; ++index) {
    ticks->set(ticks->get() + 1);
    settle(providers, application, 1);
  }
  const inspector::FrameHistory& history = inspector.model().history();
  EXPECT_EQ(history.capacity(), 300U);
  EXPECT_EQ(history.size(), 300U);
  inspector.model().select_frame(history.at(120).number);
  settle(providers, application);
  const std::optional<std::size_t> chosen = inspector.model().selected_frame();
  ASSERT_TRUE(chosen.has_value());
  const std::string described =
      FrameBreakdown::describe(history.at(*chosen).timings, std::nullopt);
  EXPECT_EQ(inspector.model().breakdown(*chosen), described);
  EXPECT_TRUE(shows(application, described));
  EXPECT_FALSE(history.at(*chosen).timings.debug);
  EXPECT_GT(inspector.model().own_frames(), 0U);
  std::println("timeline: {} frames held, {} own frames skipped",
               history.size(), inspector.model().own_frames());
}

TEST_F(InspectorTest, CostPanelRanksTheExpensiveNodeFirst) {
  provider::headless::HeadlessProviderFactory providers;
  auto ticks = std::make_shared<composition::Observable<int>>(0);
  auto counter = std::make_shared<Counter>();
  Application application = probe(providers, ticks, counter);
  inspector::Inspector inspector = inspector::Inspector::attach(application);
  inspector.open();
  inspector.show(inspector::InspectorPanel::cost);
  settle(providers, application);
  for (int index = 0; index < 40; ++index) {
    ticks->set(ticks->get() + 1);
    settle(providers, application, 1);
  }
  const auto expensive = application.root().tree().find(
      composition::ElementKey{"probe.expensive"});
  const auto cheap =
      application.root().tree().find(composition::ElementKey{"probe.cheap"});
  ASSERT_TRUE(expensive.has_value());
  ASSERT_TRUE(cheap.has_value());
  const std::vector<inspector::CostLedger::Ranked> window =
      inspector.model().ranked();
  ASSERT_FALSE(window.empty());
  EXPECT_EQ(window.front().id, *expensive);
  EXPECT_GT(window.front().cost.shaping, 0U);
  for (const inspector::CostLedger::Ranked& entry : window) {
    if (entry.id == *cheap) {
      EXPECT_LT(entry.cost.total(), window.front().cost.total());
    }
  }
  inspector.model().set_cost_span(inspector::InspectorModel::CostSpan::frame);
  settle(providers, application);
  const std::vector<inspector::CostLedger::Ranked> frame =
      inspector.model().ranked();
  ASSERT_FALSE(frame.empty());
  EXPECT_EQ(frame.front().id, *expensive);
  EXPECT_TRUE(shows(application, inspector.model().describe(*expensive)));
  std::println("cost: top {} total {} (eval {} layout {} record {} shaping {})",
               inspector.model().describe(window.front().id),
               window.front().cost.total(), window.front().cost.evaluations,
               window.front().cost.layouts, window.front().cost.records,
               window.front().cost.shaping);
}

TEST_F(InspectorTest, GpuPanelListsAnImportedTextureAndARegisteredSurface) {
  provider::headless::HeadlessProviderFactory providers;
  auto ticks = std::make_shared<composition::Observable<int>>(0);
  auto counter = std::make_shared<Counter>();
  Application application = probe(providers, ticks, counter);
  auto image =
      std::make_shared<provider::headless::SoftwareRaster::Canvas>(64, 32);
  auto imported = providers.gpu().import_texture(
      image,
      provider::ImportDescription{
          .width = 64, .height = 32, .format = provider::TextureFormat::rgba8});
  ASSERT_TRUE(imported.has_value());
  imported->publish();
  auto surface = providers.gpu().add_surface(
      provider::SurfaceOptions{.mode = provider::SurfaceMode::underlay,
                               .drive = provider::FrameDrive::continuous,
                               .frame_rate = 30.0},
      [](const provider::headless::NullGpu::FrameCanvas&,
         const provider::SurfaceFrame&) {});
  ASSERT_TRUE(surface.has_value());
  provider::FrameHook hook = providers.gpu().add_frame_hook(
      provider::FrameStage::before_ui,
      [](const provider::headless::NullGpu::FrameCanvas&,
         const provider::FrameInfo&) {});
  inspector::Inspector inspector = inspector::Inspector::attach(application);
  inspector.open();
  inspector.show(inspector::InspectorPanel::gpu);
  settle(providers, application);
  const Introspection::GpuRecord gpu = application.introspection().gpu();
  EXPECT_EQ(gpu.inventory.backend, "null");
  EXPECT_TRUE(std::ranges::any_of(gpu.external, [&](const auto& listing) {
    return listing.texture == imported->id() && listing.generation > 0;
  }));
  ASSERT_EQ(gpu.surfaces.size(), 1U);
  EXPECT_EQ(gpu.surfaces.front().surface, surface->id());
  EXPECT_EQ(gpu.surfaces.front().mode, provider::SurfaceMode::underlay);
  EXPECT_EQ(gpu.surfaces.front().drive, provider::FrameDrive::continuous);
  ASSERT_EQ(gpu.hooks.size(), 1U);
  EXPECT_EQ(gpu.hooks.front().drive, provider::FrameDrive::on_demand);
  EXPECT_TRUE(
      shows(application, std::format("Texture {}", imported->id().value())));
  EXPECT_TRUE(
      shows(application, std::format("Surface {}", surface->id().value())));
  EXPECT_TRUE(shows(application, "underlay"));
  EXPECT_TRUE(shows(application, "continuous 30 fps"));
  EXPECT_TRUE(shows(application, gpu.inventory.report));
}

TEST_F(InspectorTest, IdleOpenInspectorWithPausePresentsNothing) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start_gallery(providers, examples::GalleryPage::typography);
  inspector::Inspector inspector = inspector::Inspector::attach(application);
  inspector.open();
  inspector.show(inspector::InspectorPanel::timeline);
  settle(providers, application, 30);
  inspector.model().set_paused(true);
  settle(providers, application, 30);
  const std::size_t presented = providers.gpu().frames_presented();
  const std::uint64_t recorded = inspector.model().history().recorded();
  settle(providers, application, 120);
  EXPECT_EQ(providers.gpu().frames_presented(), presented);
  EXPECT_EQ(inspector.model().history().recorded(), recorded);
  inspector.model().set_paused(false);
  settle(providers, application, 30);
  const std::size_t live = providers.gpu().frames_presented();
  settle(providers, application, 120);
  EXPECT_EQ(providers.gpu().frames_presented(), live);
}

TEST_F(InspectorTest, ClosingHandsFocusBackToTheApp) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start_gallery(providers, examples::GalleryPage::focus);
  press_pad(providers, application, {provider::Button::dpad_down});
  const auto focused = application.focused();
  ASSERT_TRUE(focused.has_value());
  inspector::Inspector inspector = inspector::Inspector::attach(application);
  inspector.open();
  settle(providers, application);
  press_pad(providers, application, {provider::Button::dpad_down});
  ASSERT_TRUE(application.focused().has_value());
  EXPECT_TRUE(inside_debug(application, *application.focused()));
  inspector.close();
  settle(providers, application, 12);
  EXPECT_FALSE(application.root().tree().debug_layer().has_value());
  EXPECT_EQ(application.focused(), focused);
}

TEST_F(InspectorTest, ChordAndKeyToggleTheInspector) {
  provider::headless::HeadlessProviderFactory providers;
  auto ticks = std::make_shared<composition::Observable<int>>(0);
  auto counter = std::make_shared<Counter>();
  Application application = probe(providers, ticks, counter);
  inspector::Inspector inspector = inspector::Inspector::attach(application);
  const provider::Buttons chord{provider::Button::l, provider::Button::r,
                                provider::Button::minus};
  providers.input().push(pad({provider::Button::l, provider::Button::r}));
  settle(providers, application, 1);
  EXPECT_FALSE(inspector.is_open());
  providers.input().push(pad(chord));
  settle(providers, application, 1);
  providers.input().push(pad(chord));
  settle(providers, application, 1);
  providers.input().push(pad({}));
  settle(providers, application, 3);
  EXPECT_TRUE(inspector.is_open());
  EXPECT_TRUE(application.root().tree().debug_layer().has_value());
  press_pad(providers, application, chord);
  EXPECT_FALSE(inspector.is_open());
  provider::InputFrame key;
  key.keys.push_back(
      provider::KeyEvent{.key = provider::Key::f12, .down = true});
  providers.input().push(key);
  settle(providers, application, 1);
  provider::InputFrame released;
  released.keys.push_back(
      provider::KeyEvent{.key = provider::Key::f12, .down = false});
  providers.input().push(released);
  settle(providers, application, 3);
  EXPECT_TRUE(inspector.is_open());
  providers.input().push(key);
  settle(providers, application, 3);
  EXPECT_FALSE(inspector.is_open());
  EXPECT_EQ(counter->presses, 0);
}

TEST_F(InspectorTest, TheAppReceivesInputOnlyWhileTheInspectorIsClosed) {
  provider::headless::HeadlessProviderFactory providers;
  auto ticks = std::make_shared<composition::Observable<int>>(0);
  auto counter = std::make_shared<Counter>();
  Application application = probe(providers, ticks, counter);
  const auto button =
      application.root().tree().find(composition::ElementKey{"probe.press"});
  ASSERT_TRUE(button.has_value());
  ASSERT_EQ(application.focused(), button);
  inspector::Inspector inspector = inspector::Inspector::attach(application);
  press_pad(providers, application, {provider::Button::a});
  EXPECT_EQ(counter->presses, 1);
  inspector.open();
  settle(providers, application);
  press_pad(providers, application, {provider::Button::a});
  press_pad(providers, application, {provider::Button::dpad_down});
  const float scale =
      1.0F / application.introspection()
                 .points(provider::PixelPoint{.x = 1.0F, .y = 0.0F})
                 .x;
  const auto record = application.introspection().node(*button);
  ASSERT_TRUE(record.has_value());
  tap(providers, application,
      provider::PixelPoint{
          .x = (record->bounds.x + (record->bounds.width * 0.5F)) * scale,
          .y = (record->bounds.y + (record->bounds.height * 0.5F)) * scale});
  EXPECT_EQ(counter->presses, 1);
  inspector.close();
  settle(providers, application, 12);
  ASSERT_EQ(application.focused(), button);
  press_pad(providers, application, {provider::Button::a});
  EXPECT_EQ(counter->presses, 2);
}

TEST_F(InspectorTest, DeepRowsStayLeftAlignedWithRoomForTheirLabels) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = deep(providers);
  const auto leaf =
      application.root().tree().find(composition::ElementKey{"deep.leaf"});
  ASSERT_TRUE(leaf.has_value());
  ASSERT_GE(application.introspection().node(*leaf)->depth, 30U);
  inspector::Inspector inspector = inspector::Inspector::attach(application);
  inspector.open();
  inspector.model().select(*leaf);
  settle(providers, application, 12);
  const composition::Tree& tree = application.root().tree();
  const auto panel = tree.find(composition::ElementKey{
      std::string{inspector::InspectorView::panel_key}});
  const auto label =
      tree.find(composition::ElementKey{inspector::TreePanel::row_key(*leaf)});
  ASSERT_TRUE(panel.has_value());
  ASSERT_TRUE(label.has_value());
  const composition::Rect frame = tree.node(*panel)->get().presented()->bounds;
  const composition::Rect row = tree.node(*label)->get().presented()->bounds;
  EXPECT_LT(row.x - frame.x, frame.width * 0.5F);
  EXPECT_GE(row.width, (frame.width * 0.6F) - 1.0F);
  EXPECT_LE(row.x + row.width, frame.x + frame.width);
  std::println("depth {} label at {:.1f} of a {:.1f} wide panel, {:.1f} wide",
               application.introspection().node(*leaf)->depth, row.x - frame.x,
               frame.width, row.width);
}

TEST(InspectorTimelineScale, ASpikeDoesNotRescaleOtherBars) {
  const inspector::TimelineScale cpu = inspector::TimelineScale::cpu(120.0F);
  const float small = cpu.extent(std::chrono::milliseconds{2});
  EXPECT_NEAR(small, 120.0F * 2.0F / 33.333333F, 0.01F);
  EXPECT_FLOAT_EQ(cpu.extent(std::chrono::milliseconds{120}), 120.0F);
  EXPECT_TRUE(cpu.clipped(std::chrono::milliseconds{120}));
  EXPECT_FALSE(cpu.clipped(std::chrono::milliseconds{2}));
  EXPECT_FLOAT_EQ(cpu.extent(std::chrono::milliseconds{2}), small);
  EXPECT_NEAR(cpu.extent(inspector::TimelineScale::budget), 60.0F, 0.01F);
  const inspector::TimelineScale gpu = inspector::TimelineScale::gpu(48.0F);
  EXPECT_GT(gpu.extent(std::chrono::microseconds{200}), 2.0F);
  EXPECT_FLOAT_EQ(gpu.extent(std::chrono::nanoseconds{1}),
                  inspector::TimelineScale::minimum_extent);
}

TEST(InspectorNames, ReadableStripsNamespacesAndContentSuffixes) {
  EXPECT_EQ(Introspection::readable("arisa::components::Text::Content"),
            "Text");
  EXPECT_EQ(Introspection::readable(
                "arisa::composition::ComponentContent<arisa::examples::"
                "GalleryScreen>"),
            "GalleryScreen");
  EXPECT_EQ(Introspection::readable("arisa::composition::Decoration"),
            "Decoration");
  EXPECT_EQ(composition::ElementType::of<components::Text::Content>().name(),
            "arisa::components::Text::Content");
}

}
