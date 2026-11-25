#include <chrome_page.hpp>

#include <arisa/components/button.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/label.hpp>
#include <arisa/components/safe_area_inset.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/composition/presentation_placement.hpp>
#include <arisa/composition/presentation_sizing.hpp>
#include <arisa/composition/search_field_placement.hpp>
#include <arisa/composition/search_toolbar_behavior.hpp>
#include <arisa/composition/toolbar_customization.hpp>
#include <arisa/composition/toolbar_item.hpp>
#include <arisa/composition/toolbar_minimization.hpp>
#include <arisa/composition/toolbar_title_alignment.hpp>
#include <arisa/controls/searchable.hpp>
#include <arisa/motion/animation.hpp>
#include <arisa/motion/with_animation.hpp>
#include <arisa/presentation/navigation_stack.hpp>
#include <arisa/presentation/presentations.hpp>
#include <arisa/presentation/presented_destination.hpp>
#include <arisa/structure/navigation_split_view.hpp>
#include <arisa/structure/tab.hpp>
#include <arisa/structure/tab_bar_placement.hpp>
#include <arisa/structure/tab_view.hpp>
#include <arisa/structure/tab_view_style.hpp>
#include <arisa/theme/radius.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <format>
#include <string>
#include <utility>
#include <vector>

namespace arisa::examples {

using composition::Element;
using composition::infinity;
using composition::ToolbarItem;
using composition::ToolbarItemPlacement;
using motion::with_animation;
using theme::ColourRole;

const composition::AppStorage<structure::TabViewCustomization>
    ChromePage::tab_order{"gallery.chrome.tabs",
                          structure::TabViewCustomization{}};

auto ChromePage::title_of(Demo chosen) -> std::string_view {
  switch (chosen) {
    case Demo::mail:
      return "Mail";
    case Demo::onboarding:
      return "Onboarding";
    case Demo::tabs:
      return "Tabs";
    case Demo::toolbar:
      return "Toolbar";
    case Demo::bottom_bar:
      return "Bottom bar";
    case Demo::toolbar_items:
      return "Toolbar items";
    case Demo::tab_extras:
      return "Tab extras";
    case Demo::search:
      return "Search";
    case Demo::dialogs:
      return "Dialogs";
  }
  return "Mail";
}

auto ChromePage::Failure::error_description() const -> std::string {
  return std::format("Sync failed ({})", code_);
}

auto ChromePage::tile(std::string_view title, std::string_view symbol,
                      std::string_view key, std::function<void()> action)
    -> Element {
  return components::Button(
             [action = std::move(action)] { action(); },
             components::VStack(
                 components::VStackOptions{
                     .alignment = components::HorizontalAlignment::leading,
                     .spacing = theme::space::sm},
                 components::Image::system_name(std::string{symbol})
                     .font(theme::Font::title2),
                 components::Text(std::string{title})
                     .font(theme::Font::headline))
                 .frame({.width = 180.0F,
                         .height = 96.0F,
                         .alignment = composition::Alignment::top_leading}))
      .id(std::string{key});
}

auto ChromePage::tiles(composition::StateHandle<bool> large,
                       composition::StateHandle<std::optional<Demo>> chosen)
    -> Element {
  const auto open = [chosen](Demo each) {
    return std::function<void()>{[chosen, each] { chosen.set(each); }};
  };
  return components::ScrollView(
             components::VStack(
                 components::VStackOptions{
                     .alignment = components::HorizontalAlignment::leading,
                     .spacing = theme::space::lg},
                 components::HStack(
                     components::HStackOptions{.spacing = theme::space::lg},
                     tile("Large title", "text_fields", large_button,
                          [large] { large.set(true); }),
                     tile("Mail", "mail", mail_button, open(Demo::mail)),
                     tile("Onboarding", "rocket_launch", onboarding_button,
                          open(Demo::onboarding))),
                 components::HStack(
                     components::HStackOptions{.spacing = theme::space::lg},
                     tile("Tabs", "dashboard", tabs_button, open(Demo::tabs)),
                     tile("Toolbar", "tune", toolbar_button,
                          open(Demo::toolbar)),
                     tile("Bottom bar", "view_list", bottom_button,
                          open(Demo::bottom_bar))),
                 components::HStack(
                     components::HStackOptions{.spacing = theme::space::lg},
                     tile("Toolbar items", "build", items_button,
                          open(Demo::toolbar_items)),
                     tile("Tab extras", "tab", extras_button,
                          open(Demo::tab_extras)),
                     tile("Search", "search", search_button,
                          open(Demo::search)),
                     tile("Dialogs", "chat_bubble", dialogs_button,
                          open(Demo::dialogs))))
                 .padding(theme::space::xl)
                 .frame({.max_width = infinity,
                         .alignment = composition::Alignment::leading}))
      .frame({.max_width = infinity, .max_height = infinity});
}

auto ChromePage::rows(std::string_view prefix, int count) -> Element {
  std::vector<Element> lines;
  lines.reserve(static_cast<std::size_t>(count));
  for (int index = 1; index <= count; ++index) {
    lines.push_back(
        components::Text(std::format("{} {}", prefix, index))
            .padding(
                theme::Insets::symmetric(theme::space::lg, theme::space::sm))
            .frame({.max_width = infinity,
                    .alignment = composition::Alignment::leading})
            .background(ColourRole::surface_raised,
                        theme::Shape::rounded(theme::radius::control, true))
            .focusable()
            .id(composition::ElementKey{static_cast<std::int64_t>(index)}));
  }
  return components::VStack(
      components::VStackOptions{
          .alignment = components::HorizontalAlignment::leading,
          .spacing = theme::space::sm},
      std::move(lines));
}

auto ChromePage::large_title() -> Element {
  return components::ScrollView(rows("Row", row_count)
                                    .padding(theme::space::xl)
                                    .frame({.max_width = infinity}))
      .frame({.max_width = infinity, .max_height = infinity})
      .navigation_title("Large title")
      .navigation_subtitle("Collapses on scroll")
      .navigation_bar_title_display_mode(
          composition::NavigationBarTitleDisplayMode::large);
}

auto ChromePage::mail(composition::StateHandle<int> box,
                      composition::StateHandle<int> letter) -> Element {
  std::vector<Element> boxes;
  for (std::size_t index = 0; index < mailboxes.size(); ++index) {
    const int chosen = static_cast<int>(index);
    boxes.push_back(
        components::Button(
            [box, letter, chosen] {
              box.set(chosen);
              letter.set(0);
            },
            components::Label(
                std::string{mailboxes[index]},
                components::Image::system_name(
                    index == 0 ? "mail" : (index == 1 ? "share" : "folder"))))
            .frame({.max_width = infinity,
                    .alignment = composition::Alignment::leading})
            .id(composition::ElementKey{static_cast<std::int64_t>(index)}));
  }
  std::vector<Element> letters;
  for (int index = 0; index < message_count; ++index) {
    letters.push_back(
        components::Button(
            [letter, index] { letter.set(index); },
            components::VStack(
                components::VStackOptions{
                    .alignment = components::HorizontalAlignment::leading,
                    .spacing = theme::space::none},
                components::Text(std::format("Message {}", index + 1))
                    .font(theme::Font::headline),
                components::Text(
                    std::string{mailboxes[static_cast<std::size_t>(box.get())]})
                    .font(theme::Font::subheadline)
                    .foreground_style(ColourRole::text_secondary)))
            .frame({.max_width = infinity,
                    .alignment = composition::Alignment::leading})
            .id(composition::ElementKey{static_cast<std::int64_t>(index)}));
  }
  return structure::NavigationSplitView(
             components::ScrollView(
                 components::VStack(
                     components::VStackOptions{
                         .alignment = components::HorizontalAlignment::leading,
                         .spacing = theme::space::xs},
                     std::move(boxes))
                     .padding(theme::space::sm)
                     .frame({.max_width = infinity})),
             components::ScrollView(
                 components::VStack(
                     components::VStackOptions{
                         .alignment = components::HorizontalAlignment::leading,
                         .spacing = theme::space::xs},
                     std::move(letters))
                     .padding(theme::space::sm)
                     .frame({.max_width = infinity}))
                 .navigation_split_view_column_width(std::nullopt, 220.0F,
                                                     280.0F),
             components::VStack(
                 components::VStackOptions{
                     .alignment = components::HorizontalAlignment::leading,
                     .spacing = theme::space::md},
                 components::Text(std::format("Message {}", letter.get() + 1))
                     .font(theme::Font::title2),
                 components::Text(
                     "Three columns when docked, two when handheld.")
                     .foreground_style(ColourRole::text_secondary))
                 .padding(theme::space::xl)
                 .frame({.max_width = infinity,
                         .max_height = infinity,
                         .alignment = composition::Alignment::top_leading}))
      .navigation_title("Mail");
}

auto ChromePage::onboarding(composition::StateHandle<int> step) -> Element {
  const std::array<std::string_view, 3> titles{"Welcome", "Pair a controller",
                                               "Ready"};
  std::vector<Element> pages;
  for (std::size_t index = 0; index < titles.size(); ++index) {
    const std::string_view title = titles[index];
    const int next =
        (static_cast<int>(index) + 1) % static_cast<int>(titles.size());
    pages.push_back(structure::Tab(
        std::string{title}, "radio_button_unchecked", static_cast<int>(index),
        [title, step, next] {
          return components::VStack(
                     components::VStackOptions{
                         .alignment = components::HorizontalAlignment::center,
                         .spacing = theme::space::lg},
                     components::Image::system_name("sports_esports")
                         .font(theme::Font::large_title),
                     components::Text(std::string{title})
                         .font(theme::Font::title2),
                     components::Button(
                         next == 0 ? "Start over" : "Continue",
                         [step, next] {
                           with_animation(motion::Animation::snappy(0.35),
                                          [&step, next] { step.set(next); });
                         })
                         .id(std::format("chrome-continue-{}", next)))
              .frame({.max_width = infinity,
                      .max_height = infinity,
                      .alignment = composition::Alignment::center});
        }));
  }
  return structure::TabView(step, std::move(pages))
      .tab_view_style(structure::PageTabViewStyle{})
      .navigation_title("Onboarding");
}

auto ChromePage::tabs(
    composition::StateHandle<int> selected,
    composition::Binding<structure::TabViewCustomization> order) -> Element {
  const auto tab_page = [](std::string_view title) {
    return structure::Tab::Builder{[title] {
      return components::Text(std::string{title})
          .font(theme::Font::title2)
          .padding(theme::space::xl)
          .frame({.max_width = infinity,
                  .max_height = infinity,
                  .alignment = composition::Alignment::top_leading});
    }};
  };
  return structure::TabView(
             selected,
             structure::TabSection(
                 "Library",
                 Element{structure::Tab("Games", "sports_esports", 0,
                                        tab_page("Games"))
                             .customization_id("games")}
                     .badge(3),
                 structure::Tab("Media", "photo_library", 1, tab_page("Media"))
                     .customization_id("media"),
                 structure::Tab("Friends", "group", 2, tab_page("Friends"))
                     .customization_id("friends")),
             structure::TabSection(
                 "Store",
                 Element{
                     structure::Tab("News", "new_releases", 3, tab_page("News"))
                         .customization_id("news")}
                     .badge(std::optional<std::string>{"New"}),
                 structure::Tab("Settings", "settings", 4, tab_page("Settings"))
                     .customization_id("settings")),
             structure::Tab(structure::TabRole::search, 5, tab_page("Search")))
      .tab_view_customization(order)
      .tab_view_style(structure::SidebarAdaptableTabViewStyle{})
      .tab_view_sidebar_header([] {
        return components::Text("arisa")
            .font(theme::Font::title3)
            .font_weight(theme::Font::Weight::bold)
            .padding(
                theme::Insets::symmetric(theme::space::sm, theme::space::xs));
      })
      .tab_view_sidebar_footer([order] {
        return components::Button(
                   "Reset order",
                   [order] {
                     order.update([](structure::TabViewCustomization& held) {
                       held.reset_tab_order();
                     });
                   })
            .padding(
                theme::Insets::symmetric(theme::space::sm, theme::space::md))
            .id("chrome-reset-order");
      })
      .navigation_title("Tabs");
}

auto ChromePage::toolbar(composition::StateHandle<int> count,
                         composition::StateHandle<bool> edits) -> Element {
  const auto bump = [count] { count.set(count.get() + 1); };
  return components::VStack(
             components::VStackOptions{
                 .alignment = components::HorizontalAlignment::leading,
                 .spacing = theme::space::md},
             components::Text(std::format("Actions run: {}", count.get()))
                 .font(theme::Font::title3),
             components::Button(
                 edits.get() ? "Role: editor" : "Role: automatic",
                 [edits] { edits.set(!edits.get()); }))
      .padding(theme::space::xl)
      .frame({.max_width = infinity,
              .max_height = infinity,
              .alignment = composition::Alignment::top_leading})
      .navigation_title("Toolbar")
      .toolbar_role(edits.get() ? composition::ToolbarRole::editor
                                : composition::ToolbarRole::automatic)
      .toolbar_title_menu([bump] {
        return components::Group(components::Button("Rename", bump),
                                 components::Button("Duplicate", bump));
      })
      .toolbar(
          composition::ToolbarItemGroup(
              ToolbarItemPlacement::primary_action,
              components::Button(bump, components::Image::system_name("add")),
              components::Button(bump,
                                 components::Image::system_name("share"))),
          composition::ToolbarSpacer(composition::SpacerSizing::fixed,
                                     ToolbarItemPlacement::primary_action),
          ToolbarItem(ToolbarItemPlacement::primary_action,
                      components::Button(
                          bump, components::Image::system_name("delete"))),
          ToolbarItem(ToolbarItemPlacement::secondary_action,
                      [bump] { return components::Button("Archive", bump); }),
          ToolbarItem(ToolbarItemPlacement::secondary_action,
                      [bump] { return components::Button("Export", bump); }),
          ToolbarItem(ToolbarItemPlacement::bottom_bar,
                      components::Button("Previous", bump)),
          composition::ToolbarSpacer(composition::SpacerSizing::flexible,
                                     ToolbarItemPlacement::bottom_bar),
          ToolbarItem(ToolbarItemPlacement::bottom_bar,
                      components::Button("Next", bump)));
}

auto ChromePage::bottom_bar(composition::StateHandle<int> count) -> Element {
  return components::ScrollView(rows("Item", row_count)
                                    .padding(theme::space::xl)
                                    .frame({.max_width = infinity}))
      .frame({.max_width = infinity, .max_height = infinity})
      .safe_area_bar(
          composition::VerticalEdge::bottom,
          components::HStack(
              components::HStackOptions{
                  .alignment = components::VerticalAlignment::center,
                  .spacing = theme::space::md},
              components::Text(std::format("Selected {}", count.get())),
              components::Spacer(),
              components::Button("Add", [count] { count.set(count.get() + 1); })
                  .button_style(components::ButtonStyle::bordered_prominent()))
              .padding(
                  theme::Insets::symmetric(theme::space::xl, theme::space::md))
              .frame({.max_width = infinity})
              .id("chrome-bottom-bar"))
      .navigation_title("Bottom bar");
}

auto ChromePage::toolbar_items(composition::StateHandle<int> count) -> Element {
  const auto bump = [count] { count.set(count.get() + 1); };
  const auto symbol = [bump](std::string_view name) {
    return [bump, name] {
      return components::Button(
          bump, components::Image::system_name(std::string{name}));
    };
  };
  return components::ScrollView(rows("Item", row_count)
                                    .padding(theme::space::xl)
                                    .frame({.max_width = infinity}))
      .frame({.max_width = infinity, .max_height = infinity})
      .navigation_title("Toolbar items")
      .toolbar_title_alignment(composition::ToolbarTitleAlignment::center)
      .toolbar_minimization_behavior(
          composition::ToolbarMinimizationBehavior::on_scroll_down,
          composition::ToolbarPlacement::navigation_bar)
      .toolbar(std::string{toolbar_id},
               ToolbarItem("compose", ToolbarItemPlacement::primary_action,
                           symbol("edit")),
               ToolbarItem("share", ToolbarItemPlacement::primary_action,
                           symbol("share"))
                   .visibility_priority(
                       composition::ToolbarItemVisibilityPriority::low),
               ToolbarItem("flag", ToolbarItemPlacement::primary_action,
                           symbol("flag"))
                   .default_customization(composition::Visibility::hidden),
               ToolbarItem("delete", ToolbarItemPlacement::destructive_action,
                           symbol("delete"))
                   .customization_behavior(
                       composition::ToolbarCustomizationBehavior::disabled),
               ToolbarItem("back", ToolbarItemPlacement::navigation,
                           symbol("arrow_back")))
      .toolbar_overflow_menu(
          [bump] { return components::Button("Mark all read", bump); });
}

auto ChromePage::tab_extras(composition::StateHandle<int> selected) -> Element {
  const auto tab_page = [](std::string_view title) {
    return structure::Tab::Builder{[title] {
      std::vector<Element> lines;
      for (int index = 1; index <= row_count; ++index) {
        lines.push_back(
            components::Text(std::format("{} {}", title, index))
                .padding(theme::Insets::symmetric(theme::space::lg,
                                                  theme::space::sm))
                .frame({.max_width = infinity,
                        .alignment = composition::Alignment::leading})
                .focusable()
                .id(composition::ElementKey{static_cast<std::int64_t>(index)}));
      }
      return Element{
          components::ScrollView(components::VStack(std::move(lines))
                                     .padding(theme::space::xl)
                                     .frame({.max_width = infinity}))
              .frame({.max_width = infinity, .max_height = infinity})};
    }};
  };
  return structure::TabView(
             selected,
             structure::Tab("Home", "home", 0, tab_page("Home"))
                 .tab_placement(structure::TabPlacement::pinned),
             structure::Tab("Library", "photo_library", 1, tab_page("Library")),
             structure::Tab("New", "add", 2, structure::TabRole::prominent,
                            tab_page("New")),
             structure::Tab("Search", "search", 3, structure::TabRole::search,
                            tab_page("Search")))
      .tab_bar_minimize_behavior(
          structure::TabBarMinimizeBehavior::on_scroll_down)
      .tab_view_bottom_accessory([] {
        return components::HStack(
                   components::HStackOptions{
                       .alignment = components::VerticalAlignment::center,
                       .spacing = theme::space::md},
                   components::Image::system_name("music_note"),
                   components::Text("Now playing"), components::Spacer(),
                   components::Button(
                       [] {}, components::Image::system_name("play_arrow")))
            .frame({.max_width = infinity})
            .id("chrome-accessory");
      })
      .navigation_title("Tab extras");
}

auto ChromePage::search(composition::StateHandle<std::string> text,
                        composition::StateHandle<bool> active) -> Element {
  std::vector<Element> found;
  for (int index = 1; index <= row_count; ++index) {
    const std::string name = std::format("Game {}", index);
    if (!text.get().empty() && name.find(text.get()) == std::string::npos) {
      continue;
    }
    found.push_back(
        components::Text(name)
            .padding(
                theme::Insets::symmetric(theme::space::lg, theme::space::sm))
            .frame({.max_width = infinity,
                    .alignment = composition::Alignment::leading})
            .focusable()
            .id(composition::ElementKey{static_cast<std::int64_t>(index)}));
  }
  return components::ScrollView(components::VStack(std::move(found))
                                    .padding(theme::space::xl)
                                    .frame({.max_width = infinity}))
      .frame({.max_width = infinity, .max_height = infinity})
      .searchable(text, composition::Binding<bool>{active},
                  composition::SearchFieldPlacement::toolbar)
      .search_toolbar_behavior(composition::SearchToolbarBehavior::minimize)
      .search_suggestions([] {
        return components::VStack(
            components::Text("Game 1").search_completion("Game 1"),
            components::Text("Game 12").search_completion("Game 12"));
      })
      .navigation_title("Search")
      .toolbar(
          ToolbarItem(ToolbarItemPlacement::bottom_bar,
                      components::Button(
                          active.get() ? "Stop searching" : "Start searching",
                          [active] { active.set(!active.get()); })
                          .id("chrome-search-toggle")));
}

auto ChromePage::dialogs(const Handles& handles) -> Element {
  const composition::StateHandle<std::optional<int>> failure_code =
      handles.failing;
  const composition::Binding<std::optional<Failure>> error{
      [failure_code] {
        return failure_code.get().has_value()
                   ? std::optional<Failure>{Failure{*failure_code.get()}}
                   : std::nullopt;
      },
      [failure_code](std::optional<Failure> value) {
        failure_code.set(value.has_value() ? std::optional<int>{value->code()}
                                           : std::nullopt);
      }};
  const composition::StateHandle<std::optional<int>> pending_item =
      handles.pending;
  const composition::StateHandle<bool> sheet_placed = handles.placed;
  return components::VStack(
             components::VStackOptions{
                 .alignment = components::HorizontalAlignment::leading,
                 .spacing = theme::space::md},
             components::Button("Delete an item",
                                [pending_item] { pending_item.set(3); })
                 .id("chrome-dialog-item"),
             components::Button("Sync",
                                [failure_code] { failure_code.set(503); })
                 .id("chrome-dialog-error"),
             components::Button("Side sheet",
                                [sheet_placed] { sheet_placed.set(true); })
                 .id("chrome-dialog-sheet"))
      .padding(theme::space::xl)
      .frame({.max_width = infinity,
              .max_height = infinity,
              .alignment = composition::Alignment::top_leading})
      .confirmation_dialog("Delete?", pending_item,
                           composition::Visibility::visible,
                           [](const int& value) {
                             return components::Button(
                                 std::format("Delete item {}", value),
                                 components::ButtonRole::destructive, [] {});
                           })
      .alert(error,
             [](const Failure& failure) {
               return components::Button(failure.code() >= 500 ? "Retry" : "OK",
                                         [] {});
             })
      .sheet(
          sheet_placed,
          [sheet_placed] {
            return components::VStack(
                       components::Text("Side sheet").font(theme::Font::title2),
                       components::Button(
                           "Done", [sheet_placed] { sheet_placed.set(false); }))
                .padding(theme::space::xl)
                .presentation_sizing(
                    composition::PresentationSizing::form.fitting(false, true))
                .presentation_placement(
                    composition::PresentationPlacement::trailing);
          })
      .navigation_title("Dialogs");
}

auto ChromePage::shown(const Handles& handles, Demo chosen) -> Element {
  switch (chosen) {
    case Demo::mail:
      return mail(handles.mailbox, handles.message);
    case Demo::onboarding:
      return onboarding(handles.page);
    case Demo::tabs:
      return tabs(handles.tab, handles.order);
    case Demo::toolbar:
      return toolbar(handles.taps, handles.editor);
    case Demo::bottom_bar:
      return bottom_bar(handles.taps);
    case Demo::toolbar_items:
      return toolbar_items(handles.taps);
    case Demo::tab_extras:
      return tab_extras(handles.tab);
    case Demo::search:
      return search(handles.query, handles.searching);
    case Demo::dialogs:
      return dialogs(handles);
  }
  return mail(handles.mailbox, handles.message);
}

auto ChromePage::body(composition::Context& context) const -> Element {
  const auto large = context.state(large_shown);
  const auto chosen = context.state(demo);
  const auto shown_flag = shown_;
  const Handles held{.mailbox = context.state(mailbox),
                     .message = context.state(message),
                     .page = context.state(page),
                     .tab = context.state(tab),
                     .taps = context.state(taps),
                     .editor = context.state(editor),
                     .order = context.app_storage(tab_order),
                     .searching = context.state(searching),
                     .query = context.state(query),
                     .pending = context.state(pending),
                     .failing = context.state(failing),
                     .placed = context.state(placed)};
  return presentation::NavigationStack([large, chosen, shown_flag, held] {
    return tiles(large, chosen)
        .navigation_title("Chrome")
        .toolbar(ToolbarItem(
            ToolbarItemPlacement::cancellation_action,
            components::Button("Close", [shown_flag] { shown_flag.set(false); })
                .id(std::string{close_button})))
        .navigation_destination(composition::Binding<bool>{large},
                                [] { return large_title(); })
        .navigation_destination(
            chosen, [held](const Demo& each) { return shown(held, each); });
  });
}

auto ChromePage::Opener::body(composition::Context& context) const -> Element {
  const auto shown = context.state(shown_key);
  return components::Button("Chrome", [shown] { shown.set(true); })
      .id(std::string{button})
      .full_screen_cover(shown, [shown] { return Element{ChromePage{shown}}; });
}

}
