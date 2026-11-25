#include <presentations_page.hpp>

#include <covers_page.hpp>
#include <library_page.hpp>

#include <arisa/async/awaitables.hpp>
#include <arisa/composition/container_background_placement.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/markup/markup.hpp>
#include <arisa/presentation/presentations.hpp>
#include <arisa/theme/radius.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/stroke.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <format>
#include <string>
#include <utility>
#include <vector>

namespace arisa::examples {

using namespace markup;
using namespace std::chrono_literals;
using composition::Element;
using composition::infinity;
using theme::ColourRole;
using theme::RoundedCornerStyle;
using theme::RoundedRectangle;

auto PresentationsPage::card(std::string_view title, Element trigger)
    -> Element {
  return VStack(alignment::leading, spacing(theme::space::md),
                padding(theme::space::lg),
                frame({.max_width = infinity,
                       .alignment = composition::Alignment::leading}),
                background(ColourRole::surface_raised,
                           RoundedRectangle(theme::radius::card,
                                            RoundedCornerStyle::continuous)),
                border(ColourRole::separator, theme::Stroke::hairline()),
                shadow(theme::Elevation::low))(
      Text(font::headline)(std::string{title}), std::move(trigger));
}

auto PresentationsPage::background_of(int choice) -> Background {
  switch (choice) {
    case 1:
      return composition::PresentationBackground{theme::Glass::regular()};
    case 2:
      return composition::PresentationBackground{theme::Glass::clear()};
    case 3:
      return composition::PresentationBackground{theme::Glass::regular().tint(
          theme::ColourValue{ColourRole::accent}.opacity(tinted_glass))};
    default:
      break;
  }
  const auto materials = theme::Material::all();
  const int index = choice - static_cast<int>(glass_labels.size());
  if (index < 0 || index >= static_cast<int>(materials.size())) {
    return std::nullopt;
  }
  return composition::PresentationBackground{
      materials[static_cast<std::size_t>(index)]};
}

auto PresentationsPage::label_of(int choice) -> std::string_view {
  if (choice >= 0 && choice < static_cast<int>(glass_labels.size())) {
    return glass_labels[static_cast<std::size_t>(choice)];
  }
  const auto materials = theme::Material::all();
  const int index = choice - static_cast<int>(glass_labels.size());
  if (index < 0 || index >= static_cast<int>(materials.size())) {
    return glass_labels.front();
  }
  return materials[static_cast<std::size_t>(index)].name();
}

auto PresentationsPage::option_key(int choice) -> std::string {
  return std::format("present-background-{}", choice);
}

auto PresentationsPage::swatch_fill(int choice) -> theme::ColourValue {
  static constexpr std::array<float, choices> weight{0.2F,  0.12F, 0.03F, 0.0F,
                                                     0.06F, 0.1F,  0.15F, 0.22F,
                                                     0.3F,  0.18F, 0.12F, 0.5F};
  if (choice == 3) {
    return theme::ColourValue{ColourRole::accent}.opacity(tinted_glass);
  }
  const auto index =
      static_cast<std::size_t>(std::clamp(choice, 0, choices - 1));
  return theme::ColourValue{ColourRole::label}.opacity(weight[index]);
}

auto PresentationsPage::swatch(int choice) -> Element {
  return ZStack(frame({.width = swatch_extent, .height = swatch_extent}),
                background(swatch_fill(choice),
                           RoundedRectangle(swatch_corner,
                                            RoundedCornerStyle::continuous)),
                border(ColourRole::separator, theme::Stroke::hairline()))();
}

auto PresentationsPage::backgrounds() -> Element {
  std::vector<Element> options;
  for (int choice = 0; choice < choices; ++choice) {
    options.push_back(
        HStack(tag(choice), id(option_key(choice)), spacing(theme::space::sm))(
            swatch(choice), Text(std::string{label_of(choice)})));
  }
  return Group(std::move(options));
}

auto PresentationsPage::picker(Choice choice) -> Element {
  return Picker(id(background_picker))("Background", choice,
                                       [] { return backgrounds(); });
}

auto PresentationsPage::dressed(Element content, const Background& background)
    -> Element {
  if (!background.has_value()) {
    return content;
  }
  return attr::presentation_background{*background}.apply(std::move(content));
}

auto PresentationsPage::delete_actions(Note last, const Background& background)
    -> Element {
  return dressed(Group(Button(role(ButtonRole::destructive), action([last] {
                                last.set("deleted");
                              }))("Delete"),
                       Button(role(ButtonRole::cancel),
                              action([last] { last.set("kept"); }))("Cancel")),
                 background);
}

auto PresentationsPage::sheet_content(Flag shown, Flag nested, Flag lock,
                                      Note last, Choice choice) -> Element {
  const Background background = background_of(choice.get());
  return dressed(
      VStack(
          alignment::leading, spacing(theme::space::lg),
          frame({.max_width = infinity,
                 .max_height = infinity,
                 .alignment = composition::Alignment::leading}),
          interactive_dismiss_disabled(lock.get()),
          alert(
              "Delete from the sheet?", nested,
              [last, background] { return delete_actions(last, background); },
              [] {
                return Text(font::footnote)(
                    "This alert stacks above the sheet.");
              }))(Text(font::title)("Sheet"),
                  Text(font::body)("Content slides up over a dimmed page."),
                  picker(choice),
                  HStack(spacing(theme::space::md))(
                      Button(action([shown] { shown.set(false); }),
                             id(done_button))("Done"),
                      Button(action([nested] { nested.set(true); }),
                             id(nested_button))("Present alert"),
                      Button(action([lock] { lock.set(!lock.get()); }),
                             id(lock_button))(lock.get() ? "Allow B to close"
                                                         : "Block B"))),
      background);
}

auto PresentationsPage::cover_content(Flag shown, Choice choice) -> Element {
  return dressed(
      VStack(alignment::leading, spacing(theme::space::lg),
             frame({.max_width = infinity,
                    .alignment = composition::Alignment::leading}))(
          Text(font::large_title)("Full-screen cover"),
          Text(font::body)("Covers everything, including the header."),
          picker(choice),
          Button(action([shown] { shown.set(false); }))("Close")),
      background_of(choice.get()));
}

auto PresentationsPage::popover_content(Flag shown, Choice choice) -> Element {
  return dressed(
      VStack(alignment::leading, spacing(theme::space::sm),
             frame({.width = popover_width,
                    .alignment = composition::Alignment::leading}))(
          Text(font::headline)("Popover"),
          Text(font::footnote)("Points at the button that opened it."),
          picker(choice),
          Button(action([shown] { shown.set(false); }))("Got it")),
      background_of(choice.get()));
}

auto PresentationsPage::item_content(Item item, int shown) -> Element {
  return VStack(alignment::leading, spacing(theme::space::lg),
                frame({.max_width = infinity,
                       .max_height = infinity,
                       .alignment = composition::Alignment::leading}))(
      Text(font::title)(std::format("Item {}", shown)),
      HStack(spacing(theme::space::md))(
          Button(action([item] { item.set(1); }), id(item_one))("Item 1"),
          Button(action([item] { item.set(2); }), id(item_two))("Item 2"),
          Button(action([item] { item.set(std::nullopt); }))("Done")));
}

auto PresentationsPage::painted_content(Flag shown) -> Element {
  return VStack(alignment::leading, spacing(theme::space::lg),
                frame({.max_width = infinity,
                       .max_height = infinity,
                       .alignment = composition::Alignment::leading}))(
             Text(font::title)("Sheet"),
             Button(action([shown] { shown.set(false); }))("Done"))
      .container_background(
          composition::ContainerBackgroundPlacement::sheet,
          composition::Alignment::bottom_trailing, [] {
            return ZStack(
                frame({.max_width = infinity, .max_height = infinity}),
                background(theme::ColourValue{ColourRole::accent}.opacity(
                    0.18F)))(components::Image::system_name("palette")
                                 .font(theme::Font::large_title)
                                 .foreground_style(ColourRole::accent)
                                 .padding(theme::space::xl));
          });
}

auto PresentationsPage::LoadError::error_description() const -> std::string {
  return "The library could not be loaded";
}

auto PresentationsPage::LoadError::recovery_suggestion() const -> std::string {
  return "Check the SD card and try again";
}

auto PresentationsPage::load() -> std::expected<int, LoadError> {
  return std::unexpected{LoadError{}};
}

auto PresentationsPage::options_content(
    Flag shown,
    composition::StateHandle<composition::PresentationDetent> detent, Flag play)
    -> Element {
  return VStack(
      alignment::leading, spacing(theme::space::lg),
      frame({.max_width = infinity,
             .alignment = composition::Alignment::leading}),
      presentation_detents(
          {composition::PresentationDetent::medium,
           composition::PresentationDetent::large},
          composition::Binding<composition::PresentationDetent>{detent}),
      presentation_drag_indicator(Visibility::visible))(
      Text(font::title)("Options"), Toggle("Autoplay", play),
      Text(font::callout, foreground_style(ColourRole::text_secondary))(
          detent.get() == composition::PresentationDetent::large ? "Large"
                                                                 : "Medium"),
      Button(action([shown] { shown.set(false); }))("Done"));
}

auto PresentationsPage::edit_content(Flag shown, Flag changed) -> Element {
  return VStack(alignment::leading, spacing(theme::space::lg),
                frame({.max_width = infinity,
                       .max_height = infinity,
                       .alignment = composition::Alignment::leading}),
                presentation_sizing(PresentationSizing::form),
                dismissal_confirmation_dialog(
                    "Discard changes?", changed.get(),
                    [changed] {
                      return Button(
                          role(ButtonRole::destructive),
                          action([changed] { changed.set(false); }))("Discard");
                    }))(Text(font::title)("Edit"),
                        Toggle(id(edited_toggle))("Edited", changed),
                        Button(action([shown] { shown.set(false); }))("Save"));
}

auto PresentationsPage::inspector_content(Flag shown) -> Element {
  return VStack(alignment::leading, spacing(theme::space::md),
                inspector_column_width(280.0F))(
      Text(font::headline)("Inspector"), Text("Size 4.2 GB"),
      Text("Played 12 h"),
      Button(action([shown] { shown.set(false); }))("Close"));
}

auto PresentationsPage::erase_actions(Note last) -> Element {
  return Group(dialog_severity(DialogSeverity::critical))(
      Button(role(ButtonRole::destructive),
             action([last] { last.set("erased"); }))("Delete"));
}

auto PresentationsPage::flash(std::optional<runtime::Overlays> overlays)
    -> async::Task<void> {
  if (!overlays.has_value()) {
    co_return;
  }
  runtime::OverlayHandle handle = overlays->show(
      OverlayLayer::toast,
      HStack(
          spacing(theme::space::sm),
          padding(theme::Insets::symmetric(theme::space::lg, theme::space::sm)),
          background(ColourRole::surface_raised, theme::Capsule()),
          margin(theme::Insets{.bottom = theme::space::xxl}))(
          components::Image::system_name("bolt"),
          Text(font::callout)("Overlay")));
  co_await async::sleep(2s);
}

auto PresentationsPage::more(composition::Context& context, Note last)
    -> std::vector<Element> {
  const auto options = context.state(options_shown);
  const auto detent = context.state(options_detent);
  const auto play = context.state(autoplay);
  const auto failed = context.state(load_failed);
  const auto erase = context.state(erase_shown);
  const auto suppressed = context.state(erase_suppressed);
  const auto inspecting = context.state(inspector_shown);
  const auto count = context.state(flashes);
  const auto editing = context.state(edit_shown);
  const auto changed = context.state(edited);
  const auto covered = context.state(cover_item);
  const auto pointed = context.state(popover_item);
  const auto overlays = context.environment(runtime::Overlays::environment);
  std::vector<Element> cards;
  cards.push_back(
      card("Detents", Button(action([options] { options.set(true); }),
                             id(options_button), help("Opens at medium height"),
                             sheet(options, [options, detent, play] {
                               return options_content(options, detent, play);
                             }))("Show options")));
  cards.push_back(card(
      "Error alert",
      Button(action([failed] { failed.set(true); }), id(load_button))("Load")
          .alert(failed, load(), [](const LoadError&) {
            return Button(role(ButtonRole::cancel))("OK");
          })));
  cards.push_back(
      card("Don't ask again",
           Button(action([erase, suppressed, last] {
                    if (suppressed.get()) {
                      last.set("erased");
                      return;
                    }
                    erase.set(true);
                  }),
                  id(erase_button))("Delete save")
               .confirmation_dialog(
                   "Delete the save?", erase, Visibility::visible,
                   [last] { return erase_actions(last); },
                   [suppressed] {
                     return Text("This cannot be undone")
                         .dialog_suppression_toggle(
                             composition::Binding<bool>{suppressed});
                   })));
  cards.push_back(
      card("Inspector",
           Button(action([inspecting] { inspecting.set(!inspecting.get()); }),
                  id(inspector_button))(inspecting.get() ? "Hide inspector"
                                                         : "Show inspector")));
  cards.push_back(card(
      "Imperative overlay",
      Button(action([count] { count.set(count.get() + 1); }),
             id(flash_button))("Show overlay")
          .task(composition::ElementKey{static_cast<std::int64_t>(count.get())},
                [overlays, shown = count.get()] {
                  return shown > 0 ? flash(overlays) : flash(std::nullopt);
                })));
  cards.push_back(
      card("Dismissal confirmation",
           Button(action([editing] { editing.set(true); }), id(edit_button),
                  sheet(editing, [editing, changed] {
                    return edit_content(editing, changed);
                  }))("Edit")));
  cards.push_back(
      card("Item cover",
           Button(action([covered] { covered.set(7); }),
                  id(cover_item_button))("Show item 7")
               .full_screen_cover(covered, [covered](int shown) {
                 return VStack(alignment::leading, spacing(theme::space::lg))(
                     Text(font::large_title)(std::format("Item {}", shown)),
                     Button(action([covered] { covered.set(std::nullopt); }))(
                         "Close"));
               })));
  cards.push_back(
      card("Item popover",
           Button(action([pointed] { pointed.set(3); }),
                  id(popover_item_button))("Show item 3")
               .popover(pointed, [pointed](int shown) {
                 return VStack(
                     alignment::leading, spacing(theme::space::sm),
                     frame({.width = popover_width,
                            .alignment = composition::Alignment::leading}))(
                     Text(font::headline)(std::format("Item {}", shown)),
                     Button(action([pointed] { pointed.set(std::nullopt); }))(
                         "Done"));
               })));
  return cards;
}

auto PresentationsPage::body(composition::Context& context) const -> Element {
  const auto sheet_flag = context.state(sheet_shown);
  const auto cover_flag = context.state(cover_shown);
  const auto alert_flag = context.state(alert_shown);
  const auto nested_flag = context.state(nested_shown);
  const auto dialog_flag = context.state(dialog_shown);
  const auto popover_flag = context.state(popover_shown);
  const auto toast_flag = context.state(toast_shown);
  const auto lock_flag = context.state(locked);
  const auto last = context.state(outcome);
  const auto choice = context.state(backdrop);
  const auto item = context.state(sheet_item);
  const auto painted = context.state(painted_shown);
  const auto library = context.state(library_shown);
  const auto covers = context.state(covers_shown);
  const Background background = background_of(choice.get());
  return ScrollView(frame({.max_width = infinity, .max_height = infinity}))(
             VStack(alignment::leading, spacing(theme::space::lg),
                    frame({.max_width = infinity,
                           .alignment = composition::Alignment::leading}))(
                 LazyVGrid(columns(3, theme::space::lg),
                           spacing(theme::space::lg),
                           frame({.max_width = infinity}))(
                     card("Sheet",
                          Button(action([sheet_flag] { sheet_flag.set(true); }),
                                 id(sheet_button),
                                 sheet(sheet_flag,
                                       [sheet_flag, nested_flag, lock_flag,
                                        last, choice] {
                                         return sheet_content(
                                             sheet_flag, nested_flag, lock_flag,
                                             last, choice);
                                       }))("Show sheet")),
                     card("Full-screen cover",
                          Button(action([cover_flag] { cover_flag.set(true); }),
                                 id(cover_button),
                                 full_screen_cover(cover_flag,
                                                   [cover_flag, choice] {
                                                     return cover_content(
                                                         cover_flag, choice);
                                                   }))("Show cover")),
                     card("Alert",
                          Button(action([alert_flag] { alert_flag.set(true); }),
                                 id(alert_button),
                                 alert(
                                     "Delete the draft?", alert_flag,
                                     [last, background] {
                                       return delete_actions(last, background);
                                     },
                                     [] {
                                       return Text(font::footnote)(
                                           "This cannot be undone.");
                                     }))("Show alert")),
                     card("Confirmation dialog",
                          Button(
                              action([dialog_flag] { dialog_flag.set(true); }),
                              id(dialog_button),
                              confirmation_dialog(
                                  "Share", dialog_flag, Visibility::visible,
                                  [last, background] {
                                    return dressed(Group(Button(action([last] {
                                                           last.set("copied");
                                                         }))("Copy link"),
                                                         Button(action([last] {
                                                           last.set("sent");
                                                         }))("Send")),
                                                   background);
                                  }))("Share")),
                     card("Popover",
                          Button(action([popover_flag] {
                                   popover_flag.set(true);
                                 }),
                                 id(popover_button),
                                 popover(popover_flag,
                                         PopoverAttachmentAnchor::rect(),
                                         theme::Edge::top,
                                         [popover_flag, choice] {
                                           return popover_content(popover_flag,
                                                                  choice);
                                         }))("Show popover")),
                     card("Toast",
                          Button(action([toast_flag] { toast_flag.set(true); }),
                                 id(toast_button),
                                 toast(toast_flag, 2s,
                                       [background] {
                                         return dressed(
                                             Text(font::callout)("Saved to the "
                                                                 "library"),
                                             background);
                                       }))("Show toast")),
                     card("Item sheet", Button(action([item] { item.set(1); }),
                                               id(item_button))("Show item 1")
                                            .sheet(item,
                                                   [item](int shown) {
                                                     return item_content(item,
                                                                         shown);
                                                   })),
                     card("Sheet background",
                          Button(action([painted] { painted.set(true); }),
                                 id(painted_button))("Show sheet")
                              .sheet(painted,
                                     [painted] {
                                       return painted_content(painted);
                                     })),
                     card("Library",
                          Button(action([library] { library.set(true); }),
                                 id(library_button),
                                 full_screen_cover(library,
                                                   [library] {
                                                     return Element{
                                                         LibraryPage{library}};
                                                   }))("Show library")),
                     card("Covers",
                          Button(action([covers] { covers.set(true); }),
                                 id(covers_button),
                                 full_screen_cover(covers,
                                                   [covers] {
                                                     return Element{
                                                         CoversPage{covers}};
                                                   }))("Show covers"))),
                 LazyVGrid(columns(3, theme::space::lg),
                           spacing(theme::space::lg),
                           frame({.max_width = infinity}))(more(context, last)),
                 Text(font::callout,
                      foreground_style(ColourRole::text_secondary))(
                     std::format("Last choice: {}", last.get()))))
      .inspector(composition::Binding<bool>{context.state(inspector_shown)},
                 [shown = context.state(inspector_shown)] {
                   return inspector_content(shown);
                 });
}

}
