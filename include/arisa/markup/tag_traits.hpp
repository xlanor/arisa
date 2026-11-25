#pragma once

#include <arisa/attr/controls.hpp>
#include <arisa/attr/structure.hpp>
#include <arisa/attr/typography.hpp>
#include <arisa/components/alignment.hpp>
#include <arisa/components/button.hpp>
#include <arisa/components/divider.hpp>
#include <arisa/components/for_each.hpp>
#include <arisa/components/geometry_reader.hpp>
#include <arisa/components/grid.hpp>
#include <arisa/components/grid_row.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/label.hpp>
#include <arisa/components/labeled_content.hpp>
#include <arisa/components/lazy_hgrid.hpp>
#include <arisa/components/lazy_hstack.hpp>
#include <arisa/components/lazy_vgrid.hpp>
#include <arisa/components/lazy_vstack.hpp>
#include <arisa/components/link.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/scroll_view_reader.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/stack_options.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/view_that_fits.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/controls/color_picker.hpp>
#include <arisa/controls/date_picker.hpp>
#include <arisa/controls/gauge.hpp>
#include <arisa/controls/menu.hpp>
#include <arisa/controls/multi_date_picker.hpp>
#include <arisa/controls/picker.hpp>
#include <arisa/controls/progress_view.hpp>
#include <arisa/controls/secure_field.hpp>
#include <arisa/controls/slider.hpp>
#include <arisa/controls/stepper.hpp>
#include <arisa/controls/text_editor.hpp>
#include <arisa/controls/text_field.hpp>
#include <arisa/controls/toggle.hpp>
#include <arisa/presentation/navigation_link.hpp>
#include <arisa/presentation/navigation_stack.hpp>
#include <arisa/structure/control_group.hpp>
#include <arisa/structure/disclosure_group.hpp>
#include <arisa/structure/form.hpp>
#include <arisa/structure/group_box.hpp>
#include <arisa/structure/list.hpp>
#include <arisa/structure/scaffold.hpp>
#include <arisa/structure/section.hpp>
#include <arisa/structure/tab.hpp>
#include <arisa/structure/tab_view.hpp>

#include <concepts>
#include <string>
#include <utility>
#include <variant>

namespace arisa::markup {

template <class Tagged>
struct TagTraits;

template <class Tagged>
struct ForwardingTraits {
  using Options = std::monostate;

  static constexpr auto defaults() noexcept -> Options { return Options{}; }

  template <class... Arguments>
    requires std::constructible_from<Tagged, Arguments...>
  static auto make(Options, Arguments&&... arguments) -> Tagged {
    Tagged made(std::forward<Arguments>(arguments)...);
    return made;
  }
};

template <>
struct TagTraits<controls::Toggle> : ForwardingTraits<controls::Toggle> {};

template <>
struct TagTraits<components::ViewThatFits>
    : ForwardingTraits<components::ViewThatFits> {};

template <>
struct TagTraits<controls::Slider> : ForwardingTraits<controls::Slider> {};

template <>
struct TagTraits<controls::Stepper> : ForwardingTraits<controls::Stepper> {};

template <>
struct TagTraits<controls::Picker> : ForwardingTraits<controls::Picker> {};

template <>
struct TagTraits<controls::Menu> : ForwardingTraits<controls::Menu> {};

template <>
struct TagTraits<controls::ProgressView>
    : ForwardingTraits<controls::ProgressView> {};

template <>
struct TagTraits<controls::TextField> : ForwardingTraits<controls::TextField> {
};

template <>
struct TagTraits<controls::SecureField>
    : ForwardingTraits<controls::SecureField> {};

template <>
struct TagTraits<controls::TextEditor>
    : ForwardingTraits<controls::TextEditor> {};

template <>
struct TagTraits<components::Label> : ForwardingTraits<components::Label> {};

template <>
struct TagTraits<structure::DisclosureGroup>
    : ForwardingTraits<structure::DisclosureGroup> {};

template <>
struct TagTraits<components::LabeledContent>
    : ForwardingTraits<components::LabeledContent> {};

template <>
struct TagTraits<structure::GroupBox> : ForwardingTraits<structure::GroupBox> {
};

template <>
struct TagTraits<structure::ControlGroup>
    : ForwardingTraits<structure::ControlGroup> {};

template <>
struct TagTraits<controls::Gauge> : ForwardingTraits<controls::Gauge> {};

template <>
struct TagTraits<controls::DatePicker>
    : ForwardingTraits<controls::DatePicker> {};

template <>
struct TagTraits<controls::MultiDatePicker>
    : ForwardingTraits<controls::MultiDatePicker> {};

template <>
struct TagTraits<controls::ColorPicker>
    : ForwardingTraits<controls::ColorPicker> {};

template <>
struct TagTraits<structure::List> : ForwardingTraits<structure::List> {};

template <>
struct TagTraits<structure::Form> : ForwardingTraits<structure::Form> {};

template <>
struct TagTraits<structure::Section> {
  using Options = structure::Section::Labels;

  static auto defaults() -> Options { return Options{}; }

  template <class... Rows>
    requires std::constructible_from<structure::Section, Options, Rows...>
  static auto make(Options options, Rows&&... rows) -> structure::Section {
    return structure::Section(std::move(options), std::forward<Rows>(rows)...);
  }
};

template <>
struct TagTraits<structure::Tab> : ForwardingTraits<structure::Tab> {};

template <>
struct TagTraits<structure::TabView> : ForwardingTraits<structure::TabView> {};

template <>
struct TagTraits<structure::Scaffold> {
  using Options = structure::ScaffoldOptions;

  static constexpr auto defaults() noexcept -> Options { return Options{}; }

  template <class Content>
    requires std::convertible_to<Content, composition::Element>
  static auto make(Options options, Content&& content) -> structure::Scaffold {
    return structure::Scaffold(
        options, composition::Element{std::forward<Content>(content)});
  }
};

template <>
struct TagTraits<components::Divider> : ForwardingTraits<components::Divider> {
};

template <>
struct TagTraits<components::Image> {
  using Options = attr::ImageOptions;

  static constexpr auto defaults() noexcept -> Options { return Options{}; }

  template <class Name>
    requires std::constructible_from<std::string, Name>
  static auto make(Options options, Name&& name) -> components::Image {
    components::Image image{std::string{std::forward<Name>(name)}};
    if (options.resizable) {
      image = std::move(image).resizable();
    }
    if (options.rendering_mode.has_value()) {
      image = std::move(image).rendering_mode(*options.rendering_mode);
    }
    if (options.interpolation.has_value()) {
      image = std::move(image).interpolation(*options.interpolation);
    }
    return image;
  }

  static auto make(Options options, components::Image::SystemName name)
      -> components::Image {
    components::Image image{std::move(name)};
    if (options.font.has_value()) {
      return std::move(image).font(*options.font);
    }
    return image;
  }
};

template <>
struct TagTraits<components::VStack> {
  using Options = components::VStackOptions;

  static constexpr auto defaults() noexcept -> Options { return Options{}; }

  template <class... Children>
    requires std::constructible_from<components::VStack, Options, Children...>
  static auto make(Options options, Children&&... children)
      -> components::VStack {
    return components::VStack(options, std::forward<Children>(children)...);
  }
};

template <>
struct TagTraits<components::HStack> {
  using Options = components::HStackOptions;

  static constexpr auto defaults() noexcept -> Options { return Options{}; }

  template <class... Children>
    requires std::constructible_from<components::HStack, Options, Children...>
  static auto make(Options options, Children&&... children)
      -> components::HStack {
    return components::HStack(options, std::forward<Children>(children)...);
  }
};

template <>
struct TagTraits<components::ZStack> {
  using Options = components::ZStackOptions;

  static constexpr auto defaults() noexcept -> Options { return Options{}; }

  template <class... Children>
    requires std::constructible_from<components::ZStack, Options, Children...>
  static auto make(Options options, Children&&... children)
      -> components::ZStack {
    return components::ZStack(options, std::forward<Children>(children)...);
  }
};

template <>
struct TagTraits<components::Grid> {
  using Options = components::GridOptions;

  static constexpr auto defaults() noexcept -> Options { return Options{}; }

  template <class... Items>
    requires std::constructible_from<components::Grid, Options, Items...>
  static auto make(Options options, Items&&... items) -> components::Grid {
    return components::Grid(options, std::forward<Items>(items)...);
  }
};

template <>
struct TagTraits<components::GridRow> {
  using Options = components::GridRowOptions;

  static constexpr auto defaults() noexcept -> Options { return Options{}; }

  template <class... Cells>
    requires std::constructible_from<components::GridRow, Options, Cells...>
  static auto make(Options options, Cells&&... cells) -> components::GridRow {
    return components::GridRow(options, std::forward<Cells>(cells)...);
  }
};

template <>
struct TagTraits<components::LazyVGrid> {
  using Options = components::LazyVGridOptions;

  static auto defaults() -> Options { return Options{}; }

  template <class... Cells>
    requires std::constructible_from<components::LazyVGrid, Options, Cells...>
  static auto make(Options options, Cells&&... cells) -> components::LazyVGrid {
    return components::LazyVGrid(std::move(options),
                                 std::forward<Cells>(cells)...);
  }
};

template <>
struct TagTraits<components::LazyHGrid> {
  using Options = components::LazyHGridOptions;

  static auto defaults() -> Options { return Options{}; }

  template <class... Cells>
    requires std::constructible_from<components::LazyHGrid, Options, Cells...>
  static auto make(Options options, Cells&&... cells) -> components::LazyHGrid {
    return components::LazyHGrid(std::move(options),
                                 std::forward<Cells>(cells)...);
  }
};

template <>
struct TagTraits<components::Text> {
  using Options = attr::TextOptions;

  static constexpr auto defaults() noexcept -> Options { return Options{}; }

  template <class Label>
    requires std::constructible_from<std::string, Label>
  static auto make(Options options, Label&& label) -> components::Text {
    std::string text{std::forward<Label>(label)};
    components::Text written =
        components::Text(std::move(text)).font(options.font);
    if (options.weight.has_value()) {
      return std::move(written).font_weight(*options.weight);
    }
    return written;
  }
};

template <>
struct TagTraits<components::Button> {
  using Options = components::Button::Options;

  static auto defaults() noexcept -> Options { return Options{}; }

  template <class Label>
    requires std::constructible_from<std::string, Label>
  static auto make(Options options, Label&& label) -> components::Button {
    std::string text{std::forward<Label>(label)};
    return components::Button(std::move(text), options.role,
                              std::move(options.action));
  }

  template <class Label>
    requires(!std::constructible_from<std::string, Label> &&
             std::convertible_to<Label, composition::Element>)
  static auto make(Options options, Label&& label) -> components::Button {
    composition::Element content{std::forward<Label>(label)};
    return components::Button(options.role, std::move(options.action),
                              std::move(content));
  }
};

template <>
struct TagTraits<components::Group> {
  using Options = std::monostate;

  static constexpr auto defaults() noexcept -> Options { return Options{}; }

  template <class... Children>
    requires std::constructible_from<components::Group, Children...>
  static auto make(Options, Children&&... children) -> components::Group {
    components::Group group(std::forward<Children>(children)...);
    return group;
  }
};

template <>
struct TagTraits<presentation::NavigationStack> {
  using Options = std::monostate;

  static constexpr auto defaults() noexcept -> Options { return Options{}; }

  template <class... Arguments>
    requires std::constructible_from<presentation::NavigationStack,
                                     Arguments...>
  static auto make(Options, Arguments&&... arguments)
      -> presentation::NavigationStack {
    presentation::NavigationStack stack(std::forward<Arguments>(arguments)...);
    return stack;
  }
};

template <>
struct TagTraits<presentation::NavigationLink> {
  using Options = std::monostate;

  static constexpr auto defaults() noexcept -> Options { return Options{}; }

  template <class... Arguments>
    requires std::constructible_from<presentation::NavigationLink, Arguments...>
  static auto make(Options, Arguments&&... arguments)
      -> presentation::NavigationLink {
    presentation::NavigationLink link(std::forward<Arguments>(arguments)...);
    return link;
  }
};

template <>
struct TagTraits<components::Spacer> {
  using Options = std::monostate;

  static constexpr auto defaults() noexcept -> Options { return Options{}; }

  template <class... Arguments>
    requires std::constructible_from<components::Spacer, Arguments...>
  static auto make(Options, Arguments&&... arguments) -> components::Spacer {
    components::Spacer spacer(std::forward<Arguments>(arguments)...);
    return spacer;
  }
};

template <>
struct TagTraits<components::ForEach> {
  using Options = std::monostate;

  static constexpr auto defaults() noexcept -> Options { return Options{}; }

  template <class... Source>
    requires std::constructible_from<components::ForEach, Source...>
  static auto make(Options, Source&&... source) -> components::ForEach {
    components::ForEach each(std::forward<Source>(source)...);
    return each;
  }
};

template <>
struct TagTraits<components::LazyVStack> {
  using Options = components::LazyVStackOptions;

  static constexpr auto defaults() noexcept -> Options { return Options{}; }

  template <class... Each>
    requires std::constructible_from<components::LazyVStack, Options, Each...>
  static auto make(Options options, Each&&... each) -> components::LazyVStack {
    return components::LazyVStack(options, std::forward<Each>(each)...);
  }
};

template <>
struct TagTraits<components::LazyHStack> {
  using Options = components::LazyHStackOptions;

  static constexpr auto defaults() noexcept -> Options { return Options{}; }

  template <class... Each>
    requires std::constructible_from<components::LazyHStack, Options, Each...>
  static auto make(Options options, Each&&... each) -> components::LazyHStack {
    return components::LazyHStack(options, std::forward<Each>(each)...);
  }
};

template <>
struct TagTraits<components::ScrollView> {
  using Options = components::ScrollViewOptions;

  static constexpr auto defaults() noexcept -> Options { return Options{}; }

  template <class... Children>
    requires std::constructible_from<components::ScrollView, Options,
                                     Children...>
  static auto make(Options options, Children&&... children)
      -> components::ScrollView {
    return components::ScrollView(options, std::forward<Children>(children)...);
  }
};

template <>
struct TagTraits<components::ScrollViewReader> {
  using Options = std::monostate;

  static constexpr auto defaults() noexcept -> Options { return Options{}; }

  template <class Builder>
    requires std::constructible_from<components::ScrollViewReader, Builder>
  static auto make(Options, Builder&& builder) -> components::ScrollViewReader {
    return components::ScrollViewReader(std::forward<Builder>(builder));
  }
};

template <>
struct TagTraits<components::GeometryReader> {
  using Options = std::monostate;

  static constexpr auto defaults() noexcept -> Options { return Options{}; }

  template <class Builder>
    requires std::constructible_from<components::GeometryReader, Builder>
  static auto make(Options, Builder&& builder) -> components::GeometryReader {
    return components::GeometryReader(std::forward<Builder>(builder));
  }
};

template <>
struct TagTraits<components::Link> {
  using Options = std::monostate;

  static constexpr auto defaults() noexcept -> Options { return Options{}; }

  template <class Title, class Destination>
    requires std::constructible_from<std::string, Title> &&
             std::constructible_from<std::string, Destination>
  static auto make(Options, Title&& title, Destination&& destination)
      -> components::Link {
    return components::Link(
        std::string{std::forward<Title>(title)},
        std::string{std::forward<Destination>(destination)});
  }
};

}
