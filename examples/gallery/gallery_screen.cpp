#include <crashes_page.hpp>
#include <gallery_screen.hpp>
#include <gallery_wallpaper.hpp>
#include <geometry_page.hpp>
#include <panels_page.hpp>
#include <paths_page.hpp>
#include <world_page.hpp>

#include <arisa/components/environment.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/markup/markup.hpp>
#include <arisa/presentation/presentations.hpp>
#include <arisa/text/locale.hpp>
#include <arisa/text/localized_string_key.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/metrics.hpp>
#include <arisa/theme/radius.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/stroke.hpp>
#include <arisa/theme/theme.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <format>
#include <optional>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

namespace arisa::examples {

using namespace markup;
using composition::Element;
using composition::infinity;
using theme::Capsule;
using theme::ColourRole;
using theme::Elevation;
using theme::RoundedCornerStyle;
using theme::RoundedRectangle;
using theme::Shape;

template <class... Attributes>
auto GalleryScreen::caption(std::string_view label, Attributes&&... attributes)
    -> Element {
  return Text(font::caption, foreground_style(ColourRole::text_secondary),
              std::forward<Attributes>(attributes)...)(std::string{label});
}

template <class... Attributes>
auto GalleryScreen::swatch(composition::Length wide, composition::Length tall,
                           theme::ColourValue fill, Shape shape,
                           Attributes&&... attributes) -> Element {
  return VStack(frame({.width = wide, .height = tall}), background(fill, shape),
                std::forward<Attributes>(attributes)...)();
}

const composition::StateKey<std::vector<FeedPage::Item>>
    GalleryScreen::feed_items{"feed-items", FeedPage::fetch(0, 0)};

auto GalleryScreen::operator==(const GalleryScreen& other) const -> bool {
  const auto address = [](const NoticeRef& ref) {
    return ref.has_value() ? &ref->get() : nullptr;
  };
  return initial_ == other.initial_ && seed_ == other.seed_ &&
         MotionPage{meter_} == MotionPage{other.meter_} &&
         address(notices_) == address(other.notices_) &&
         launch_ == other.launch_ && lettering_ == other.lettering_ &&
         photo_ == other.photo_;
}

auto GalleryScreen::body(composition::Context& context) const -> Element {
  const auto chosen = context.state(selected);
  const auto count = context.state(clicks);
  const auto scheme = context.state(appearance);
  const GalleryPage current = chosen.get().value_or(initial_);
  const std::optional<arisa::appearance::AppearanceModel>& model =
      context.environment(components::Environment::appearance);
  const theme::ColorScheme shade =
      model.has_value() ? context.read(model->settings())
                              .resolved_scheme()
                              .value_or(theme::ColorScheme::light)
                        : scheme.get().value_or(seed_.scheme);
  const auto shown = context.state(notice);
  const auto replay = context.state(replaying);
  const auto done = context.state(launched);
  const auto warm = context.state(warmed);
  const Shared shared{
      .count = count,
      .meter = meter_,
      .feed = context.state(feed_items),
      .feed_position = context.state(feed_position),
      .replay =
          std::make_shared<const std::function<void(LaunchScreen::Style)>>(
              [replay](LaunchScreen::Style style) { replay.set(style); }),
      .lettering_font = lettering_ != nullptr,
      .warm = warm,
      .photo = photo_};
  std::optional<LaunchScreen::Style> showing = replay.get();
  if (!showing.has_value() && !done.get()) {
    showing = launch_;
  }
  const std::string latest =
      notices_.has_value() ? context.read(notices_->get()) : std::string{};
  std::function<void()> toggle = [scheme, shade] { flip(scheme, shade); };
  if (model.has_value()) {
    toggle = [held = *model, shade] { flip(held, shade); };
  }
  Element content =
      tabs(current, chosen, shared)
          .on_change(latest,
                     [shown, latest] {
                       if (!latest.empty()) {
                         shown.set(latest);
                       }
                     })
          .toast(shown, std::chrono::seconds{3},
                 [](const std::string& message) {
                   return Text(font::callout)(message);
                 })
          .navigation_title("arisa")
          .toolbar(
              ToolbarItem(ToolbarItemPlacement::principal,
                          components::Text(text::LocalizedStringKey{std::string{
                                               GalleryPages::title(current)}})
                              .font(theme::Font::title3)
                              .foreground_style(ColourRole::text_secondary)),
              ToolbarItem(ToolbarItemPlacement::top_bar_trailing,
                          Button(action(toggle), id(scheme_button))(
                              std::string{scheme_label(shade)})));
  if (model.has_value()) {
    content = std::move(content).toolbar(ToolbarItem(
        ToolbarItemPlacement::top_bar_trailing, theme_picker(*model)));
  }
  Element scaffold = Scaffold(
      on_command(provider::Button::l, "Previous",
                 [chosen, current] {
                   chosen.set(GalleryPages::next(current, false));
                 }),
      on_command(
          provider::Button::r, "Next",
          [chosen, current] { chosen.set(GalleryPages::next(current, true)); }),
      on_command(provider::Button::y, std::string{scheme_label(shade)},
                 toggle))(std::move(content));
  const theme::Theme& inherited =
      context.environment(components::Environment::theme);
  if (inherited.glass().sidebar.has_value()) {
    scaffold = std::move(scaffold).background(
        Element{GalleryWallpaper{GalleryWallpaper::Look::window}});
  } else {
    scaffold = std::move(scaffold).background(ColourRole::background);
  }
  if (!model.has_value()) {
    scaffold = std::move(scaffold).environment(
        components::Environment::colour_scheme, std::optional{shade});
  }
  if (warm.get()) {
    scaffold =
        std::move(scaffold).theme_override(ThemingPage::warm_theme(inherited));
  }
  const std::string spoken = context.app_storage(WorldPage::language).get();
  if (const auto chosen_locale = text::Locale::create(spoken)) {
    scaffold =
        std::move(scaffold)
            .environment(components::Environment::locale, *chosen_locale)
            .environment(components::Environment::layout_direction,
                         chosen_locale->right_to_left()
                             ? components::LayoutDirection::right_to_left
                             : components::LayoutDirection::left_to_right)
            .environment(components::Environment::calendar,
                         components::Calendar::for_locale(*chosen_locale));
  }
  std::optional<composition::Element> writing;
  if (showing.has_value()) {
    writing = composition::Element{LaunchScreen{
        *showing, std::make_shared<const std::function<void()>>([replay, done] {
          done.set(true);
          replay.set(std::nullopt);
        }),
        lettering_, photo_}};
  }
  return std::move(scaffold).overlay(
      showing.has_value(), launch_layer,
      writing.has_value() ? std::move(*writing)
                          : composition::Element{components::Group()});
}

auto GalleryScreen::theme_picker(
    const arisa::appearance::AppearanceModel& model) -> Element {
  return Menu("Theme",
              [model] {
                std::vector<Element> items;
                const std::string current = model.settings().get().theme;
                for (const std::string& name : model.theme_names()) {
                  const auto pick = [model, name] {
                    model.update(
                        [name](
                            arisa::appearance::AppearanceOverrides& overrides) {
                          overrides.theme = name;
                        });
                  };
                  if (name == current) {
                    items.push_back(components::Button(
                        pick,
                        components::Label(
                            name, components::Image::system_name("check"))));
                  } else {
                    items.push_back(components::Button(name, pick));
                  }
                }
                return Group(std::move(items));
              })
      .id(theme_menu);
}

auto GalleryScreen::policy(std::optional<theme::ColorScheme> scheme)
    -> arisa::appearance::AppearancePolicy {
  arisa::appearance::AppearancePolicy chosen{
      .application = "arisa-gallery",
      .defaults = {},
      .locks = {},
      .packs = {std::string{"arisa-theme-glass"}}};
  if (scheme.has_value()) {
    chosen.defaults.color_scheme =
        *scheme == theme::ColorScheme::dark
            ? arisa::appearance::ColorSchemePreference::dark
            : arisa::appearance::ColorSchemePreference::light;
  }
  return chosen;
}

auto GalleryScreen::tabs(
    GalleryPage shown,
    composition::StateHandle<std::optional<GalleryPage>> chosen,
    const Shared& shared) -> Element {
  std::vector<Element> pages;
  pages.reserve(GalleryPages::all.size());
  for (const GalleryPage each : GalleryPages::all) {
    pages.push_back(
        Tab(std::string{GalleryPages::title(each)},
            std::string{GalleryPages::symbol(each)}, each, [each, shared] {
              return VStack(
                  alignment::leading,
                  padding(theme::Insets{.top = theme::space::lg,
                                        .leading = theme::space::xl,
                                        .bottom = theme::space::md,
                                        .trailing = theme::space::xl}),
                  frame({.max_width = infinity,
                         .max_height = infinity,
                         .alignment = composition::Alignment::top_leading}))(
                  page(each, shared));
            }));
  }
  return Element{
      TabView(structure::TabView::Selection{
                  .is_selected =
                      [shown](const composition::TagValue& tag) {
                        const GalleryPage* value = tag.get<GalleryPage>();
                        return value != nullptr && *value == shown;
                      },
                  .select =
                      [chosen](const composition::TagValue& tag) {
                        if (const GalleryPage* value = tag.get<GalleryPage>()) {
                          chosen.set(*value);
                        }
                      }},
              std::move(pages))}
      .tab_view_style(SidebarAdaptableTabViewStyle{})
      .sidebar_collapse(
          structure::SidebarCollapse{.detail_only = true, .icons_only = true});
}

auto GalleryScreen::flip(const arisa::appearance::AppearanceModel& model,
                         theme::ColorScheme current) -> void {
  model.update([current](arisa::appearance::AppearanceOverrides& overrides) {
    overrides.color_scheme =
        current == theme::ColorScheme::dark
            ? arisa::appearance::ColorSchemePreference::light
            : arisa::appearance::ColorSchemePreference::dark;
  });
}

auto GalleryScreen::flip(
    composition::StateHandle<std::optional<theme::ColorScheme>> scheme,
    theme::ColorScheme current) -> void {
  with_animation(Animation::smooth(0.35), [scheme, current] {
    scheme.set(current == theme::ColorScheme::dark ? theme::ColorScheme::light
                                                   : theme::ColorScheme::dark);
  });
}

auto GalleryScreen::scheme_label(theme::ColorScheme current)
    -> std::string_view {
  return current == theme::ColorScheme::dark ? "Light mode" : "Dark mode";
}

auto GalleryScreen::page(GalleryPage shown, const Shared& shared) -> Element {
  const composition::StateHandle<int> count = shared.count;
  const MotionPage::MeterRef meter = shared.meter;
  switch (shown) {
    case GalleryPage::typography:
      return TypographyPage{};
    case GalleryPage::multilingual:
      return languages();
    case GalleryPage::cards:
      return cards();
    case GalleryPage::layout:
      return layouts();
    case GalleryPage::themes:
      return themes();
    case GalleryPage::counter:
      return counter(count);
    case GalleryPage::motion:
      return MotionPage{meter};
    case GalleryPage::effects:
      return EffectsPage{};
    case GalleryPage::focus:
      return FocusPage{};
    case GalleryPage::scroll:
      return ScrollPage{};
    case GalleryPage::navigation:
      return NavigationPage{};
    case GalleryPage::presentations:
      return PresentationsPage{};
    case GalleryPage::controls:
      return ControlsPage{};
    case GalleryPage::lists:
      return ListsPage{};
    case GalleryPage::icons:
      return IconsPage{};
    case GalleryPage::colours:
      return ColoursPage{};
    case GalleryPage::fidelity:
      return FidelityPage{};
    case GalleryPage::theming:
      return ThemingPage{shared.warm};
    case GalleryPage::settings:
      return SettingsPage{};
    case GalleryPage::paths:
      return PathsPage{};
    case GalleryPage::feed:
      return FeedPage{shared.feed, shared.feed_position};
    case GalleryPage::launch:
      return LaunchPage{shared.replay, shared.lettering_font};
    case GalleryPage::materials:
      return MaterialsPage{};
    case GalleryPage::glass:
      return GlassPage{shared.photo};
    case GalleryPage::feedback:
      return FeedbackPage{};
    case GalleryPage::panels:
      return PanelsPage{};
    case GalleryPage::crashes:
      return CrashesPage{};
  }
  return TypographyPage{};
}

auto GalleryScreen::languages() -> Element {
  struct Line {
    std::string_view locale;
    std::string_view text;
  };
  constexpr std::array lines{
      Line{"en", "Display & Brightness → Wave office 1234"},
      Line{"ja", "設定を変更しました。「次へ」を押してください。"},
      Line{"zh-Hans", "设置已保存。请按「确定」继续。"},
      Line{"zh-Hant", "設定已儲存。請按「確定」繼續。"},
      Line{"ko", "설정이 저장되었습니다. 계속하려면 확인을 누르세요."},
      Line{"ar", "الإعدادات محفوظة"},
      Line{"he", "ההגדרות נשמרו"},
      Line{"th", "การตั้งค่าถูกบันทึกแล้ว"},
      Line{"hi", "सेटिंग्स सहेजी गईं"},
      Line{"en", "Hello שלום world مرحبا 123"}};
  std::vector<Element> rows;
  for (const Line& line : lines) {
    rows.push_back(HStack(alignment::center, spacing(theme::space::lg))(
        caption(line.locale,
                frame({.width = 80.0F,
                       .alignment = composition::Alignment::leading})),
        Text(font::body,
             environment(components::Environment::locale,
                         text::Locale::create(line.locale)
                             .value_or(text::Locale::undetermined())))(
            std::string{line.text})));
  }
  rows.push_back(HStack(alignment::top, spacing(theme::space::lg))(
      caption("ja wrap", frame({.width = 80.0F,
                                .alignment = composition::Alignment::leading})),
      Text(font::body, frame({.max_width = 200.0F}),
           environment(components::Environment::locale,
                       text::Locale::create("ja").value_or(
                           text::Locale::undetermined())))(
          "設定を変更しました。「次へ」を押してください。")));
  return VStack(alignment::leading, spacing(theme::space::md),
                frame({.max_width = infinity,
                       .alignment = composition::Alignment::leading}))(
      VStack(alignment::leading, spacing(theme::space::sm),
             padding(theme::space::xl),
             frame({.max_width = infinity,
                    .alignment = composition::Alignment::leading}),
             background(ColourRole::surface_raised,
                        RoundedRectangle(theme::radius::card,
                                         RoundedCornerStyle::continuous)),
             border(ColourRole::separator, theme::Stroke::hairline()),
             shadow(Elevation::low))(std::move(rows)));
}

auto GalleryScreen::card(std::string_view title, Elevation elevation,
                         Shape shape) -> Element {
  return VStack(alignment::leading, spacing(theme::space::xs),
                padding(theme::space::lg),
                frame({.max_width = infinity,
                       .alignment = composition::Alignment::leading}),
                background(ColourRole::surface_raised, shape),
                border(ColourRole::separator, theme::Stroke::hairline()),
                shadow(elevation))(Text(font::headline)(std::string{title}));
}

auto GalleryScreen::cards() -> Element {
  const RoundedRectangle continuous{theme::radius::card,
                                    RoundedCornerStyle::continuous};
  const RoundedRectangle circular{theme::radius::card,
                                  RoundedCornerStyle::circular};
  return VStack(alignment::leading, spacing(theme::space::xl),
                frame({.max_width = infinity,
                       .alignment = composition::Alignment::leading}))(
      LazyVGrid(columns(3, theme::space::lg), spacing(theme::space::lg),
                frame({.max_width = infinity}))(
          card("Flat", Elevation::none, continuous),
          card("Low", Elevation::low, continuous),
          card("Medium", Elevation::medium, continuous),
          card("High", Elevation::high, continuous),
          card("Circular corners", Elevation::low, circular),
          card("Capsule", Elevation::low, Capsule())),
      HStack(alignment::center, spacing(theme::space::xl))(
          VStack(spacing(theme::space::sm))(
              swatch(120.0F, 120.0F, ColourRole::accent,
                     RoundedRectangle(36.0F, RoundedCornerStyle::continuous)),
              caption("continuous")),
          VStack(spacing(theme::space::sm))(
              swatch(120.0F, 120.0F, ColourRole::accent,
                     RoundedRectangle(36.0F, RoundedCornerStyle::circular)),
              caption("circular")),
          VStack(spacing(theme::space::sm))(
              swatch(240.0F, 120.0F, ColourRole::surface_raised,
                     RoundedRectangle(theme::radius::sheet,
                                      RoundedCornerStyle::continuous),
                     border(ColourRole::separator, theme::Stroke::hairline()),
                     shadow(Elevation::high)),
              caption("sheet radius, high elevation"))));
}

auto GalleryScreen::layouts() -> Element {
  const RoundedRectangle tile{theme::radius::control,
                              RoundedCornerStyle::continuous};
  std::vector<Element> tiles;
  for (int index = 1; index <= 8; ++index) {
    tiles.push_back(
        Text(font::headline, padding(theme::space::sm),
             frame({.max_width = infinity}),
             background(ColourRole::surface_raised, tile),
             border(ColourRole::separator, theme::Stroke::hairline()),
             shadow(Elevation::low))(std::format("{}", index)));
  }
  const auto column = [&tile](auto edge, composition::Alignment placed,
                              std::string_view name) {
    return VStack(edge, spacing(theme::space::xs), padding(theme::space::md),
                  frame({.width = 160.0F, .alignment = placed}),
                  background(ColourRole::surface, tile),
                  border(ColourRole::separator, theme::Stroke::hairline()))(
        swatch(120.0F, 24.0F, ColourRole::accent, tile),
        swatch(80.0F, 24.0F, ColourRole::success, tile),
        swatch(40.0F, 24.0F, ColourRole::warning, tile), caption(name));
  };
  return VStack(alignment::leading, spacing(theme::space::lg),
                frame({.max_width = infinity,
                       .alignment = composition::Alignment::leading}))(
      GeometryPage::Link{},
      HStack(padding(theme::space::md), frame({.max_width = infinity}),
             background(ColourRole::surface, tile),
             border(ColourRole::separator, theme::Stroke::hairline()))(
          swatch(48.0F, 48.0F, ColourRole::accent, tile), Spacer(),
          caption("Spacer pushes these apart"), Spacer(),
          swatch(48.0F, 48.0F, ColourRole::destructive, tile)),
      HStack(alignment::top, spacing(theme::space::lg),
             frame({.max_width = infinity,
                    .alignment = composition::Alignment::top}))(
          column(alignment::leading, composition::Alignment::leading,
                 "leading"),
          column(alignment::center, composition::Alignment::center, "center"),
          column(alignment::trailing, composition::Alignment::trailing,
                 "trailing"),
          ZStack(alignment::top_trailing)(
              swatch(140.0F, 112.0F, ColourRole::primary,
                     RoundedRectangle(theme::radius::card,
                                      RoundedCornerStyle::continuous)),
              Text(font::caption,
                   padding(theme::Edge::horizontal, theme::space::sm),
                   padding(theme::Edge::vertical, theme::space::xxs),
                   foreground_style(ColourRole::on_accent),
                   background(ColourRole::accent, Capsule()))("ZStack")),
          LazyVGrid(columns(4, theme::space::sm), spacing(theme::space::sm),
                    frame({.max_width = infinity}))(std::move(tiles))),
      HStack(spacing(theme::space::sm), frame({.max_width = infinity}))(
          swatch(composition::Length::percent(0.25F), 24.0F, ColourRole::accent,
                 tile),
          swatch(composition::Length::percent(0.5F), 24.0F, ColourRole::success,
                 tile),
          caption("25% and 50%")),
      frames(), table());
}

auto GalleryScreen::table() -> Element {
  const RoundedRectangle tile{theme::radius::control,
                              RoundedCornerStyle::continuous};
  return VStack(alignment::leading, spacing(theme::space::xs))(
      Grid(id(std::string{grid_table}), padding(theme::space::md),
           background(ColourRole::surface, tile),
           border(ColourRole::separator, theme::Stroke::hairline()))(
          GridRow(
              Text(foreground_style(ColourRole::text_secondary))("Resolution")
                  .grid_column_alignment(
                      composition::HorizontalAlignment::trailing),
              Text("1080p").grid_column_alignment(
                  composition::HorizontalAlignment::leading)),
          GridRow(
              Text(foreground_style(ColourRole::text_secondary))("Frame rate"),
              Text("60 fps")),
          Divider().grid_cell_unsized_axes(composition::Axes::horizontal),
          GridRow(Text(foreground_style(ColourRole::text_secondary))("Bitrate"),
                  Text("15000 kbps")),
          GridRow(swatch(160.0F, 20.0F, ColourRole::accent, tile)
                      .grid_cell_columns(2)),
          GridRow(swatch(24.0F, 24.0F, ColourRole::success, tile)
                      .grid_cell_anchor(motion::UnitPoint::top_leading),
                  swatch(24.0F, 24.0F, ColourRole::warning, tile)
                      .grid_cell_anchor(motion::UnitPoint::bottom_trailing))),
      caption("Grid"));
}

auto GalleryScreen::frames() -> Element {
  const RoundedRectangle tile{theme::radius::control,
                              RoundedCornerStyle::continuous};
  const auto aligned = [&tile](composition::Alignment where,
                               std::string_view name, std::string_view key) {
    return VStack(alignment::center, spacing(theme::space::xs))(
        ZStack(frame({.width = 132.0F, .height = 64.0F, .alignment = where}),
               background(ColourRole::surface, tile),
               border(ColourRole::separator, theme::Stroke::hairline()),
               id(std::string{key}))(
            swatch(40.0F, 24.0F, ColourRole::accent, tile)),
        caption(name));
  };
  std::vector<Element> ideal;
  for (int index = 0; index < 4; ++index) {
    ideal.push_back(VStack(
        frame({.height = 24.0F, .ideal_width = 120.0F}),
        background(index % 2 == 0 ? ColourRole::success : ColourRole::warning,
                   tile))());
  }
  return HStack(
      alignment::top, spacing(theme::space::lg), wrap,
      frame({.max_width = infinity, .alignment = composition::Alignment::top}))(
      aligned(composition::Alignment::leading, "leading", frame_leading),
      aligned(composition::Alignment::center, "center", frame_center),
      aligned(composition::Alignment::trailing, "trailing", frame_trailing),
      VStack(alignment::center, spacing(theme::space::xs))(
          components::ScrollView(
              composition::Axes::horizontal,
              HStack(spacing(theme::space::sm))(std::move(ideal)))
              .frame({.width = 220.0F, .height = 64.0F})
              .background(ColourRole::surface, tile)
              .id(std::string{ideal_strip}),
          caption("ideal width")),
      VStack(alignment::center, spacing(theme::space::xs))(
          HStack(spacing(theme::space::sm), frame({.height = 64.0F}))(
              swatch(32.0F, 32.0F, ColourRole::accent, tile),
              swatch(32.0F, 32.0F, ColourRole::destructive, tile, hidden,
                     id(std::string{hidden_tile})),
              swatch(32.0F, 32.0F, ColourRole::accent, tile)),
          caption("hidden")));
}

auto GalleryScreen::preview(std::string_view name, theme::ColorScheme scheme)
    -> Element {
  return VStack(alignment::leading, spacing(theme::space::md),
                padding(theme::space::xl),
                frame({.max_width = infinity,
                       .alignment = composition::Alignment::leading}),
                background(ColourRole::background,
                           RoundedRectangle(theme::radius::sheet,
                                            RoundedCornerStyle::continuous)),
                border(ColourRole::separator, theme::Stroke::hairline()),
                environment(components::Environment::colour_scheme, scheme))(
      Text(font::title)(std::string{name}),
      HStack(spacing(theme::space::sm))(
          Button(action([] {}))("Continue"),
          swatch(28.0F, 28.0F, ColourRole::success, Capsule()),
          swatch(28.0F, 28.0F, ColourRole::warning, Capsule()),
          swatch(28.0F, 28.0F, ColourRole::destructive, Capsule())),
      card("Card", Elevation::medium,
           RoundedRectangle(theme::radius::card,
                            RoundedCornerStyle::continuous)));
}

auto GalleryScreen::themes() -> Element {
  const auto seeded = [](std::string_view name, theme::Colour accent) {
    return Text(environment(components::Environment::theme,
                            theme::Theme::from(theme::Seed{
                                .accent = accent,
                                .scheme = theme::ColorScheme::light})),
                foreground_style(ColourRole::on_accent),
                padding(theme::Insets::symmetric(
                    theme::metrics::button.horizontal_padding,
                    theme::metrics::button.vertical_padding)),
                background(ColourRole::accent,
                           RoundedRectangle(theme::radius::control,
                                            RoundedCornerStyle::continuous)))(
        std::string{name});
  };
  return VStack(alignment::leading, spacing(theme::space::lg),
                frame({.max_width = infinity,
                       .alignment = composition::Alignment::leading}))(
      HStack(alignment::top, spacing(theme::space::lg),
             frame({.max_width = infinity,
                    .alignment = composition::Alignment::top}))(
          preview("Light", theme::ColorScheme::light),
          preview("Dark", theme::ColorScheme::dark)),
      HStack(spacing(theme::space::lg))(
          seeded("Green", theme::Colour::hex(0x34C759)),
          seeded("Orange", theme::Colour::hex(0xFF9500)),
          seeded("Purple", theme::Colour::hex(0xAF52DE)),
          seeded("Pink", theme::Colour::hex(0xFF2D55))));
}

auto GalleryScreen::counter(composition::StateHandle<int> count) -> Element {
  return ScrollView(frame({.max_width = infinity, .max_height = infinity}),
                    content_margins(theme::space::lg))(VStack(
      alignment::leading, spacing(theme::space::md),
      frame({.max_width = infinity,
             .alignment = composition::Alignment::leading}))(tally(count)));
}

auto GalleryScreen::tally(composition::StateHandle<int> count) -> Element {
  return VStack(alignment::leading, spacing(theme::space::md),
                padding(theme::space::xl),
                frame({.min_width = 360.0F,
                       .alignment = composition::Alignment::leading}),
                background(ColourRole::surface_raised,
                           RoundedRectangle(theme::radius::card,
                                            RoundedCornerStyle::continuous)),
                border(ColourRole::separator, theme::Stroke::hairline()),
                shadow(Elevation::medium))(
      Text(font::large_title)(std::format("count: {}", count.get())),
      HStack(spacing(theme::space::md))(
          Button(action([count] { count.update([](int& value) { ++value; }); }),
                 id(increment_button))("Increment"),
          Button(action([count] { count.set(0); }), id(reset_button))("Reset")),
      VStack(alignment::leading, spacing(theme::space::xxs))(ForEach(
          std::views::iota(0, count.get()), [](int item) { return item; },
          [](int item) {
            return Text(font::footnote)(std::format("item {}", item + 1));
          })));
}

}
