#include <fidelity_page.hpp>

#include <arisa/components/environment.hpp>
#include <arisa/composition/alignment.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/markup/markup.hpp>
#include <arisa/theme/metrics.hpp>
#include <arisa/theme/radius.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/stroke.hpp>

#include <format>
#include <string>
#include <utility>

namespace arisa::examples {

using namespace markup;
using composition::Element;
using composition::infinity;
using theme::ColourRole;
using theme::RoundedCornerStyle;
using theme::RoundedRectangle;

const composition::StateKey<std::string> FidelityPage::typed{"fidelity-typed",
                                                             std::string{}};

auto FidelityPage::key(std::string_view element) -> std::string {
  return std::format("fidelity.{}", element);
}

auto FidelityPage::section(std::string_view title, Element content) -> Element {
  return VStack(alignment::leading, spacing(theme::space::md),
                padding(theme::space::lg),
                frame({.max_width = infinity,
                       .alignment = composition::Alignment::leading}),
                background(ColourRole::surface_raised,
                           RoundedRectangle(theme::radius::card,
                                            RoundedCornerStyle::continuous)),
                border(ColourRole::separator, theme::Stroke::hairline()))(
      Text(font::headline)(std::string{title}), std::move(content));
}

auto FidelityPage::picture(Reference reference, theme::ColorScheme scheme)
    -> Element {
  return Image(resizable,
               frame({.width = reference.width, .height = reference.height}))(
      std::format("{}{}_{}.png", directory, reference.name,
                  scheme == theme::ColorScheme::dark ? "dark" : "light"));
}

auto FidelityPage::pair(std::string_view element, Element arisa,
                        Reference reference, theme::ColorScheme scheme)
    -> Element {
  return Element{
      HStack(alignment::center, spacing(theme::space::xl))(
          VStack(alignment::leading,
                 frame({.width = list_width,
                        .alignment = composition::Alignment::leading}))(
              std::move(arisa).id(key(element))),
          picture(reference, scheme), Spacer())}
      .focus_fallback();
}

auto FidelityPage::choices() -> Element {
  return Group(Text(tag(Choice::one))("One"), Text(tag(Choice::two))("Two"),
               Text(tag(Choice::three))("Three"));
}

auto FidelityPage::text_styles(composition::Context& context,
                               theme::ColorScheme scheme) -> Element {
  const auto text = context.state(typed);
  return section(
      "Text styles",
      VStack(alignment::leading, spacing(theme::space::sm))(
          pair("text.large_title", Text(font::large_title)(std::string{sample}),
               {"text_large_title", 228.0F, 35.0F}, scheme),
          pair("text.title", Text(font::title)(std::string{sample}),
               {"text_title", 195.0F, 30.0F}, scheme),
          pair("text.title3", Text(font::title3)(std::string{sample}),
               {"text_title3", 143.0F, 23.0F}, scheme),
          pair("text.headline", Text(font::headline)(std::string{sample}),
               {"text_headline", 135.0F, 20.0F}, scheme),
          pair("text.body", Text(font::body)(std::string{sample}),
               {"text_body", 127.0F, 20.0F}, scheme),
          pair("text.caption", Text(font::caption)(std::string{sample}),
               {"text_caption", 102.0F, 17.0F}, scheme)));
}

auto FidelityPage::stacks(composition::Context& context,
                          theme::ColorScheme scheme) -> Element {
  const auto text = context.state(typed);
  return section(
      "Stacks, padding and spacer",
      VStack(alignment::leading, spacing(theme::space::sm))(
          pair("vstack",
               VStack(alignment::leading)(Text("A").id(key("vstack.a")),
                                          Text("B").id(key("vstack.b"))),
               {"stack_vstack", 21.0F, 44.0F}, scheme),
          pair("hstack",
               HStack(alignment::center)(Text("A").id(key("hstack.a")),
                                         Text("B").id(key("hstack.b"))),
               {"stack_hstack", 38.0F, 28.0F}, scheme),
          pair("padding",
               VStack(alignment::leading)(
                   Text(padding())("Padded").id(key("padding.text"))),
               {"padding_default", 91.0F, 60.0F}, scheme),
          pair("spacer",
               HStack(alignment::center, spacing(0.0F), fixed_size())(
                   Text("A").id(key("spacer.a")), Spacer(),
                   Text("B").id(key("spacer.b"))),
               {"stack_hstack", 38.0F, 28.0F}, scheme)));
}

auto FidelityPage::buttons(composition::Context& context,
                           theme::ColorScheme scheme) -> Element {
  const auto counted = context.state(count);
  const auto noop = [counted] { counted.set(counted.get()); };
  return section(
      "Buttons",
      VStack(alignment::leading, spacing(theme::space::sm))(
          pair("button.automatic", Button(action(noop))("Button"),
               {"button_automatic_regular", 77.0F, 36.0F}, scheme),
          pair("button.bordered",
               Button(action(noop),
                      button_style(ButtonStyle::bordered()))("Button"),
               {"button_bordered_regular", 77.0F, 36.0F}, scheme),
          pair(
              "button.bordered_prominent",
              Button(action(noop),
                     button_style(ButtonStyle::bordered_prominent()))("Button"),
              {"button_bordered_prominent_regular", 77.0F, 36.0F}, scheme),
          pair("button.borderless",
               Button(action(noop),
                      button_style(ButtonStyle::borderless()))("Button"),
               {"button_borderless_regular", 53.0F, 28.0F}, scheme),
          pair("button.plain",
               Button(action(noop),
                      button_style(ButtonStyle::plain()))("Button"),
               {"button_plain_regular", 53.0F, 28.0F}, scheme)));
}

auto FidelityPage::toggles(composition::Context& context,
                           theme::ColorScheme scheme) -> Element {
  const auto on = context.state(switch_on);
  const auto off = context.state(switch_off);
  const auto box = context.state(box_on);
  return section(
      "Toggles",
      VStack(alignment::leading, spacing(theme::space::sm))(
          pair("toggle.switch_on", Toggle("Switch", on),
               {"toggle_switch_on", 66.0F, 36.0F}, scheme),
          pair("toggle.switch_off", Toggle("Switch", off),
               {"toggle_switch_off", 66.0F, 36.0F}, scheme),
          pair("toggle.checkbox",
               Toggle(toggle_style(CheckboxToggleStyle{}))("Checkbox", box),
               {"toggle_checkbox_labelled", 99.0F, 28.5F}, scheme),
          pair("toggle.button",
               HStack(alignment::center)(
                   Toggle(toggle_style(ButtonToggleStyle{}))("Button", on)),
               {"toggle_button_on", 77.0F, 36.0F}, scheme)));
}

auto FidelityPage::controls(composition::Context& context,
                            theme::ColorScheme scheme) -> Element {
  const auto slid = context.state(level);
  const auto counted = context.state(count);
  const auto chosen = context.state(choice);
  return section(
      "Slider, stepper, picker and menu",
      VStack(alignment::leading, spacing(theme::space::sm))(
          pair("slider", VStack(frame({.width = control_width}))(Slider(slid)),
               {"slider", 212.0F, 28.0F}, scheme),
          pair("stepper",
               Stepper(std::format("Count {}", counted.get()), counted,
                       ClosedRange<int>{.lower_bound = 0, .upper_bound = 9}),
               {"stepper_labelled", 92.0F, 36.0F}, scheme),
          pair("picker.segmented",
               HStack(alignment::center)(
                   Picker(picker_style(SegmentedPickerStyle{}))(
                       "Picker", chosen, [] { return choices(); })),
               {"picker_segmented", 192.0F, 36.0F}, scheme),
          pair("picker.menu",
               Picker("Picker", chosen, [] { return choices(); }),
               {"picker_menu", 84.5F, 36.0F}, scheme),
          pair("menu",
               HStack(alignment::center)(Menu(
                   "Button",
                   [counted] {
                     return Group(
                         Button(action([counted] { counted.set(1); }))("One"),
                         Button(action([counted] { counted.set(2); }))("Two"));
                   })),
               {"menu", 100.5F, 36.0F}, scheme)));
}

auto FidelityPage::progress(composition::Context& context,
                            theme::ColorScheme scheme) -> Element {
  const auto text = context.state(typed);
  return section(
      "Progress and text field",
      VStack(alignment::leading, spacing(theme::space::sm))(
          pair("progress.linear",
               VStack(frame({.width = control_width}))(ProgressView(
                   progress_view_style(LinearProgressViewStyle{}))(0.4)),
               {"progress_linear", 212.0F, 32.0F}, scheme),
          pair("progress.circular", HStack(alignment::center)(ProgressView()),
               {"progress_circular", 44.0F, 44.0F}, scheme),
          pair("text_field",
               VStack(frame({.width = control_width}))(
                   TextField("Placeholder", text)),
               {"text_field_rounded_border", 212.0F, 36.0F}, scheme)));
}

auto FidelityPage::lists(theme::ColorScheme scheme) -> Element {
  return section(
      "Lists and forms",
      VStack(alignment::leading, spacing(theme::space::sm))(
          pair("form",
               VStack(frame({.width = list_width, .height = 240.0F}))(
                   Form(id(key("form.list")))(
                       Section(header(Text("Header")), footer(Text("Footer")))(
                           Text("One"), Text("Two"), Text("Three")))),
               {"form_grouped", 360.0F, 360.0F}, scheme),
          pair(
              "list.plain",
              VStack(frame({.width = 300.0F, .height = 150.0F}))(List(
                  list_style(PlainListStyle{}))(Section(header(Text("Header")))(
                  Text("One"), Text("Two"), Text("Three"), Text("Four")))),
              {"list_plain", 300.0F, 260.0F}, scheme)));
}

auto FidelityPage::alert(composition::Context& context,
                         theme::ColorScheme scheme) -> Element {
  const auto counted = context.state(count);
  const auto noop = [counted] { counted.set(counted.get()); };
  return section(
      "Alert and focus ring",
      HStack(alignment::top, spacing(theme::space::xl))(
          picture({"alert", 260.0F, 218.0F}, scheme),
          VStack(alignment::leading, spacing(theme::space::lg))(
              picture({"focus_ring", 76.0F, 46.0F}, scheme),
              Button(action(noop), id(key("focus.target")))("Focus"))));
}

auto FidelityPage::body(composition::Context& context) const -> Element {
  const theme::ColorScheme scheme =
      context.environment(components::Environment::colour_scheme)
          .value_or(theme::ColorScheme::light);
  return ScrollView(frame({.max_width = infinity, .max_height = infinity}),
                    content_margins(theme::space::sm))(
      VStack(alignment::leading, spacing(theme::space::lg),
             frame({.max_width = infinity,
                    .alignment = composition::Alignment::leading}))(
          text_styles(context, scheme), stacks(context, scheme),
          buttons(context, scheme), toggles(context, scheme),
          controls(context, scheme), progress(context, scheme), lists(scheme),
          alert(context, scheme)));
}

}
