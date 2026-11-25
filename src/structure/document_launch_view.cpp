#include <arisa/structure/document_launch_view.hpp>

#include <arisa/components/environment.hpp>
#include <arisa/components/geometry_reader.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/coordinate_space.hpp>
#include <arisa/composition/dismiss_action.hpp>
#include <arisa/composition/geometry_proxy.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/structure/file_browser.hpp>
#include <arisa/text/localized_string_key.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <memory>
#include <utility>

namespace arisa::structure {

using composition::Element;

const composition::EnvironmentKey<bool> DocumentLaunchView::creates_documents{
    "arisa.document-launch.creates", false};

DocumentLaunchView::DocumentLaunchView(
    std::string title, std::vector<composition::UTType> content_types,
    Build actions, OnDocumentOpen on_document_open, Options options)
    : Element(std::make_unique<Content>(
          Content::Parts{.title = std::move(title),
                         .content_types = std::move(content_types),
                         .actions = std::move(actions),
                         .on_document_open = std::move(on_document_open),
                         .options = std::move(options)})) {}

DocumentLaunchView::DocumentLaunchView(
    std::string title, std::vector<composition::UTType> content_types,
    Build actions, OnDocumentOpen on_document_open)
    : DocumentLaunchView(std::move(title), std::move(content_types),
                         std::move(actions), std::move(on_document_open),
                         Options{}) {}

DocumentLaunchView::DocumentLaunchView(
    std::string title, std::vector<composition::UTType> content_types,
    graphics::ShapeStyle background_style, Build actions,
    OnDocumentOpen on_document_open)
    : DocumentLaunchView(std::move(title), std::move(content_types),
                         std::move(background_style), std::move(actions),
                         std::move(on_document_open), Options{}) {}

DocumentLaunchView::DocumentLaunchView(
    std::string title, std::vector<composition::UTType> content_types,
    graphics::ShapeStyle background_style, Build actions,
    OnDocumentOpen on_document_open, Options options)
    : DocumentLaunchView(
          std::move(title), std::move(content_types), std::move(actions),
          std::move(on_document_open), [&options, &background_style] {
            Options styled = std::move(options);
            styled.background_style = std::move(background_style);
            return styled;
          }()) {}

auto DocumentLaunchView::title_key() -> composition::ElementKey {
  return composition::ElementKey{std::string{"arisa.document-launch.title"}};
}

auto DocumentLaunchView::browser_key() -> composition::ElementKey {
  return composition::ElementKey{std::string{"arisa.document-launch.browser"}};
}

auto DocumentLaunchView::document_key() -> composition::ElementKey {
  return composition::ElementKey{std::string{"arisa.document-launch.document"}};
}

auto DocumentLaunchView::Content::type() const noexcept
    -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

auto DocumentLaunchView::Content::opening(composition::Context& context) const
    -> components::NewDocumentAction {
  const auto shown = context.state(opened);
  const auto failed = context.state(failure);
  return components::NewDocumentAction{
      [shown, failed, create = parts_.options.create](
          components::NewDocumentAction::Request request) {
        if (create) {
          auto made = create(std::move(request));
          if (!made.has_value()) {
            failed.set(made.error().explanation());
            return;
          }
          failed.set(std::string{});
          shown.set(std::string{made->path()});
          return;
        }
        const std::optional<composition::FileURL> prepared =
            request.prepare ? request.prepare() : std::nullopt;
        if (!prepared.has_value()) {
          failed.set(components::DocumentError{
              components::DocumentError::Stage::create,
              std::string{"no document to create"}}
                         .explanation());
          return;
        }
        failed.set(std::string{});
        shown.set(std::string{prepared->path()});
      }};
}

auto DocumentLaunchView::Content::launch(composition::Context& context) const
    -> Element {
  const auto shown = context.state(opened);
  const auto titled = context.state(title_frame);
  const std::string said = context.state(failure).get();
  std::vector<Element> column;
  column.push_back(
      components::Text(text::LocalizedStringKey{parts_.title})
          .font(theme::Font::large_title)
          .on_geometry_change(
              std::type_identity<composition::Rect>{},
              [](const composition::GeometryProxy& proxy) {
                return proxy.frame(composition::CoordinateSpace::global);
              },
              [titled](const composition::Rect& frame) { titled.set(frame); })
          .id(title_key()));
  if (parts_.options.subtitle.has_value()) {
    column.push_back(
        components::Text(text::LocalizedStringKey{*parts_.options.subtitle})
            .font(theme::Font::title3)
            .foreground_color(theme::ColourRole::text_secondary));
  }
  if (parts_.actions) {
    column.push_back(parts_.actions());
  }
  if (!said.empty()) {
    column.push_back(components::Text(said).foreground_color(
        theme::ColourRole::destructive));
  }
  FileBrowser::Request request{.mode = FileBrowser::Mode::import,
                               .allowed = parts_.content_types};
  column.push_back(Element{
      FileBrowser{std::move(request),
                  composition::Binding<bool>::constant(true),
                  FileBrowser::Completion{[shown](FileBrowser::Result result) {
                    if (result.has_value()) {
                      shown.set(std::string{result->path()});
                    }
                  }}}}
                       .frame({.max_width = composition::infinity,
                               .max_height = composition::infinity})
                       .id(browser_key()));
  Element body = components::VStack(
                     components::VStackOptions{
                         .alignment = components::HorizontalAlignment::center,
                         .spacing = theme::space::lg},
                     std::move(column))
                     .padding(theme::space::xl)
                     .frame({.max_width = composition::infinity,
                             .max_height = composition::infinity});
  const composition::Rect title = titled.get();
  const Accessory under = parts_.options.background_accessory;
  const Accessory over = parts_.options.overlay_accessory;
  const Build backdrop = parts_.options.background;
  const std::optional<graphics::ShapeStyle> style =
      parts_.options.background_style;
  std::vector<Element> layers;
  if (backdrop) {
    layers.push_back(backdrop().frame({.max_width = composition::infinity,
                                       .max_height = composition::infinity}));
  } else if (style.has_value()) {
    layers.push_back(components::ZStack()
                         .frame({.max_width = composition::infinity,
                                 .max_height = composition::infinity})
                         .background(*style));
  }
  if (under || over) {
    const auto accessory = [title](const Accessory& build) {
      return Element{components::GeometryReader(
          [build, title](const composition::GeometryProxy& proxy) {
            return build(DocumentLaunchGeometryProxy{
                .frame = proxy.frame(composition::CoordinateSpace::global),
                .title_view_frame = title});
          })};
    };
    if (under) {
      layers.push_back(accessory(under));
    }
    layers.push_back(std::move(body));
    if (over) {
      layers.push_back(accessory(over));
    }
  } else {
    layers.push_back(std::move(body));
  }
  return components::ZStack(components::ZStackOptions{}, std::move(layers));
}

auto DocumentLaunchView::Content::body(composition::Context& context) const
    -> std::vector<Element> {
  const auto shown = context.state(opened);
  const components::NewDocumentAction creating = opening(context);
  const components::OpenDocumentAction opener{
      [shown](const composition::FileURL& url)
          -> std::expected<void, components::DocumentError> {
        shown.set(std::string{url.path()});
        return {};
      }};
  Element presented = components::ZStack();
  const std::string& path = shown.get();
  const std::optional<composition::FileURL> open =
      path.empty() ? std::nullopt : composition::FileURL::create(path);
  if (open.has_value() && parts_.on_document_open) {
    presented = parts_.on_document_open(*open)
                    .environment(components::Environment::dismiss,
                                 composition::DismissAction{
                                     composition::Identity{&opened}, 0,
                                     [shown] { shown.set(std::string{}); }})
                    .frame({.max_width = composition::infinity,
                            .max_height = composition::infinity})
                    .id(document_key());
  } else {
    presented = launch(context);
  }
  std::vector<Element> produced;
  produced.push_back(
      std::move(presented)
          .environment(components::Environment::new_document, creating)
          .environment(components::Environment::open_document, opener));
  return produced;
}

}
