#include <benchmark.hpp>

#include <arisa/appearance/theme_token.hpp>
#include <arisa/components/status/clock.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/node_lifecycle_phase.hpp>
#include <arisa/composition/tree.hpp>
#include <arisa/provider/diagnostics/allocation_meter.hpp>
#include <arisa/provider/gpu/shape_vertex.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/runtime/application_options.hpp>
#include <arisa/runtime/frame_breakdown.hpp>
#include <arisa/structure/scaffold.hpp>
#include <arisa/theme/hsb.hpp>
#include <arisa/theme/material.hpp>
#include <arisa/theme/seed.hpp>

#include <gallery_fonts.hpp>
#include <gallery_page.hpp>
#include <gallery_photo.hpp>
#include <gallery_screen.hpp>
#include <launch_screen.hpp>
#include <materials_page.hpp>

#ifdef ARISA_INSPECTOR
#include <arisa/inspector/inspector.hpp>
#include <arisa/inspector/token_editor.hpp>
#endif

#include <algorithm>
#include <format>
#include <iterator>
#include <utility>
#include <variant>

namespace arisa::examples {

Benchmark::Benchmark(provider::ProviderFactory& providers,
                     provider::headless::ScriptedInput& input, Advance advance,
                     bool real_time, bool render_thread) noexcept
    : providers_(providers),
      input_(input),
      advance_(std::move(advance)),
      real_time_(real_time),
      render_thread_(render_thread),
      remote_(std::make_shared<BenchScreen::Remote>()),
      split_(std::make_shared<SplitScreen::Remote>()),
      interaction_(std::make_shared<InteractionScreen::Remote>()) {}

auto Benchmark::name(Scenario scenario) noexcept -> std::string_view {
  switch (scenario) {
    case Scenario::idle:
      return "idle";
    case Scenario::fling:
      return "fling";
    case Scenario::gallery:
      return "gallery";
    case Scenario::navigation:
      return "navigation";
    case Scenario::sheet:
      return "sheet";
    case Scenario::theme:
      return "theme";
    case Scenario::launch:
      return "launch";
    case Scenario::launch_path:
      return "launch_path";
    case Scenario::sidebar_scroll:
      return "sidebar_scroll";
    case Scenario::detail_scroll:
      return "detail_scroll";
    case Scenario::sidebar_fast:
      return "sidebar_fast";
    case Scenario::sidebar_steps:
      return "sidebar_steps";
    case Scenario::sidebar_settle:
      return "sidebar_settle";
    case Scenario::split_steps:
      return "split_steps";
    case Scenario::glass_page:
      return "glass_page";
    case Scenario::materials_page:
      return "materials_page";
    case Scenario::covered_page:
      return "covered_page";
    case Scenario::sheet_glass:
      return "sheet_glass";
    case Scenario::toggle_flip:
      return "toggle_flip";
    case Scenario::slider_drag:
      return "slider_drag";
    case Scenario::list_walk:
      return "list_walk";
    case Scenario::popover_menu:
      return "popover_menu";
    case Scenario::spinner_heavy:
      return "spinner_heavy";
    case Scenario::text_typing:
      return "text_typing";
    case Scenario::clock_tick:
      return "clock_tick";
    case Scenario::list_paging:
      return "list_paging";
    case Scenario::materials_leave:
      return "materials_leave";
    case Scenario::lists_idle:
      return "lists_idle";
    case Scenario::lists_enter:
      return "lists_enter";
    case Scenario::lists_return:
      return "lists_return";
    case Scenario::tour:
      return "tour";
    case Scenario::launch_materials:
      return "launch_materials";
    case Scenario::token_slider_drag:
      return "token_slider_drag";
  }
  return "idle";
}

auto Benchmark::parse(std::string_view wanted) noexcept
    -> std::optional<Scenario> {
  const auto found = std::ranges::find(scenarios, wanted, &Benchmark::name);
  if (found == scenarios.end()) {
    return std::nullopt;
  }
  return *found;
}

auto Benchmark::plan(Scenario scenario) noexcept -> Plan {
  switch (scenario) {
    case Scenario::idle:
      return Plan{.warmup = idle_warmup, .frames = idle_frames};
    case Scenario::fling:
      return Plan{.warmup = warmup, .frames = fling_strokes * fling_period};
    case Scenario::gallery:
      return Plan{
          .warmup = warmup,
          .frames = static_cast<std::uint32_t>(GalleryPages::all.size()) *
                    page_frames};
    case Scenario::navigation:
    case Scenario::sheet:
    case Scenario::sheet_glass:
      return Plan{.warmup = warmup, .frames = cycles * cycle_frames};
    case Scenario::theme:
      return Plan{.warmup = warmup, .frames = theme_switches * theme_frames};
    case Scenario::launch:
    case Scenario::launch_path:
      return Plan{.warmup = 0, .frames = launch_frames};
    case Scenario::launch_materials:
      return Plan{.warmup = 0, .frames = launch_materials_frames};
    case Scenario::sidebar_scroll:
    case Scenario::detail_scroll:
      return Plan{.warmup = warmup, .frames = scroll_strokes * scroll_period};
    case Scenario::sidebar_fast:
      return Plan{
          .warmup = warmup,
          .frames =
              (2 * static_cast<std::uint32_t>(GalleryPages::all.size() - 1) *
               sweep_period) +
              sweep_tail};
    case Scenario::sidebar_steps:
      return Plan{.warmup = idle_warmup, .frames = step_presses * step_period};
    case Scenario::sidebar_settle:
      return Plan{.warmup = idle_warmup,
                  .frames = settle_presses * settle_period};
    case Scenario::split_steps:
      return Plan{.warmup = idle_warmup,
                  .frames = 2 *
                            static_cast<std::uint32_t>(SplitScreen::rows - 1) *
                            step_period};
    case Scenario::glass_page:
    case Scenario::materials_page:
      return Plan{.warmup = idle_warmup,
                  .frames = still_frames + scroll_period};
    case Scenario::covered_page:
      return Plan{.warmup = idle_warmup, .frames = cover_frames + still_frames};
    case Scenario::toggle_flip:
      return Plan{.warmup = idle_warmup, .frames = flips * flip_period};
    case Scenario::slider_drag:
      return Plan{.warmup = idle_warmup,
                  .frames = (slider_strokes * slider_period) +
                            (slider_presses * step_period)};
    case Scenario::list_walk:
      return Plan{
          .warmup = idle_warmup,
          .frames =
              2 * static_cast<std::uint32_t>(InteractionScreen::list_rows - 1) *
              step_period};
    case Scenario::popover_menu:
      return Plan{.warmup = idle_warmup, .frames = menu_cycles * menu_period};
    case Scenario::spinner_heavy:
      return Plan{.warmup = idle_warmup, .frames = still_frames};
    case Scenario::text_typing:
      return Plan{.warmup = idle_warmup, .frames = keystrokes * step_period};
    case Scenario::clock_tick:
      return Plan{.warmup = idle_warmup, .frames = ticks * tick_period};
    case Scenario::list_paging:
      return Plan{.warmup = idle_warmup,
                  .frames = paging_entry + paging_hold + paging_release +
                            ((paging_forward + paging_back) * paging_period) +
                            paging_tail};
    case Scenario::lists_idle:
      return Plan{.warmup = idle_warmup, .frames = idle_frames};
    case Scenario::lists_enter:
      return Plan{.warmup = idle_warmup, .frames = tour_stay};
    case Scenario::lists_return:
      return Plan{.warmup = idle_warmup, .frames = 2 * tour_stay};
    case Scenario::token_slider_drag:
      return Plan{.warmup = idle_warmup,
                  .frames = token_strokes * token_period};
    case Scenario::materials_leave:
      return Plan{.warmup = idle_warmup,
                  .frames = still_frames + leave_tail + leave_tail};
    case Scenario::tour:
      return Plan{
          .warmup = idle_warmup,
          .frames = (static_cast<std::uint32_t>(GalleryPages::all.size() - 1) *
                     tour_stay) +
                    (static_cast<std::uint32_t>(GalleryPages::all.size() - 1) *
                     step_period) +
                    tour_tail};
  }
  return Plan{};
}

auto Benchmark::rehearsed(Scenario scenario) noexcept -> bool {
  return scenario != Scenario::idle && scenario != Scenario::launch &&
         scenario != Scenario::launch_path &&
         scenario != Scenario::sidebar_settle &&
         scenario != Scenario::list_paging &&
         scenario != Scenario::materials_leave && scenario != Scenario::tour &&
         scenario != Scenario::launch_materials &&
         scenario != Scenario::lists_idle &&
         scenario != Scenario::lists_enter &&
         scenario != Scenario::lists_return && !paged(scenario);
}

auto Benchmark::worst_from(Scenario scenario) noexcept -> std::uint32_t {
  return scenario == Scenario::lists_return ? tour_stay : 0;
}

auto Benchmark::paged(Scenario scenario) noexcept -> bool {
  return scenario == Scenario::glass_page ||
         scenario == Scenario::materials_page ||
         scenario == Scenario::covered_page;
}

auto Benchmark::still(Scenario scenario, std::uint32_t frame) noexcept -> bool {
  if (scenario == Scenario::covered_page) {
    return frame >= cover_frames;
  }
  return paged(scenario) && frame < still_frames;
}

auto Benchmark::quiet(Scenario scenario, std::uint32_t frame) noexcept -> bool {
  switch (scenario) {
    case Scenario::list_paging:
      return frame >= plan(scenario).frames - (paging_tail / 2);
    case Scenario::materials_leave:
      return frame >= still_frames + (leave_tail / 2) - leave_quiet &&
             frame < still_frames + (leave_tail / 2);
    default:
      return false;
  }
}

auto Benchmark::resumed(Scenario scenario, std::uint32_t frame) noexcept
    -> bool {
  return scenario == Scenario::materials_leave &&
         frame >= plan(scenario).frames - (leave_tail / 2);
}

auto Benchmark::steady(const Sample& sample) noexcept -> bool {
  return sample.presented && !sample.laid_out && sample.evaluations == 0 &&
         !sample.stimulus && !sample.released;
}

auto Benchmark::interactive(Scenario scenario) noexcept -> bool {
  switch (scenario) {
    case Scenario::toggle_flip:
    case Scenario::slider_drag:
    case Scenario::list_walk:
    case Scenario::popover_menu:
    case Scenario::spinner_heavy:
    case Scenario::text_typing:
    case Scenario::clock_tick:
      return true;
    default:
      return false;
  }
}

auto Benchmark::mode(Scenario scenario) noexcept -> InteractionScreen::Mode {
  switch (scenario) {
    case Scenario::slider_drag:
      return InteractionScreen::Mode::slider;
    case Scenario::list_walk:
      return InteractionScreen::Mode::list;
    case Scenario::popover_menu:
      return InteractionScreen::Mode::menu;
    case Scenario::spinner_heavy:
      return InteractionScreen::Mode::spinner;
    case Scenario::text_typing:
      return InteractionScreen::Mode::text;
    case Scenario::clock_tick:
      return InteractionScreen::Mode::clock;
    default:
      return InteractionScreen::Mode::toggle;
  }
}

auto Benchmark::subtree(const composition::Node& node) -> std::size_t {
  if (node.phase() == composition::NodeLifecyclePhase::departing) {
    return 0;
  }
  std::size_t total = 1;
  for (const composition::Node& child : node.children()) {
    total += subtree(child);
  }
  return total;
}

auto Benchmark::depth(composition::Tree& tree, const composition::Node& node)
    -> std::size_t {
  std::size_t found = 0;
  std::optional<composition::NodeId> above = node.parent();
  while (above.has_value()) {
    ++found;
    const auto parent = tree.node(*above);
    above = parent.has_value() ? parent->get().parent() : std::nullopt;
  }
  return found;
}

auto Benchmark::region(runtime::Application& application,
                       std::string_view key) const
    -> std::optional<composition::Rect> {
  composition::Tree& tree = application.root().tree();
  const auto found = tree.find(composition::ElementKey{key});
  if (!found.has_value()) {
    return std::nullopt;
  }
  const auto node = tree.node(*found);
  if (!node.has_value()) {
    return std::nullopt;
  }
  const composition::Node& held = node->get();
  return held.presented().has_value() ? held.presented()->bounds : held.frame();
}

auto Benchmark::area(runtime::Application& application, Scenario scenario) const
    -> composition::Rect {
  const composition::Rect screen = application.root().tree().root().frame();
  if (scenario == Scenario::popover_menu) {
    return screen;
  }
  const std::string_view held = scenario == Scenario::clock_tick
                                    ? structure::Scaffold::trailing_group_key
                                    : InteractionScreen::stage;
  const auto found = region(application, held);
  if (!found.has_value()) {
    return composition::Rect{};
  }
  composition::Rect united = *found;
  for (const std::string_view consumer :
       {structure::Scaffold::leading_group_key,
        structure::Scaffold::trailing_group_key}) {
    const auto over = region(application, consumer);
    if (!over.has_value() || over->x > united.x + united.width ||
        united.x > over->x + over->width ||
        over->y > united.y + united.height ||
        united.y > over->y + over->height) {
      continue;
    }
    const float left = std::min(united.x, over->x);
    const float top = std::min(united.y, over->y);
    const float right =
        std::max(united.x + united.width, over->x + over->width);
    const float bottom =
        std::max(united.y + united.height, over->y + over->height);
    united = composition::Rect{
        .x = left, .y = top, .width = right - left, .height = bottom - top};
  }
  return united;
}

auto Benchmark::pixels(const Summary& summary, float outset) -> std::uint64_t {
  const float scale = summary.scale;
  const composition::Rect& held = summary.region;
  if (held.width <= 0.0F || held.height <= 0.0F) {
    return 0;
  }
  const float width = (held.width + (2.0F * outset)) * scale;
  const float height = (held.height + (2.0F * outset)) * scale;
  return std::min(static_cast<std::uint64_t>(width * height),
                  summary.surface_pixels);
}

auto Benchmark::photo() -> LaunchScreen::Photo {
  if (!photo_.has_value()) {
    photo_ = GalleryPhoto::load(providers_.get().assets(),
                                providers_.get().surface().size());
  }
  return *photo_;
}

auto Benchmark::content(Scenario scenario) -> composition::Element {
  switch (scenario) {
    case Scenario::fling:
      return BenchScreen{BenchScreen::Mode::fling, remote_};
    case Scenario::navigation:
      return BenchScreen{BenchScreen::Mode::navigation, remote_};
    case Scenario::sheet:
      return BenchScreen{BenchScreen::Mode::sheet, remote_};
    case Scenario::sheet_glass:
      return BenchScreen{BenchScreen::Mode::sheet_glass, remote_, photo()};
    case Scenario::launch:
      return GalleryScreen{GalleryPage::typography,
                           theme::Seed{},
                           std::nullopt,
                           std::nullopt,
                           LaunchScreen::Style{},
                           GalleryFonts::lettering(providers_.get().assets()),
                           photo()};
    case Scenario::launch_materials:
      return GalleryScreen{
          GalleryPage::materials, theme::Seed{}, std::nullopt, std::nullopt,
          LaunchScreen::Style{},  nullptr,       photo()};
    case Scenario::launch_path:
      return GalleryScreen{
          GalleryPage::typography, theme::Seed{}, std::nullopt, std::nullopt,
          LaunchScreen::Style{},   nullptr,       photo()};
    case Scenario::sidebar_scroll:
    case Scenario::sidebar_fast:
      return GalleryScreen{GalleryPage::typography, theme::Seed{}};
    case Scenario::detail_scroll:
      return GalleryScreen{GalleryPage::settings, theme::Seed{}};
    case Scenario::glass_page:
      return GalleryScreen{
          GalleryPage::glass, theme::Seed{}, std::nullopt, std::nullopt,
          std::nullopt,       nullptr,       photo()};
    case Scenario::list_paging:
    case Scenario::lists_idle:
    case Scenario::lists_return:
      return GalleryScreen{GalleryPage::lists, theme::Seed{}};
    case Scenario::tour:
      return GalleryScreen{GalleryPage::typography, theme::Seed{}};
    case Scenario::lists_enter:
    case Scenario::token_slider_drag:
      return GalleryScreen{GalleryPage::controls, theme::Seed{}};
    case Scenario::materials_page:
    case Scenario::materials_leave:
    case Scenario::covered_page:
    case Scenario::sidebar_steps:
    case Scenario::sidebar_settle:
      return GalleryScreen{GalleryPage::materials, theme::Seed{}};
    case Scenario::split_steps:
      return SplitScreen{split_};
    case Scenario::toggle_flip:
    case Scenario::slider_drag:
    case Scenario::list_walk:
    case Scenario::popover_menu:
    case Scenario::spinner_heavy:
    case Scenario::text_typing:
    case Scenario::clock_tick:
      return InteractionScreen{mode(scenario), interaction_};
    case Scenario::idle:
    case Scenario::gallery:
    case Scenario::theme:
      return GalleryScreen{GalleryPage::typography, theme::Seed{}};
  }
  return GalleryScreen{GalleryPage::typography, theme::Seed{}};
}

void Benchmark::press(provider::Button button) {
  pressed_ = 2;
  provider::InputFrame down;
  down.pads[0] = provider::PadState{
      .connected = true, .buttons = {button}, .left = {}, .right = {}};
  input_.get().push(std::move(down));
  provider::InputFrame up;
  up.pads[0] = provider::PadState{
      .connected = true, .buttons = {}, .left = {}, .right = {}};
  input_.get().push(std::move(up));
}

void Benchmark::hold(provider::Button button) {
  provider::InputFrame down;
  down.pads[0] = provider::PadState{
      .connected = true, .buttons = {button}, .left = {}, .right = {}};
  input_.get().push(std::move(down));
}

auto Benchmark::page(std::uint32_t frame) -> bool {
  if (frame == 0 || frame == paging_entry / 2) {
    press(provider::Button::dpad_right);
    return true;
  }
  if (frame < paging_entry) {
    return false;
  }
  const std::uint32_t held = frame - paging_entry;
  if (held < paging_hold) {
    hold(provider::Button::dpad_down);
    return held == 0;
  }
  if (held == paging_hold) {
    provider::InputFrame up;
    up.pads[0] = provider::PadState{
        .connected = true, .buttons = {}, .left = {}, .right = {}};
    input_.get().push(std::move(up));
    return false;
  }
  const std::uint32_t paged_from = paging_hold + paging_release;
  if (held < paged_from) {
    return false;
  }
  const std::uint32_t turned = held - paged_from;
  const std::uint32_t index = turned / paging_period;
  if (turned % paging_period != 0 || index >= paging_forward + paging_back) {
    return false;
  }
  press(index < paging_forward ? provider::Button::zr : provider::Button::zl);
  return true;
}

auto Benchmark::deepest(runtime::Application& application) -> std::uint32_t {
  composition::Tree& tree = application.root().tree();
  for (int row = long_rows; row > 0; --row) {
    if (tree.find(composition::ElementKey{std::int64_t{long_key + row}})
            .has_value()) {
      return static_cast<std::uint32_t>(row);
    }
  }
  return 0;
}

void Benchmark::touch(provider::PixelPoint point, provider::TouchPhase phase) {
  provider::InputFrame frame;
  frame.touches.push_back(provider::Touch{
      .id = provider::TouchId{1}, .position = point, .phase = phase});
  input_.get().push(std::move(frame));
}

auto Benchmark::stroke(std::uint32_t frame) -> bool {
  const std::uint32_t index = frame / fling_period;
  const std::uint32_t step = frame % fling_period;
  if (index >= fling_strokes || step > fling_drag + 1) {
    return false;
  }
  drag(step, fling_drag, index < fling_strokes / 2, 0.6F);
  return step == 0;
}

auto Benchmark::sweep(std::uint32_t frame) -> bool {
  const auto rows = static_cast<std::uint32_t>(GalleryPages::all.size() - 1);
  const std::uint32_t index = frame / sweep_period;
  if (frame % sweep_period != 0 || index >= 2 * rows) {
    return false;
  }
  press(index < rows ? provider::Button::dpad_down : provider::Button::dpad_up);
  return true;
}

auto Benchmark::step(std::uint32_t frame) -> bool {
  const std::uint32_t index = frame / step_period;
  if (frame % step_period != 0 || index >= step_presses) {
    return false;
  }
  press(index % 2 == 0 ? provider::Button::dpad_down
                       : provider::Button::dpad_up);
  return true;
}

auto Benchmark::settle(std::uint32_t frame) -> bool {
  const std::uint32_t index = frame / settle_period;
  if (frame % settle_period != 0 || index >= settle_presses) {
    return false;
  }
  press(provider::Button::dpad_up);
  return true;
}

auto Benchmark::split(std::uint32_t frame) -> bool {
  const auto presses = static_cast<std::uint32_t>(SplitScreen::rows - 1);
  const std::uint32_t index = frame / step_period;
  if (frame % step_period != 0 || index >= 2 * presses) {
    return false;
  }
  press(index < presses ? provider::Button::dpad_down
                        : provider::Button::dpad_up);
  return true;
}

auto Benchmark::slide(std::uint32_t frame, runtime::Application& application)
    -> bool {
  const std::uint32_t stroked = slider_strokes * slider_period;
  if (frame >= stroked) {
    const std::uint32_t index = (frame - stroked) / step_period;
    if ((frame - stroked) % step_period != 0 || index >= slider_presses) {
      return false;
    }
    press(index < slider_presses / 2 ? provider::Button::dpad_right
                                     : provider::Button::dpad_left);
    return true;
  }
  const std::uint32_t index = frame / slider_period;
  const std::uint32_t step = frame % slider_period;
  if (step > fling_drag + 1) {
    return false;
  }
  const auto bounds = region(application, InteractionScreen::control);
  const float wide = application.root().tree().root().frame().width;
  if (!bounds.has_value() || wide <= 0.0F) {
    return false;
  }
  const float scale =
      static_cast<float>(providers_.get().surface().size().width.value()) /
      wide;
  const bool rightward = index % 2 == 0;
  const float from = rightward ? 0.15F : 0.85F;
  const float to = rightward ? 0.85F : 0.15F;
  const float progress = static_cast<float>(std::min(step, fling_drag)) /
                         static_cast<float>(fling_drag);
  const float along = from + ((to - from) * progress);
  const provider::PixelPoint point{
      .x = (bounds->x + (bounds->width * along)) * scale,
      .y = (bounds->y + (bounds->height * 0.5F)) * scale};
  if (step == 0) {
    touch(point, provider::TouchPhase::began);
    return true;
  }
  touch(point, step > fling_drag ? provider::TouchPhase::ended
                                 : provider::TouchPhase::moved);
  return true;
}

auto Benchmark::edit_token(std::uint32_t frame,
                           runtime::Application& application) -> bool {
  const std::uint32_t step = frame % token_period;
  if (step > token_drag + 1) {
    return false;
  }
  const float progress = static_cast<float>(std::min(step, token_drag)) /
                         static_cast<float>(token_drag);
  const float along = 0.1F + (0.8F * progress);
  const float at = (frame / token_period) % 2 == 0 ? along : 1.0F - along;
#ifdef ARISA_INSPECTOR
  const auto bounds =
      region(application, inspector::TokenEditor::saturation_key);
  const float wide = application.root().tree().root().frame().width;
  if (!bounds.has_value() || wide <= 0.0F) {
    return false;
  }
  const float scale =
      static_cast<float>(providers_.get().surface().size().width.value()) /
      wide;
  const provider::PixelPoint point{
      .x = (bounds->x + (bounds->width * at)) * scale,
      .y = (bounds->y + (bounds->height * 0.5F)) * scale};
  touch(point, step == 0           ? provider::TouchPhase::began
               : step > token_drag ? provider::TouchPhase::ended
                                   : provider::TouchPhase::moved);
  return true;
#else
  const auto& model = application.appearance();
  const auto token = appearance::ThemeToken::find(dragged_token);
  if (!model.has_value() || !token.has_value() || step > token_drag) {
    return false;
  }
  theme::Hsb shade = theme::Hsb::from(
      std::get<theme::Colour>(token->get().read(model->theme())));
  shade.saturation = at;
  return model->edit(token->get(), shade.colour());
#endif
}

auto Benchmark::walk(std::uint32_t frame) -> bool {
  const auto presses =
      static_cast<std::uint32_t>(InteractionScreen::list_rows - 1);
  const std::uint32_t index = frame / step_period;
  if (frame % step_period != 0 || index >= 2 * presses) {
    return false;
  }
  press(index < presses ? provider::Button::dpad_down
                        : provider::Button::dpad_up);
  return true;
}

auto Benchmark::cycle(std::uint32_t frame) -> bool {
  if (frame % (menu_period / 2) != 0) {
    return false;
  }
  press(frame % menu_period == 0 ? provider::Button::a : provider::Button::b);
  return true;
}

auto Benchmark::type(std::uint32_t frame) -> bool {
  const std::uint32_t index = frame / step_period;
  if (frame % step_period != 0 || index >= keystrokes ||
      !interaction_->text.has_value()) {
    return false;
  }
  interaction_->text->set(std::string{typed_phrase.substr(
      0, std::min<std::size_t>(index + 1, typed_phrase.size()))});
  return true;
}

auto Benchmark::tick(std::uint32_t frame) -> bool {
  if (frame % tick_period != 0) {
    return false;
  }
  provider::ClockProvider& clock = providers_.get().clock();
  const auto local = clock.wall_now().time_since_epoch() +
                     clock.local_offset().value_or(std::chrono::seconds{0});
  const auto into =
      std::chrono::duration_cast<std::chrono::nanoseconds>(local) %
      std::chrono::nanoseconds{components::status::Clock::minute};
  const std::chrono::nanoseconds until =
      std::chrono::nanoseconds{components::status::Clock::minute} - into;
  skip_ = std::max(until - std::chrono::nanoseconds{tick_lead},
                   std::chrono::nanoseconds{0});
  return true;
}

auto Benchmark::scroll(std::uint32_t frame, float column) -> bool {
  const std::uint32_t index = frame / scroll_period;
  const std::uint32_t step = frame % scroll_period;
  if (index >= scroll_strokes || step > fling_drag + 1) {
    return false;
  }
  drag(step, fling_drag, index % 2 == 0, column);
  return step == 0;
}

void Benchmark::drag(std::uint32_t step, std::uint32_t drag_frames, bool down,
                     float column) {
  const provider::SurfaceSize size = providers_.get().surface().size();
  const auto width = static_cast<float>(size.width.value());
  const auto height = static_cast<float>(size.height.value());
  const float start = height * (down ? 0.75F : 0.25F);
  const float travel = height * fling_distance * (down ? -1.0F : 1.0F);
  const float progress = static_cast<float>(std::min(step, drag_frames)) /
                         static_cast<float>(drag_frames);
  const provider::PixelPoint point{.x = width * column,
                                   .y = start + (travel * progress)};
  if (step == 0) {
    touch(point, provider::TouchPhase::began);
    return;
  }
  touch(point, step > drag_frames ? provider::TouchPhase::ended
                                  : provider::TouchPhase::moved);
}

auto Benchmark::glass_appearance() -> runtime::AppearanceOptions {
  runtime::AppearanceOptions options{
      .policy = GalleryScreen::policy(std::nullopt), .storage = std::nullopt};
  options.policy.defaults.theme = "arisa-theme-glass";
  options.policy.defaults.ui_scale = appearance::UiScales{
      .handheld = glass_scale, .docked = glass_scale, .desktop = glass_scale};
  return options;
}

auto Benchmark::stimulate(Scenario scenario, std::uint32_t frame,
                          runtime::Application& application) -> bool {
  switch (scenario) {
    case Scenario::toggle_flip:
      if (frame % flip_period == 0) {
        press(provider::Button::a);
        return true;
      }
      return false;
    case Scenario::slider_drag:
      return slide(frame, application);
    case Scenario::token_slider_drag:
      return edit_token(frame, application);
    case Scenario::list_walk:
      return walk(frame);
    case Scenario::popover_menu:
      return cycle(frame);
    case Scenario::spinner_heavy:
      return false;
    case Scenario::text_typing:
      return type(frame);
    case Scenario::clock_tick:
      return tick(frame);
    case Scenario::list_paging:
      return page(frame);
    case Scenario::tour: {
      const auto pages =
          static_cast<std::uint32_t>(GalleryPages::all.size() - 1);
      if (frame < pages * tour_stay) {
        if (frame % tour_stay == 0 && frame > 0) {
          press(provider::Button::dpad_down);
          return true;
        }
        if (frame == 0) {
          press(provider::Button::dpad_down);
          return true;
        }
        return false;
      }
      const std::uint32_t back = frame - (pages * tour_stay);
      if (back % step_period == 0 && back / step_period < pages) {
        press(provider::Button::dpad_up);
        return true;
      }
      return false;
    }
    case Scenario::lists_enter:
      if (frame == 0) {
        press(provider::Button::dpad_down);
        return true;
      }
      return false;
    case Scenario::lists_return:
      if (frame == 0 || frame == tour_stay) {
        press(frame == 0 ? provider::Button::dpad_up
                         : provider::Button::dpad_down);
        return true;
      }
      return false;
    case Scenario::materials_leave:
      if (frame == still_frames) {
        press(provider::Button::dpad_down);
        return true;
      }
      if (frame == still_frames + (leave_tail / 2)) {
        press(provider::Button::dpad_up);
        return true;
      }
      return false;
    case Scenario::lists_idle:
      static_cast<void>(application.root().tree().find(
          composition::ElementKey{LaunchScreen::root_view}));
      return false;
    case Scenario::idle:
    case Scenario::launch:
    case Scenario::launch_path:
    case Scenario::launch_materials:
      return false;
    case Scenario::fling:
      return stroke(frame);
    case Scenario::sidebar_scroll:
      return scroll(frame, sidebar_column);
    case Scenario::detail_scroll:
      return scroll(frame, detail_column);
    case Scenario::sidebar_fast:
      return sweep(frame);
    case Scenario::sidebar_steps:
      return step(frame);
    case Scenario::sidebar_settle:
      return settle(frame);
    case Scenario::split_steps:
      return split(frame);
    case Scenario::glass_page:
    case Scenario::materials_page:
      return frame >= still_frames &&
             scroll(frame - still_frames, detail_column);
    case Scenario::covered_page:
      if (frame == 0) {
        press(provider::Button::r);
        return true;
      }
      return false;
    case Scenario::gallery:
      if (frame > 0 && frame % page_frames == 0) {
        press(provider::Button::r);
        return true;
      }
      return false;
    case Scenario::theme:
      if (frame % theme_frames == 0) {
        press(provider::Button::y);
        return true;
      }
      return false;
    case Scenario::navigation:
      if (remote_->path.has_value() && frame % (cycle_frames / 2) == 0) {
        remote_->path->set(frame % cycle_frames == 0 ? std::vector<int>{1}
                                                     : std::vector<int>{});
        return true;
      }
      return false;
    case Scenario::sheet:
    case Scenario::sheet_glass:
      if (remote_->sheet.has_value() && frame % (cycle_frames / 2) == 0) {
        remote_->sheet->set(frame % cycle_frames == 0);
        return true;
      }
      return false;
  }
  return false;
}

auto Benchmark::run(Scenario scenario)
    -> std::expected<Summary, runtime::ApplicationError> {
  provider::ProviderFactory& providers = providers_;
  runtime::ApplicationOptions options{
      .executors = std::nullopt,
      .fonts = GalleryFonts::load(providers.assets()),
      .atlas = {},
      .interaction = {},
      .appearance = std::nullopt,
      .launch_colour = std::nullopt};
  options.render_thread = render_thread_;
  if (scenario == Scenario::launch || scenario == Scenario::launch_path ||
      scenario == Scenario::launch_materials) {
    options.launch_colour =
        theme::Colour::hex(photo() != nullptr ? GalleryPhoto::average
                                              : LaunchScreen::wallpaper[5]);
  }
  if (scenario == Scenario::sidebar_scroll ||
      scenario == Scenario::detail_scroll ||
      scenario == Scenario::sidebar_fast ||
      scenario == Scenario::sidebar_steps ||
      scenario == Scenario::sidebar_settle ||
      scenario == Scenario::split_steps || scenario == Scenario::sheet_glass ||
      scenario == Scenario::list_paging ||
      scenario == Scenario::materials_leave || scenario == Scenario::tour ||
      scenario == Scenario::lists_idle || scenario == Scenario::lists_enter ||
      scenario == Scenario::lists_return ||
      scenario == Scenario::token_slider_drag || paged(scenario) ||
      interactive(scenario)) {
    options.appearance = glass_appearance();
  }
  auto created =
      runtime::Application::create(providers, content(scenario), options);
  if (!created.has_value()) {
    return std::unexpected(created.error());
  }
  runtime::Application application = std::move(*created);
#ifdef ARISA_INSPECTOR
  std::optional<inspector::Inspector> inspected;
  if (scenario == Scenario::token_slider_drag) {
    inspected.emplace(inspector::Inspector::attach(application));
    inspected->open();
    inspected->show(inspector::InspectorPanel::tokens);
    if (const auto token = appearance::ThemeToken::find(dragged_token)) {
      inspected->model().select_token(token->get().index());
    }
  }
#endif
  const Plan planned = plan(scenario);
  const std::uint32_t rehearsal = rehearsed(scenario) ? planned.frames : 0;
  const std::uint32_t lead = planned.warmup + rehearsal;
  const std::size_t first = samples_.size();
  std::vector<provider::ThreadCpu> threads_before;
  std::chrono::steady_clock::time_point began{};
  std::uint64_t wakes_before = 0;
  std::uint64_t blurs = 0;
  std::uint64_t blur_hits = 0;
  std::uint64_t builds_before = 0;
  std::uint64_t searches_before = 0;
  for (std::uint32_t index = 0; index < lead + planned.frames; ++index) {
    const bool measured = index >= lead;
    if (index == lead) {
      builds_before = split_->detail_builds;
      searches_before = application.root().tree().searches();
      threads_before = provider::ThreadCpuMeter::sample();
      began = std::chrono::steady_clock::now();
      wakes_before = application.animation_wakes();
    }
    const bool scripted = index >= planned.warmup;
    const std::uint32_t frame =
        scripted ? (index - planned.warmup) % planned.frames : 0;
    const bool stimulus = scripted && stimulate(scenario, frame, application);
    const bool released = !stimulus && pressed_ == 1;
    pressed_ = pressed_ > 0 ? pressed_ - 1 : 0;
    if (!advance_(std::exchange(skip_, std::chrono::nanoseconds{0}))) {
      break;
    }
    const std::optional<std::uint64_t> before =
        provider::AllocationMeter::count();
    const auto started = std::chrono::steady_clock::now();
    if (auto stepped = application.frame(); !stepped.has_value()) {
      return std::unexpected(stepped.error());
    }
    const auto wall = std::chrono::steady_clock::now() - started;

    const std::optional<std::uint64_t> after =
        provider::AllocationMeter::count();
    if (measured) {
      const runtime::FrameTimings& timings = application.last_frame();
      blurs += timings.blurs;
      blur_hits += timings.blur_hits;
      std::optional<std::chrono::nanoseconds> gpu;
      if (timings.gpu_measured) {
        gpu = timings.interface_gpu.value_or(std::chrono::nanoseconds{0}) +
              timings.effects_gpu.value_or(std::chrono::nanoseconds{0});
      }
      samples_.push_back(Sample{
          .scenario = scenario,
          .frame = frame,
          .kind = timings.kind,
          .presented = timings.presented,
          .laid_out = timings.laid_out,
          .stimulus = stimulus,
          .released = released,
          .evaluations = timings.evaluations,
          .cpu = std::max(
              std::chrono::duration_cast<std::chrono::nanoseconds>(wall) -
                  timings.blocked(),
              std::chrono::nanoseconds{0}),
          .wall = std::chrono::duration_cast<std::chrono::nanoseconds>(wall),
          .gpu = gpu,
          .allocations = before.has_value() && after.has_value()
                             ? std::optional{*after - *before}
                             : std::nullopt,
          .uploaded_bytes = timings.uploaded_bytes,
          .uploaded_vertices = timings.uploaded_vertices,
          .damaged_pixels = timings.damaged_pixels,
          .blurs = timings.blurs,
          .recorded = timings.recorded,
          .spliced = timings.spliced,
          .nodes = application.root().tree().size(),
          .reconcile = timings.reconcile,
          .layout = timings.layout,
          .paint = timings.paint,
          .row = scenario == Scenario::list_paging &&
                         (frame + 1 == paging_entry ||
                          frame + 1 == paging_entry + paging_hold ||
                          frame + 1 == planned.frames)
                     ? deepest(application)
                     : 0U,
          .animating = timings.animating,
          .paced = timings.paced,
          .computations = timings.layout_computations,
          .input = timings.input,
          .collections = timings.focus_collections,
          .refreshes = timings.focus_refreshes,
          .uncovered = timings.focus_uncovered,
          .timings = timings});
    }
    if (real_time_ && application.waiting()) {
      application.wait();
    }
  }
  Summary summary = summarise(scenario, first);
  summary.elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
      std::chrono::steady_clock::now() - began);
  summary.threads = provider::ThreadCpuMeter::since(
      threads_before, provider::ThreadCpuMeter::sample());
  summary.animation_wakes = application.animation_wakes() - wakes_before;
  summary.blurs = blurs;
  summary.blur_hits = blur_hits;
  summary.detail_builds = split_->detail_builds - builds_before;
  summary.searches = application.root().tree().searches() - searches_before;
  if (scenario == Scenario::materials_page) {
    const float wide = application.root().tree().root().frame().width;
    summary.scale = wide > 0.0F
                        ? static_cast<float>(
                              providers_.get().surface().size().width.value()) /
                              wide
                        : 1.0F;
    summary.region = region(application, MaterialsPage::wallpaper_row)
                         .value_or(composition::Rect{});
    summary.region_pixels = pixels(summary, 0.0F);
  }
  if (interactive(scenario)) {
    const std::string_view held =
        scenario == Scenario::clock_tick
            ? std::string_view{"arisa.scaffold.status"}
            : InteractionScreen::control;
    composition::Tree& tree = application.root().tree();
    if (const auto found = tree.find(composition::ElementKey{held})) {
      if (const auto node = tree.node(*found)) {
        summary.control_nodes = subtree(node->get());
        summary.control_depth = depth(tree, node->get());
      }
    }
    const float wide = tree.root().frame().width;
    summary.scale = wide > 0.0F
                        ? static_cast<float>(
                              providers_.get().surface().size().width.value()) /
                              wide
                        : 1.0F;
    summary.region = area(application, scenario);
    summary.region_pixels = pixels(summary, 0.0F);
  }
  return summary;
}

auto Benchmark::summarise(Scenario scenario, std::size_t first) const
    -> Summary {
  Summary summary{.scenario = scenario};
  const std::span<const Sample> taken = std::span{samples_}.subspan(first);
  std::vector<std::chrono::nanoseconds> cpu;
  std::chrono::nanoseconds gpu_total{0};
  std::uint32_t gpu_frames = 0;
  std::chrono::nanoseconds wall_total{0};
  std::uint64_t uploaded_total = 0;
  std::uint64_t vertex_total = 0;
  std::uint64_t repaint_vertex_total = 0;
  std::uint64_t damaged_total = 0;
  std::vector<std::uint64_t> damaged;
  std::chrono::nanoseconds still_total{0};
  std::chrono::nanoseconds drift_total{0};
  std::chrono::nanoseconds layout_total{0};
  std::chrono::nanoseconds input_total{0};
  std::uint32_t laid_out = 0;
  if (!taken.empty()) {
    summary.nodes_min = taken.front().nodes;
    summary.nodes_max = taken.front().nodes;
  }
  const Sample* before = nullptr;
  for (const Sample& sample : taken) {
    summary.nodes_min = std::min(summary.nodes_min, sample.nodes);
    summary.nodes_max = std::max(summary.nodes_max, sample.nodes);
    summary.damaged_pixels_max =
        std::max(summary.damaged_pixels_max,
                 sample.presented ? sample.damaged_pixels : 0);
    summary.recorded_max = std::max(summary.recorded_max, sample.recorded);
    const bool settled = interactive(scenario)
                             ? steady(sample)
                             : before != nullptr && steady(*before) &&
                                   steady(sample) &&
                                   before->nodes == sample.nodes;
    if (settled) {
      summary.steady_recorded_max =
          std::max(summary.steady_recorded_max, sample.recorded);
    }
    before = &sample;
    if (sample.frame >= worst_from(scenario) &&
        sample.wall > summary.worst_wall) {
      summary.worst_wall = sample.wall;
      summary.worst_frame = sample.frame;
      summary.worst_allocations = sample.allocations;
      summary.worst = sample.timings;
    }
    summary.computations += sample.computations;
    summary.collections += sample.collections;
    if (sample.evaluations == 0 && !sample.laid_out && !sample.stimulus &&
        !sample.released) {
      summary.quiet_collections += sample.collections - sample.uncovered;
    }
    if (sample.laid_out) {
      summary.entry_computations =
          std::max(summary.entry_computations, sample.computations);
    }
    if (sample.presented) {
      input_total += sample.input;
    }
    summary.deepest = std::max(summary.deepest, sample.row);
    if (sample.laid_out) {
      ++laid_out;
      layout_total += sample.layout;
    }
    if (quiet(scenario, sample.frame) && sample.presented) {
      ++summary.quiet_presented;
    }
    if (resumed(scenario, sample.frame) && sample.presented) {
      ++summary.resumed_presented;
    }
    if (still(scenario, sample.frame) && sample.presented) {
      summary.still_damaged_max =
          std::max(summary.still_damaged_max, sample.damaged_pixels);
    }
    if (still(scenario, sample.frame)) {
      ++summary.still_frames;
      summary.still_presented += sample.presented ? 1U : 0U;
      summary.still_blurs += sample.blurs;
      still_total += sample.cpu;
      drift_total += sample.presented ? sample.cpu : std::chrono::nanoseconds{};
    }
    ++summary.frames;
    wall_total += sample.wall;
    summary.evaluations += sample.evaluations;
    summary.steps += sample.stimulus ? 1U : 0U;
    if (sample.evaluations > 0) {
      ++summary.building;
      summary.building_allocations += sample.allocations.value_or(0);
    }
    if (sample.allocations.has_value()) {
      summary.allocations =
          summary.allocations.value_or(0) + *sample.allocations;
    }
    if (!sample.presented) {
      continue;
    }
    ++summary.presented;
    uploaded_total += sample.uploaded_bytes;
    vertex_total += sample.uploaded_vertices * sizeof(provider::ShapeVertex);
    damaged_total += sample.damaged_pixels;
    damaged.push_back(sample.damaged_pixels);
    cpu.push_back(sample.cpu);
    if (sample.gpu.has_value()) {
      gpu_total += *sample.gpu;
      ++gpu_frames;
      summary.gpu_max = std::max(
          summary.gpu_max.value_or(std::chrono::nanoseconds{0}), *sample.gpu);
    }
    if (!sample.laid_out && sample.evaluations == 0 && !sample.stimulus &&
        !sample.released) {
      ++summary.repaints;
      repaint_vertex_total +=
          sample.uploaded_vertices * sizeof(provider::ShapeVertex);
      if (sample.allocations.has_value()) {
        summary.repaint_allocations =
            summary.repaint_allocations.value_or(0) + *sample.allocations;
      }
    }
  }
  if (!cpu.empty()) {
    std::chrono::nanoseconds total{0};
    for (const std::chrono::nanoseconds each : cpu) {
      total += each;
    }
    summary.cpu_mean = total / static_cast<std::int64_t>(cpu.size());
    std::ranges::sort(cpu);
    summary.cpu_p95 =
        cpu[(cpu.size() * 95) / 100 < cpu.size() ? (cpu.size() * 95) / 100
                                                 : cpu.size() - 1];
    summary.cpu_max = cpu.back();
  }
  summary.laid_out = laid_out;
  if (summary.presented > 0) {
    summary.input_mean =
        input_total / static_cast<std::int64_t>(summary.presented);
  }
  if (laid_out > 0) {
    summary.layout_mean = layout_total / static_cast<std::int64_t>(laid_out);
  }
  if (summary.still_frames > 0) {
    summary.still_cpu_mean =
        still_total / static_cast<std::int64_t>(summary.still_frames);
  }
  if (summary.still_presented > 0) {
    summary.drift_cpu_mean =
        drift_total / static_cast<std::int64_t>(summary.still_presented);
  }
  if (summary.frames > 0) {
    summary.wall_mean = wall_total / static_cast<std::int64_t>(summary.frames);
  }
  if (summary.presented > 0) {
    summary.uploaded_bytes_mean = uploaded_total / summary.presented;
    summary.vertex_bytes_mean = vertex_total / summary.presented;
    summary.damaged_pixels_mean = damaged_total / summary.presented;
    std::ranges::nth_element(damaged, damaged.begin() + (damaged.size() / 2));
    summary.damaged_pixels_median = damaged[damaged.size() / 2];
  }
  const provider::SurfaceSize surface = providers_.get().surface().size();
  summary.surface_pixels = static_cast<std::uint64_t>(surface.width.value()) *
                           surface.height.value();
  if (summary.repaints > 0) {
    summary.repaint_vertex_bytes_mean = repaint_vertex_total / summary.repaints;
  }
  if (gpu_frames > 0) {
    summary.gpu_mean = gpu_total / static_cast<std::int64_t>(gpu_frames);
  }
  return summary;
}

auto Benchmark::csv_header() -> std::string_view {
  return "scenario,frame,kind,presented,laid_out,evaluations,stimulus,cpu_us,"
         "wall_us,gpu_us,allocations,uploaded_bytes,damaged_pixels,"
         "uploaded_vertex_bytes,blurs,recorded,spliced,reconcile_us,layout_us,"
         "paint_us,row,animating,paced,computations,input_us,collections,"
         "refreshes,walk_us,upload_us,emit_us,present_us,present_cpu_us,"
         "blocked_us,record_us,wait_us,threaded";
}

auto Benchmark::csv(const Sample& sample) -> std::string {
  const auto micro = [](std::chrono::nanoseconds value) {
    return std::chrono::duration<double, std::micro>(value).count();
  };
  return std::format(
      "{},{},{},{},{},{},{},{:.1f},{:.1f},{},{},{},{},{},{},{},{},{:.1f},{:.1f}"
      ","
      "{:.1f},{},{},{},{},{:.1f},{},{},{:.1f},{:.1f},{:.1f},{:.1f},{:.1f},"
      "{:.1f},{:.1f},{:.1f},{}",
      name(sample.scenario), sample.frame, static_cast<int>(sample.kind),
      sample.presented ? 1 : 0, sample.laid_out ? 1 : 0, sample.evaluations,
      sample.stimulus ? 1 : 0, micro(sample.cpu), micro(sample.wall),
      sample.gpu.has_value() ? std::format("{:.1f}", micro(*sample.gpu))
                             : std::string{},
      sample.allocations.has_value() ? std::format("{}", *sample.allocations)
                                     : std::string{},
      sample.uploaded_bytes, sample.damaged_pixels,
      sample.uploaded_vertices * sizeof(provider::ShapeVertex), sample.blurs,
      sample.recorded, sample.spliced, micro(sample.reconcile),
      micro(sample.layout), micro(sample.paint), sample.row,
      sample.animating ? 1 : 0, sample.paced ? 1 : 0, sample.computations,
      micro(sample.input), sample.collections, sample.refreshes,
      micro(sample.timings.walk), micro(sample.timings.upload),
      micro(sample.timings.emit), micro(sample.timings.present),
      micro(sample.timings.present_cpu()), micro(sample.timings.blocked()),
      micro(sample.timings.record), micro(sample.timings.wait),
      sample.timings.threaded ? 1 : 0);
}

auto Benchmark::describe(const Summary& summary) -> std::string {
  const auto micro = [](std::chrono::nanoseconds value) {
    return std::chrono::duration<double, std::micro>(value).count();
  };
  const auto optional_micro =
      [&micro](const std::optional<std::chrono::nanoseconds>& value) {
        return value.has_value() ? std::format("{:.0f}", micro(*value))
                                 : std::string{"-"};
      };
  const auto optional_count = [](const std::optional<std::uint64_t>& value) {
    return value.has_value() ? std::format("{}", *value) : std::string{"-"};
  };
  return std::format(
             "bench {}: {} frames, {} presented, wall mean {:.0f} us, cpu mean "
             "{:.0f} "
             "p95 {:.0f} max {:.0f} us, gpu mean {} max {} us, allocations {} "
             "({} in "
             "{} repaints), uploaded mean {} B (vertices {} B, {} B per "
             "repaint), "
             "damaged mean {} px",
             name(summary.scenario), summary.frames, summary.presented,
             micro(summary.wall_mean), micro(summary.cpu_mean),
             micro(summary.cpu_p95), micro(summary.cpu_max),
             optional_micro(summary.gpu_mean), optional_micro(summary.gpu_max),
             optional_count(summary.allocations),
             optional_count(summary.repaint_allocations), summary.repaints,
             summary.uploaded_bytes_mean, summary.vertex_bytes_mean,
             summary.repaint_vertex_bytes_mean, summary.damaged_pixels_mean) +
         std::format(", blurs {} cached {}, animation wakes {}", summary.blurs,
                     summary.blur_hits, summary.animation_wakes) +
         (summary.scenario == Scenario::sheet_glass
              ? std::format(
                    ", recorded per fade frame max {} of {} sheet nodes",
                    summary.steady_recorded_max,
                    summary.nodes_max - summary.nodes_min)
              : std::string{}) +
         (summary.scenario == Scenario::split_steps
              ? std::format(", detail builds {}, evaluations per step {:.1f}",
                            summary.detail_builds,
                            summary.steps > 0
                                ? static_cast<double>(summary.evaluations) /
                                      static_cast<double>(summary.steps)
                                : 0.0)
              : std::string{}) +
         (interactive(summary.scenario)
              ? std::format(
                    ", control nodes {} at depth {}, recorded per steady frame "
                    "max {} (any "
                    "frame {}), evaluations per input {:.1f}, damage max {} of "
                    "region {} px, blurs per frame {:.2f}",
                    summary.control_nodes, summary.control_depth,
                    summary.steady_recorded_max, summary.recorded_max,
                    static_cast<double>(summary.evaluations) /
                        std::max(1.0, static_cast<double>(summary.steps)),
                    summary.damaged_pixels_max, summary.region_pixels,
                    summary.presented > 0
                        ? static_cast<double>(summary.blurs) /
                              static_cast<double>(summary.presented)
                        : 0.0)
              : std::string{}) +
         (summary.building > 0
              ? std::format(", allocations per building frame {}",
                            summary.building_allocations / summary.building)
              : std::string{}) +
         (paged(summary.scenario)
              ? std::format(
                    ", still {} frames {} presented cpu mean {:.0f} us drift "
                    "cpu mean {:.0f} us blurs {} damage max {} px (row {} px)",
                    summary.still_frames, summary.still_presented,
                    micro(summary.still_cpu_mean),
                    micro(summary.drift_cpu_mean), summary.still_blurs,
                    summary.still_damaged_max, summary.region_pixels)
              : std::string{}) +
         (summary.scenario == Scenario::list_paging ||
                  summary.scenario == Scenario::materials_leave ||
                  summary.scenario == Scenario::lists_enter ||
                  summary.scenario == Scenario::lists_return ||
                  summary.scenario == Scenario::lists_idle
              ? std::format(
                    ", layout computations {} over {} laid-out steps ({:.0f} "
                    "per step), layout mean {:.0f} us, quiet presented {}, "
                    "resumed presented {}",
                    summary.computations, summary.laid_out,
                    static_cast<double>(summary.computations) /
                        std::max(1.0, static_cast<double>(summary.laid_out)),
                    micro(summary.layout_mean), summary.quiet_presented,
                    summary.resumed_presented) +
                    std::format(
                        ", input mean {:.0f} us, focus collects {} ({} on "
                        "quiet frames), entry computations {}, linear "
                        "searches {}",
                        micro(summary.input_mean), summary.collections,
                        summary.quiet_collections, summary.entry_computations,
                        summary.searches)
              : std::string{}) +
         threads(summary) +
         std::format(", worst frame {} wall {:.0f} us: {}", summary.worst_frame,
                     micro(summary.worst_wall),
                     runtime::FrameBreakdown::describe(
                         summary.worst, summary.worst_allocations));
}

auto Benchmark::threads(const Summary& summary) -> std::string {
  const double elapsed = std::chrono::duration<double>(summary.elapsed).count();
  if (elapsed <= 0.0 || summary.threads.empty()) {
    return {};
  }
  std::string line{", threads"};
  for (const provider::ThreadCpu& thread : summary.threads) {
    line += std::format(
        " {} {:.2f}%", thread.name,
        100.0 * std::chrono::duration<double>(thread.cpu).count() / elapsed);
  }
  return line;
}

auto Benchmark::thread_share(const Summary& summary, std::string_view prefix)
    -> double {
  const double elapsed = std::chrono::duration<double>(summary.elapsed).count();
  if (elapsed <= 0.0) {
    return 0.0;
  }
  std::chrono::nanoseconds spent{0};
  for (const provider::ThreadCpu& thread : summary.threads) {
    if (thread.name.starts_with(prefix)) {
      spent += thread.cpu;
    }
  }
  return std::chrono::duration<double>(spent).count() / elapsed;
}

auto Benchmark::violations(const Summary& summary, const Budget& budget)
    -> std::vector<std::string> {
  std::vector<std::string> found;
  const std::string_view shown = name(summary.scenario);
  const std::uint64_t tolerated = summary.scenario == Scenario::gallery
                                      ? summary.repaints / app_task_share
                                      : 0;
  if (rehearsed(summary.scenario) &&
      summary.scenario != Scenario::token_slider_drag &&
      summary.repaint_allocations.value_or(0) > tolerated) {
    found.push_back(std::format("{}: {} heap allocations in {} repaints", shown,
                                *summary.repaint_allocations,
                                summary.repaints));
  }
  switch (summary.scenario) {
    case Scenario::token_slider_drag:
      if (summary.cpu_mean > budget.token_drag_cpu) {
        found.push_back(
            std::format("token_slider_drag: cpu mean {} us over {} us",
                        std::chrono::duration_cast<std::chrono::microseconds>(
                            summary.cpu_mean)
                            .count(),
                        std::chrono::duration_cast<std::chrono::microseconds>(
                            budget.token_drag_cpu)
                            .count()));
      }
      break;
    case Scenario::idle:
      if (summary.repaints > 0) {
        found.push_back(std::format(
            "idle: {} frames repainted without a change", summary.repaints));
      }
      if (summary.wall_mean > budget.idle_cpu) {
        found.push_back(
            std::format("idle: wall mean {} us over {} us",
                        std::chrono::duration_cast<std::chrono::microseconds>(
                            summary.wall_mean)
                            .count(),
                        std::chrono::duration_cast<std::chrono::microseconds>(
                            budget.idle_cpu)
                            .count()));
      }
      break;
    case Scenario::fling:
      if (summary.cpu_mean > budget.scroll_cpu) {
        found.push_back(
            std::format("fling: cpu mean {} us over {} us",
                        std::chrono::duration_cast<std::chrono::microseconds>(
                            summary.cpu_mean)
                            .count(),
                        std::chrono::duration_cast<std::chrono::microseconds>(
                            budget.scroll_cpu)
                            .count()));
      }
      if (budget.scroll_gpu.has_value() && summary.gpu_mean.has_value() &&
          *summary.gpu_mean > *budget.scroll_gpu) {
        found.push_back("fling: gpu mean over budget");
      }
      if (summary.repaint_vertex_bytes_mean >
          budget.fling_repaint_vertex_bytes) {
        found.push_back(
            std::format("fling: {} vertex bytes uploaded per repaint over {} B",
                        summary.repaint_vertex_bytes_mean,
                        budget.fling_repaint_vertex_bytes));
      }
      if (summary.allocations.has_value() && summary.building > 0 &&
          summary.building_allocations / summary.building >
              budget.fling_building_allocations) {
        found.push_back(std::format(
            "fling: {} heap allocations per row-building frame over {}",
            summary.building_allocations / summary.building,
            budget.fling_building_allocations));
      }
      break;
    case Scenario::launch:
    case Scenario::launch_path:
      if (summary.cpu_mean > budget.launch_cpu) {
        found.push_back(
            std::format("{}: cpu mean {} us over {} us", shown,
                        std::chrono::duration_cast<std::chrono::microseconds>(
                            summary.cpu_mean)
                            .count(),
                        std::chrono::duration_cast<std::chrono::microseconds>(
                            budget.launch_cpu)
                            .count()));
      }
      if (summary.uploaded_bytes_mean > budget.launch_uploaded) {
        found.push_back(std::format("{}: uploaded mean {} B over {} B", shown,
                                    summary.uploaded_bytes_mean,
                                    budget.launch_uploaded));
      }
      break;
    case Scenario::sidebar_scroll:
    case Scenario::detail_scroll: {
      if (summary.cpu_mean > budget.scroll_cpu) {
        found.push_back(
            std::format("{}: cpu mean {} us over {} us", shown,
                        std::chrono::duration_cast<std::chrono::microseconds>(
                            summary.cpu_mean)
                            .count(),
                        std::chrono::duration_cast<std::chrono::microseconds>(
                            budget.scroll_cpu)
                            .count()));
      }
      const double seconds =
          std::chrono::duration<double>(summary.elapsed).count();
      if (seconds > 0.0 &&
          static_cast<double>(summary.animation_wakes) / seconds >
              budget.animation_wakes_per_second) {
        found.push_back(
            std::format("{}: animation worker woke {} times in {:.1f} s", shown,
                        summary.animation_wakes, seconds));
      }
      if (summary.scenario == Scenario::sidebar_scroll &&
          summary.presented > 0 &&
          static_cast<double>(summary.blurs) /
                  static_cast<double>(summary.presented) >
              budget.blurs_per_frame) {
        found.push_back(std::format("{}: {} backdrop blurs in {} frames", shown,
                                    summary.blurs, summary.presented));
      }
      break;
    }
    case Scenario::sidebar_fast: {
      const auto micro = [](std::chrono::nanoseconds value) {
        return std::chrono::duration_cast<std::chrono::microseconds>(value)
            .count();
      };
      if (summary.cpu_mean > budget.sweep_cpu) {
        found.push_back(std::format("{}: cpu mean {} us over {} us", shown,
                                    micro(summary.cpu_mean),
                                    micro(budget.sweep_cpu)));
      }
      if (summary.cpu_max > budget.sweep_frame) {
        found.push_back(std::format("{}: cpu max {} us over {} us", shown,
                                    micro(summary.cpu_max),
                                    micro(budget.sweep_frame)));
      }
      break;
    }
    case Scenario::split_steps: {
      const auto micro = [](std::chrono::nanoseconds value) {
        return std::chrono::duration_cast<std::chrono::microseconds>(value)
            .count();
      };
      if (summary.cpu_mean > budget.steps_cpu) {
        found.push_back(std::format("{}: cpu mean {} us over {} us", shown,
                                    micro(summary.cpu_mean),
                                    micro(budget.steps_cpu)));
      }
      if (summary.frames > 0 && static_cast<double>(summary.blurs) /
                                        static_cast<double>(summary.frames) >
                                    budget.blurs_per_frame) {
        found.push_back(std::format("{}: {} backdrop blurs in {} frames", shown,
                                    summary.blurs, summary.frames));
      }
      if (summary.steps > 0 &&
          summary.evaluations > budget.split_step_evaluations * summary.steps) {
        found.push_back(std::format(
            "{}: {} evaluations in {} steps, over {} per step", shown,
            summary.evaluations, summary.steps, budget.split_step_evaluations));
      }
      if (summary.detail_builds > budget.split_builds) {
        found.push_back(std::format("{}: {} detail builds over {}", shown,
                                    summary.detail_builds,
                                    budget.split_builds));
      }
      break;
    }
    case Scenario::sidebar_steps: {
      const auto micro = [](std::chrono::nanoseconds value) {
        return std::chrono::duration_cast<std::chrono::microseconds>(value)
            .count();
      };
      if (summary.cpu_mean > budget.steps_cpu) {
        found.push_back(std::format("{}: cpu mean {} us over {} us", shown,
                                    micro(summary.cpu_mean),
                                    micro(budget.steps_cpu)));
      }
      const auto column = static_cast<std::uint64_t>(
          static_cast<double>(summary.surface_pixels) * budget.sidebar_share);
      if (summary.damaged_pixels_median > column) {
        found.push_back(std::format(
            "{}: {} damaged pixels in the median frame, over the {} of the "
            "sidebar column",
            shown, summary.damaged_pixels_median, column));
      }
      if (summary.presented > 0 &&
          static_cast<double>(summary.blurs) /
                  static_cast<double>(summary.presented) >
              budget.blurs_per_frame) {
        found.push_back(std::format("{}: {} backdrop blurs in {} frames", shown,
                                    summary.blurs, summary.presented));
      }
      break;
    }
    case Scenario::glass_page:
    case Scenario::materials_page:
    case Scenario::covered_page: {
      const auto micro = [](std::chrono::nanoseconds value) {
        return std::chrono::duration_cast<std::chrono::microseconds>(value)
            .count();
      };
      const double drift_steps = static_cast<double>(summary.still_frames) *
                                 LaunchScreen::drift_frame_rate / nominal_rate;
      if (summary.scenario == Scenario::materials_page) {
        if (static_cast<double>(summary.still_presented) > drift_steps + 1.0) {
          found.push_back(std::format(
              "{}: {} still frames presented over the {:.0f} drift steps",
              shown, summary.still_presented, drift_steps));
        }
        const auto sources = static_cast<double>(theme::Material::all().size());
        if (drift_steps > 0.0 &&
            static_cast<double>(summary.still_blurs) / drift_steps > sources) {
          found.push_back(std::format(
              "{}: {} backdrop blurs over {:.0f} drift steps, more than {:.0f} "
              "per step",
              shown, summary.still_blurs, drift_steps, sources));
        }
        const std::uint64_t allowed = pixels(summary, budget.materials_reach);
        if (summary.still_damaged_max > allowed) {
          found.push_back(std::format(
              "{}: {} damaged pixels in a drift frame, over the wallpaper "
              "row's {} grown by the readers' reach",
              shown, summary.still_damaged_max, allowed));
        }
        if (summary.drift_cpu_mean > budget.drift_cpu) {
          found.push_back(std::format("{}: drift cpu mean {} us over {} us",
                                      shown, micro(summary.drift_cpu_mean),
                                      micro(budget.drift_cpu)));
        }
      } else if (summary.still_blurs > 0) {
        found.push_back(std::format("{}: {} backdrop blurs in {} still frames",
                                    shown, summary.still_blurs,
                                    summary.still_frames));
      }
      if (summary.still_cpu_mean > budget.still_cpu) {
        found.push_back(std::format("{}: still cpu mean {} us over {} us",
                                    shown, micro(summary.still_cpu_mean),
                                    micro(budget.still_cpu)));
      }
      if (summary.scenario == Scenario::covered_page &&
          summary.still_presented > 0) {
        found.push_back(std::format(
            "{}: {} of {} still frames presented behind a covered page", shown,
            summary.still_presented, summary.still_frames));
      }
      break;
    }
    case Scenario::list_paging: {
      const auto micro = [](std::chrono::nanoseconds value) {
        return std::chrono::duration_cast<std::chrono::microseconds>(value)
            .count();
      };
      if (summary.cpu_mean > budget.paging_cpu) {
        found.push_back(std::format("{}: cpu mean {} us over {} us", shown,
                                    micro(summary.cpu_mean),
                                    micro(budget.paging_cpu)));
      }
      if (summary.layout_mean > budget.paging_layout) {
        found.push_back(std::format(
            "{}: layout mean {} us per laid-out frame over {} us", shown,
            micro(summary.layout_mean), micro(budget.paging_layout)));
      }
      const double passes =
          std::max(1.0, static_cast<double>(summary.laid_out));
      if (static_cast<double>(summary.computations) / passes >
          budget.paging_computations) {
        found.push_back(std::format(
            "{}: {} layout computations in {} laid-out steps, over {:.0f} per "
            "step",
            shown, summary.computations, summary.laid_out,
            budget.paging_computations));
      }
      if (summary.building > 0 &&
          summary.building_allocations / summary.building >
              budget.paging_building_allocations) {
        found.push_back(std::format(
            "{}: {} heap allocations per row-building frame over {}", shown,
            summary.building_allocations / summary.building,
            budget.paging_building_allocations));
      }
      if (summary.quiet_presented > 0) {
        found.push_back(std::format("{}: {} frames presented after paging",
                                    shown, summary.quiet_presented));
      }
      if (summary.input_mean > budget.paging_input) {
        found.push_back(std::format("{}: input mean {} us over {} us", shown,
                                    micro(summary.input_mean),
                                    micro(budget.paging_input)));
      }
      if (summary.quiet_collections > 0) {
        found.push_back(std::format(
            "{}: {} focus collects on frames without a structural change",
            shown, summary.quiet_collections));
      }
      if (summary.deepest < paged_rows) {
        found.push_back(std::format(
            "{}: paging reached row {}, focus never paged the long list", shown,
            summary.deepest));
      }
      break;
    }
    case Scenario::lists_enter: {
      const auto micro = [](std::chrono::nanoseconds value) {
        return std::chrono::duration_cast<std::chrono::microseconds>(value)
            .count();
      };
      if (summary.entry_computations > budget.entry_computations) {
        found.push_back(std::format(
            "{}: {} layout computations entering the page, over {}", shown,
            summary.entry_computations, budget.entry_computations));
      }
      if (summary.cpu_max > budget.entry_cpu) {
        found.push_back(std::format("{}: entry frame cpu {} us over {} us",
                                    shown, micro(summary.cpu_max),
                                    micro(budget.entry_cpu)));
      }
      break;
    }
    case Scenario::lists_idle: {
      const auto micro = [](std::chrono::nanoseconds value) {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(value)
            .count();
      };
      if (summary.searches > 0) {
        found.push_back(
            std::format("{}: {} linear tree searches in {} idle ticks", shown,
                        summary.searches, summary.frames));
      }
      if (summary.wall_mean > budget.idle_tick) {
        found.push_back(std::format("{}: idle tick {} ns over {} ns", shown,
                                    micro(summary.wall_mean),
                                    micro(budget.idle_tick)));
      }
      break;
    }
    case Scenario::materials_leave: {
      if (summary.quiet_presented > 0) {
        found.push_back(
            std::format("{}: {} frames presented while the page was hidden",
                        shown, summary.quiet_presented));
      }
      const double drift_steps = static_cast<double>(leave_tail / 2) *
                                 LaunchScreen::drift_frame_rate / nominal_rate;
      if (static_cast<double>(summary.resumed_presented) < drift_steps - 1.0) {
        found.push_back(std::format(
            "{}: {} frames presented after returning, under the {:.0f} drift "
            "steps",
            shown, summary.resumed_presented, drift_steps));
      }
      break;
    }
    case Scenario::sheet_glass: {
      const auto micro = [](std::chrono::nanoseconds value) {
        return std::chrono::duration_cast<std::chrono::microseconds>(value)
            .count();
      };
      if (summary.cpu_mean > budget.sheet_cpu) {
        found.push_back(std::format("{}: cpu mean {} us over {} us", shown,
                                    micro(summary.cpu_mean),
                                    micro(budget.sheet_cpu)));
      }
      const std::size_t sheet = summary.nodes_max - summary.nodes_min;
      if (summary.steady_recorded_max > sheet) {
        found.push_back(std::format(
            "{}: {} nodes recorded in a fade frame, over the sheet's {}", shown,
            summary.steady_recorded_max, sheet));
      }
      break;
    }
    default:
      break;
  }
  if (interactive(summary.scenario)) {
    std::ranges::move(interaction_violations(summary, budget),
                      std::back_inserter(found));
  }
  return found;
}

auto Benchmark::interaction_violations(const Summary& summary,
                                       const Budget& budget)
    -> std::vector<std::string> {
  std::vector<std::string> found;
  const auto chosen = std::ranges::find(budget.interactions, summary.scenario,
                                        &Interaction::scenario);
  if (chosen == budget.interactions.end()) {
    return found;
  }
  const std::string_view shown = name(summary.scenario);
  const auto micro = [](std::chrono::nanoseconds value) {
    return std::chrono::duration_cast<std::chrono::microseconds>(value).count();
  };
  if (summary.cpu_mean > chosen->cpu) {
    found.push_back(std::format("{}: cpu mean {} us over {} us", shown,
                                micro(summary.cpu_mean), micro(chosen->cpu)));
  }
  const double inputs = std::max(1.0, static_cast<double>(summary.steps));
  if (static_cast<double>(summary.evaluations) / inputs > chosen->evaluations) {
    found.push_back(std::format(
        "{}: {} evaluations in {} inputs, over {:.0f} per input", shown,
        summary.evaluations, summary.steps, chosen->evaluations));
  }
  const std::size_t transient = summary.nodes_max - summary.nodes_min;
  if (summary.steady_recorded_max >
      summary.control_nodes + transient + chosen->ring) {
    found.push_back(std::format(
        "{}: {} nodes recorded in a steady frame, over the control's {}, {} "
        "presented and {} for its focus ring",
        shown, summary.steady_recorded_max, summary.control_nodes, transient,
        chosen->ring));
  }
  const std::uint64_t allowed = pixels(summary, chosen->outset);
  if (summary.damaged_pixels_max > allowed) {
    found.push_back(std::format(
        "{}: {} damaged pixels in a frame, over the control region's {}", shown,
        summary.damaged_pixels_max, allowed));
  }
  if (summary.presented > 0 && static_cast<double>(summary.blurs) /
                                       static_cast<double>(summary.presented) >
                                   chosen->blurs) {
    found.push_back(std::format("{}: {} backdrop blurs in {} frames", shown,
                                summary.blurs, summary.presented));
  }
  return found;
}

}
