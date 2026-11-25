#include <arisa/appearance/appearance_settings.hpp>
#include <arisa/appearance/theme_token.hpp>
#include <arisa/appearance/theme_vocabulary.hpp>
#include <arisa/appearance/theme_watcher.hpp>
#include <arisa/components/button.hpp>
#include <arisa/components/text.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/node.hpp>
#include <arisa/provider/headless/manual_clock.hpp>
#include <arisa/provider/headless/scripted_system_info.hpp>
#include <arisa/provider/providers.hpp>
#include <arisa/provider/sdl3/sdl3_factory.hpp>
#include <arisa/provider/standard/directory_storage.hpp>
#include <arisa/runtime/application.hpp>
#include <arisa/runtime/application_options.hpp>
#include <arisa/runtime/frame_timings.hpp>
#include <arisa/runtime/root.hpp>
#include <arisa/structure/tab_view.hpp>
#include <arisa/text/locale.hpp>
#include <arisa/theme/dynamic_type.hpp>
#include <arisa/theme/seed.hpp>
#include <capture_clock.hpp>
#include <desktop_example.hpp>
#include <effects_page.hpp>
#include <example_options.hpp>
#include <frame_meter.hpp>
#include <gallery_fonts.hpp>
#include <gallery_page.hpp>
#include <gallery_photo.hpp>
#include <gallery_screen.hpp>
#include <input_blocker.hpp>
#include <launch_screen.hpp>
#include <motion_page.hpp>
#include <navigation_page.hpp>
#include <presentations_page.hpp>

#ifdef ARISA_INSPECTOR
#include <arisa/inspector/inspector.hpp>
#include <arisa/inspector/inspector_panel.hpp>
#endif

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <optional>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace arisa::examples {

class GalleryApp {
 public:
  static constexpr std::uint32_t activation_frames = 40;
  static constexpr std::uint32_t inspector_frame = 3;

#ifdef ARISA_INSPECTOR
  static void inspect(inspector::Inspector& inspector, runtime::Root& root,
                      inspector::InspectorPanel panel,
                      const std::optional<std::string>& key,
                      const std::optional<std::string>& token) {
    inspector.open();
    inspector.show(panel);
    if (!key.has_value()) {
      return;
    }
    if (panel != inspector::InspectorPanel::tokens) {
      inspector.model().select(root.tree().find(composition::ElementKey{*key}));
      return;
    }
    const auto found = appearance::ThemeToken::find(*key);
    if (!found.has_value()) {
      return;
    }
    inspector.model().show_tokens(found->get().group());
    inspector.model().select_token(found->get().index());
    if (token.has_value()) {
      if (const auto value = typed(found->get(), *token)) {
        inspector.model().edit_token(found->get().index(), *value);
      }
    }
  }

  static auto typed(const appearance::ThemeToken& token, std::string_view text)
      -> std::optional<appearance::TokenValue> {
    switch (token.kind()) {
      case appearance::TokenKind::colour:
        return appearance::ThemeVocabulary::colour(text).transform(
            [](theme::Colour colour) {
              return appearance::TokenValue{colour};
            });
      case appearance::TokenKind::number: {
        float number = 0.0F;
        const auto parsed =
            std::from_chars(text.data(), text.data() + text.size(), number);
        if (parsed.ec != std::errc{}) {
          return std::nullopt;
        }
        return appearance::TokenValue{number};
      }
      case appearance::TokenKind::toggle:
        return appearance::TokenValue{text == "on" || text == "true"};
      case appearance::TokenKind::choice:
        break;
    }
    const auto choices = token.choices();
    const auto found = std::ranges::find(choices, text);
    if (found == choices.end()) {
      return std::nullopt;
    }
    return appearance::TokenValue{
        static_cast<std::size_t>(found - choices.begin())};
  }
#endif

  static auto activate(runtime::Root& root, const composition::ElementKey& key)
      -> bool {
    const auto found = root.tree().find(key);
    if (!found.has_value()) {
      return false;
    }
    if (root.activate(*found)) {
      return true;
    }
    const auto node = root.tree().node(*found);
    if (!node.has_value()) {
      return false;
    }
    if (activate_within(root, node->get())) {
      return true;
    }
    for (std::optional<composition::NodeId> above = node->get().parent();
         above.has_value();) {
      if (root.activate(*above)) {
        return true;
      }
      const auto parent = root.tree().node(*above);
      above = parent.has_value() ? parent->get().parent() : std::nullopt;
    }
    return false;
  }

  static auto activate_within(runtime::Root& root,
                              const composition::Node& node) -> bool {
    for (const composition::Node& child : node.children()) {
      if (root.activate(child.id()) || activate_within(root, child)) {
        return true;
      }
    }
    return false;
  }

  static auto appearance(
      const ExampleOptions& options,
      std::optional<provider::standard::DirectoryStorage>& storage,
      std::optional<provider::standard::DirectoryStorage>& themes)
      -> runtime::AppearanceOptions {
    runtime::AppearanceOptions chosen{
        .policy = GalleryScreen::policy(options.scheme()),
        .storage = {},
        .themes = {},
        .hot_reload = {}};
    if (storage.has_value()) {
      chosen.storage = *storage;
    }
    if (themes.has_value()) {
      chosen.themes = *themes;
#ifndef NDEBUG
      chosen.hot_reload = appearance::ThemeWatcher::default_interval;
#endif
    }
    if (options.theme().has_value()) {
      chosen.policy.defaults.theme = *options.theme();
    }
    if (options.sidebar().has_value()) {
      if (const auto visibility =
              appearance::ThemeVocabulary::visibility(*options.sidebar())) {
        chosen.policy.defaults.sidebar = *visibility;
      }
    }
    if (options.text_size().has_value()) {
      if (const auto size = theme::DynamicType::parse(*options.text_size())) {
        chosen.policy.defaults.dynamic_type_size = *size;
      }
    }
    if (const std::optional<float> scale = options.ui_scale()) {
      chosen.policy.defaults.ui_scale = appearance::UiScales{
          .handheld = scale, .docked = scale, .desktop = scale};
    }
    return chosen;
  }

  static auto run(const ExampleOptions& options) -> int {
    GalleryPage page = GalleryPage::typography;
    std::optional<LaunchScreen::Style> launch;
    if (!options.page().has_value() && !options.capture().has_value() &&
        !options.offscreen().has_value()) {
      launch = LaunchScreen::Style{};
    }
    using Ending = LaunchScreen::Ending;
    using Ink = LaunchScreen::Ink;
    std::string requested{options.page().value_or("")};
    constexpr std::string_view drawn = "-path";
    const bool path_only = requested.ends_with(drawn);
    if (path_only) {
      requested.resize(requested.size() - drawn.size());
    }
    if (requested == "writing") {
      launch = LaunchScreen::Style{};
    } else if (requested == "writing-erase") {
      launch = LaunchScreen::Style{.ending = Ending::erase};
    } else if (requested == "writing-gradient") {
      launch = LaunchScreen::Style{.ink = Ink::gradient};
    } else if (requested == "writing-glass") {
      launch = LaunchScreen::Style{.ink = Ink::glass};
    } else if (requested == "writing-erase-glass") {
      launch = LaunchScreen::Style{.ending = Ending::erase, .ink = Ink::glass};
    } else if (requested == "writing-erase-gradient") {
      launch =
          LaunchScreen::Style{.ending = Ending::erase, .ink = Ink::gradient};
    } else if (options.page().has_value()) {
      const auto parsed = GalleryPages::parse(*options.page());
      if (!parsed.has_value()) {
        std::println(stderr, "unknown page {}", *options.page());
        return 2;
      }
      page = *parsed;
    }
    const std::optional<std::uint32_t> motion = options.motion_milliseconds();
    provider::headless::ManualClock manual;
    CaptureClock fixed;
    provider::headless::ScriptedSystemInfo status;
    status.set_battery(provider::BatteryState{
        .percent = provider::BatteryPercent::of<80>(), .charging = false});
    status.set_network(
        provider::NetworkState{.kind = provider::NetworkKind::wifi,
                               .signal_bars = provider::SignalBars::of<3>(),
                               .address = {}});
    provider::Providers overrides;
    if (options.capture().has_value()) {
      overrides.system_info = status;
      overrides.clock = fixed;
    }
    if (motion.has_value()) {
      manual.set_wall(CaptureClock::fixed_wall);
      overrides.clock = manual;
    }
    auto factory = DesktopExample::factory(options, "arisa gallery", overrides);
    if (!factory.has_value()) {
      std::println(stderr, "{}", factory.error());
      return 1;
    }
    FrameMeter meter;
    std::optional<provider::standard::DirectoryStorage> storage;
    std::optional<provider::standard::DirectoryStorage> themes;
    std::optional<provider::standard::DirectoryStorage> files;
    if (!options.capture().has_value() && !options.offscreen().has_value()) {
      storage.emplace(provider::standard::DirectoryStorage::application_config(
          "arisa-gallery"));
      themes.emplace(
          provider::sdl3::Sdl3ProviderFactory::default_assets_root() / "arisa");
      if (const char* home = std::getenv("HOME"); home != nullptr) {
        files.emplace(std::filesystem::path{home});
      }
    }
    const LaunchScreen::Photo photo =
        GalleryPhoto::load(factory->assets(), factory->surface().size());
    const auto blocker = std::make_shared<InputBlocker>();
    auto application =
        runtime::Application::create(
            *factory,
            composition::Element{
                GalleryScreen{page,
                              theme::Seed{.scheme = options.scheme().value_or(
                                              theme::ColorScheme::light)},
                              std::cref(meter), std::nullopt, launch,
                              path_only
                                  ? nullptr
                                  : GalleryFonts::lettering(factory->assets()),
                              photo}}
                .environment(InputBlocker::key, blocker),
            runtime::ApplicationOptions{
                .executors = std::nullopt,
                .fonts = GalleryFonts::load(factory->assets()),
                .atlas = {},
                .interaction = {},
                .appearance = appearance(options, storage, themes),
                .launch_colour =
                    launch.has_value()
                        ? std::optional{theme::Colour::hex(
                              photo != nullptr ? GalleryPhoto::average
                                               : LaunchScreen::wallpaper[5])}
                        : std::nullopt,
                .debug_glass_coverage = options.debug_glass_coverage(),
                .storage = storage.has_value()
                               ? runtime::StorageOptions{.provider = *storage}
                               : runtime::StorageOptions{},
                .localization =
                    runtime::LocalizationOptions{
                        .locale = options.locale().has_value()
                                      ? text::Locale::create(*options.locale())
                                      : std::nullopt,
                        .strings = std::string{GalleryScreen::strings}},
                .files = files.has_value()
                             ? std::optional<std::reference_wrapper<
                                   provider::StorageProvider>>{*files}
                             : std::nullopt});
    if (!application.has_value()) {
      std::println(stderr, "the application could not start: {}",
                   application.error().explanation());
      return 1;
    }
    blocker->attach(application->root());
#ifdef ARISA_INSPECTOR
    inspector::Inspector inspector = inspector::Inspector::attach(*application);
    std::optional<inspector::InspectorPanel> inspected;
    if (options.inspector().has_value()) {
      inspected = inspector::InspectorPanels::parse(*options.inspector());
      if (!inspected.has_value()) {
        std::println(stderr, "unknown inspector panel {}",
                     *options.inspector());
        return 2;
      }
    }
#endif
    std::optional<Choreography> choreography;
    if (motion.has_value()) {
      choreography.emplace(manual, std::chrono::milliseconds{*motion}, page,
                           launch.has_value());
    }
    const bool play = !motion.has_value() && page == GalleryPage::motion &&
                      !options.tour() && !launch.has_value();
    std::optional<Tour> tour;
    if (options.tour()) {
      tour.emplace();
    }
    const std::optional<std::uint32_t> limit = options.frames();
    Tally tally;
    if (auto ran = application->run_until([&](runtime::Root& root) {
          tally.add(application->last_frame());
          meter.record(application->last_frame(),
                       factory->clock().now().since_origin());
#ifdef ARISA_INSPECTOR
          if (inspected.has_value() && tally.frames() == inspector_frame) {
            inspect(inspector, root, *inspected, options.inspect(),
                    options.token());
          }
#endif
          if (choreography.has_value()) {
            return choreography->step(root, tally.frames());
          }
          if (tour.has_value()) {
            return tour->step(root);
          }
          if (play && tally.frames() == 1) {
            root.activate(composition::ElementKey{MotionPage::play_button});
          }
          const std::vector<std::string>& keys = options.activations();
          const std::uint32_t frame = tally.frames();
          if (frame % activation_frames == 1 &&
              frame / activation_frames < keys.size()) {
            activate(root,
                     composition::ElementKey{keys[frame / activation_frames]});
          }
          return limit.has_value() && tally.frames() >= *limit;
        });
        !ran.has_value()) {
      std::println(stderr, "frame {} failed: {}", tally.frames(),
                   ran.error().explanation());
      return 1;
    }
    tally.print(GalleryPages::name(page));
    if (options.capture().has_value()) {
      if (auto captured = DesktopExample::capture(
              factory->gpu(), *options.capture(), options.lcd_capture());
          !captured.has_value()) {
        std::println(stderr, "{}", captured.error());
        return 1;
      }
    }
    return 0;
  }

 private:
  class Choreography {
   public:
    static constexpr std::chrono::nanoseconds frame_step{16'666'667};

    Choreography(provider::headless::ManualClock& clock,
                 std::chrono::nanoseconds target, GalleryPage page,
                 bool launching) noexcept
        : clock_(clock), target_(target), page_(page), launching_(launching) {}

    auto step(runtime::Root& root, std::uint32_t frame) -> bool {
      if (frame == 1 && !launching_) {
        if (page_ == GalleryPage::effects) {
          for (const std::string_view key : EffectsPage::choreography) {
            root.activate(composition::ElementKey{key});
          }
        } else {
          root.activate(composition::ElementKey{MotionPage::play_button});
        }
        return false;
      }
      if (launching_) {
        const std::chrono::nanoseconds now = clock_.get().now().since_origin();
        if (!start_.has_value()) {
          start_ = now;
        }
        elapsed_ = now - *start_;
      }
      if (elapsed_ >= target_) {
        return true;
      }
      const std::chrono::nanoseconds step =
          std::min(frame_step, target_ - elapsed_);
      if (!clock_.get().advance(step).has_value()) {
        return true;
      }
      elapsed_ += step;
      return false;
    }

   private:
    std::reference_wrapper<provider::headless::ManualClock> clock_;
    std::chrono::nanoseconds target_;
    GalleryPage page_;
    bool launching_{false};
    std::optional<std::chrono::nanoseconds> start_;
    std::chrono::nanoseconds elapsed_{0};
  };

  class Tour {
   public:
    static constexpr std::uint32_t settle_frames = 30;
    static constexpr std::uint32_t toast_frames = 240;

    Tour() {
      for (std::size_t index = 0; index < GalleryPages::all.size(); ++index) {
        plan(Step{
            .key =
                std::string{structure::TabView::item_key(index).description()},
            .label = {},
            .wait = settle_frames});
        const GalleryPage page = GalleryPages::all[index];
        if (page == GalleryPage::presentations) {
          present(PresentationsPage::sheet_button, "Done");
          present(PresentationsPage::cover_button, "Close");
          present(PresentationsPage::alert_button, "Cancel");
          present(PresentationsPage::dialog_button, "Cancel");
          present(PresentationsPage::popover_button, "Got it");
          plan(Step{.key = std::string{PresentationsPage::toast_button},
                    .label = {},
                    .wait = toast_frames});
        }
        if (page == GalleryPage::navigation) {
          plan(Step{.key = std::string{NavigationPage::jump_button},
                    .label = {},
                    .wait = settle_frames});
          plan(Step{.key = std::string{NavigationPage::Detail::deeper_link},
                    .label = {},
                    .wait = settle_frames});
          plan(Step{.key = std::string{NavigationPage::Detail::root_button},
                    .label = {},
                    .wait = settle_frames});
        }
      }
    }

    auto step(runtime::Root& root) -> bool {
      if (waiting_ > 0) {
        --waiting_;
        return false;
      }
      if (next_ >= steps_.size()) {
        return true;
      }
      const Step& current = steps_[next_++];
      if (!current.key.empty()) {
        root.activate(composition::ElementKey{current.key});
      } else if (const auto found =
                     labelled(root.tree().root(), current.label)) {
        root.activate(*found);
      }
      waiting_ = current.wait;
      return false;
    }

   private:
    struct Step {
      std::string key;
      std::string label;
      std::uint32_t wait{0};
    };

    void plan(Step step) { steps_.push_back(std::move(step)); }

    void present(std::string_view opener, std::string_view closer) {
      plan(
          Step{.key = std::string{opener}, .label = {}, .wait = settle_frames});
      plan(
          Step{.key = {}, .label = std::string{closer}, .wait = settle_frames});
    }

    static auto labelled(const composition::Node& node, std::string_view label)
        -> std::optional<composition::NodeId> {
      if (const auto button = node.content_as<components::Button::Content>()) {
        if (button->get().label() == label || spells(node, label)) {
          return node.id();
        }
      }
      for (const composition::Node& child : node.children()) {
        if (const auto found = labelled(child, label)) {
          return found;
        }
      }
      return std::nullopt;
    }

    static auto spells(const composition::Node& node, std::string_view label)
        -> bool {
      if (const auto text = node.content_as<components::Text::Content>()) {
        if (text->get().text() == label) {
          return true;
        }
      }
      return std::ranges::any_of(node.children(),
                                 [label](const composition::Node& child) {
                                   return spells(child, label);
                                 });
    }

    std::vector<Step> steps_;
    std::size_t next_{0};
    std::uint32_t waiting_{0};
  };

  class Tally {
   public:
    void add(const runtime::FrameTimings& timings) {
      ++frames_;
      layouts_ += timings.laid_out ? 1U : 0U;
      if (!timings.presented) {
        ++idle_;
        return;
      }
      last_ = timings;
      ++presented_;
      blurs_ += timings.blurs;
      blur_hits_ += timings.blur_hits;
      layer_renders_ += timings.layer_renders;
      layer_hits_ += timings.layer_hits;
      if (timings.effects_gpu.has_value()) {
        ++effect_frames_;
        effects_total_ += *timings.effects_gpu;
        effects_peak_ = std::max(effects_peak_, *timings.effects_gpu);
      }
      if (timings.interface_gpu.has_value()) {
        ++interface_frames_;
        interface_total_ += *timings.interface_gpu;
      }
      gpu_measured_ = timings.gpu_measured;
      cpu_total_ += timings.cpu();
      cpu_peak_ = std::max(cpu_peak_, timings.cpu());
      present_total_ += timings.present;
      if (timings.kind == runtime::FrameKind::animation) {
        ++animation_frames_;
        animation_cpu_ += timings.cpu();
        animation_layouts_ += timings.laid_out ? 1U : 0U;
      }
    }

    auto frames() const noexcept -> std::uint32_t { return frames_; }

    void print(std::string_view page) const {
      const auto milliseconds = [](std::chrono::nanoseconds value) {
        return std::chrono::duration<double, std::milli>(value).count();
      };
      const auto average = [](std::chrono::nanoseconds total,
                              std::uint32_t count) {
        return total /
               static_cast<std::int64_t>(std::max<std::uint32_t>(count, 1));
      };
      std::println(
          "gallery {}: {} frames ({} presented, {} idle), {} layouts, cpu avg "
          "{:.3f} ms max {:.3f} ms, present avg {:.3f} ms, {} animation-only "
          "frames (cpu avg {:.3f} ms, {} layouts), {} quads, {} commands",
          page, frames_, presented_, idle_, layouts_,
          milliseconds(average(cpu_total_, presented_)),
          milliseconds(cpu_peak_),
          milliseconds(average(present_total_, presented_)), animation_frames_,
          milliseconds(average(animation_cpu_, animation_frames_)),
          animation_layouts_, last_.quads, last_.commands);
      std::println(
          "gallery {} effects: {} backdrops, {} blurs, {} cached blurs, {} "
          "effect passes, {} layer renders, {} layer hits, effects {} avg "
          "{:.3f} ms max {:.3f} ms over {} frames, interface avg {:.3f} ms",
          page, last_.backdrops, blurs_, blur_hits_, last_.effect_passes,
          layer_renders_, layer_hits_, gpu_measured_ ? "gpu" : "cpu-side",
          milliseconds(average(effects_total_, effect_frames_)),
          milliseconds(effects_peak_), effect_frames_,
          milliseconds(average(interface_total_, interface_frames_)));
    }

   private:
    std::uint32_t frames_{0};
    std::uint32_t presented_{0};
    std::uint32_t idle_{0};
    std::uint32_t layouts_{0};
    std::uint32_t animation_frames_{0};
    std::uint32_t animation_layouts_{0};
    std::chrono::nanoseconds cpu_total_{0};
    std::chrono::nanoseconds cpu_peak_{0};
    std::chrono::nanoseconds present_total_{0};
    std::chrono::nanoseconds animation_cpu_{0};
    std::uint32_t blurs_{0};
    std::uint32_t blur_hits_{0};
    std::uint32_t layer_renders_{0};
    std::uint32_t layer_hits_{0};
    std::uint32_t effect_frames_{0};
    std::uint32_t interface_frames_{0};
    std::chrono::nanoseconds effects_total_{0};
    std::chrono::nanoseconds effects_peak_{0};
    std::chrono::nanoseconds interface_total_{0};
    bool gpu_measured_{false};
    runtime::FrameTimings last_;
  };
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
  return arisa::examples::GalleryApp::run(*options);
}
