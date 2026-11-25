#include <arisa/components/canvas.hpp>

#include <arisa/components/environment.hpp>
#include <arisa/components/geometry_reader.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/text_layout_key.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/geometry_proxy.hpp>
#include <arisa/composition/preference_observer.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>

namespace arisa::components {

const composition::StateKey<std::vector<composition::Size>>
    Canvas::Content::measured{"canvas-symbols", {}};

const composition::StateKey<std::vector<GraphicsContext::TextMeasure>>
    Canvas::Content::texts{"canvas-texts", {}};

Canvas::Canvas(Renderer renderer, bool opaque)
    : Canvas(std::move(renderer), opaque, Symbols{}) {}

Canvas::Canvas(Renderer renderer, bool opaque, Symbols symbols)
    : Element(std::make_unique<Content>(std::move(renderer), opaque,
                                        std::move(symbols))) {}

Canvas::Canvas(bool opaque, bool renders_asynchronously, Renderer renderer,
               Symbols symbols)
    : Element(std::make_unique<Content>(std::move(renderer), opaque,
                                        std::move(symbols),
                                        renders_asynchronously)) {}

auto Canvas::Content::probes(
    const GraphicsContext::TextLedger& ledger,
    composition::StateHandle<std::vector<GraphicsContext::TextMeasure>> known)
    -> std::vector<composition::Element> {
  std::vector<composition::Element> made;
  for (const GraphicsContext::TextLedger::Request& request :
       ledger.requests()) {
    composition::Element text = request.make();
    if (std::isfinite(request.proposal.width)) {
      text = std::move(text)
                 .frame({.width = request.proposal.width,
                         .alignment = composition::Alignment::top_leading})
                 .fixed_size(false, true);
    } else {
      text = std::move(text).fixed_size();
    }
    made.push_back(std::move(text).hidden().on_preference_change(
        Text::LayoutKey::key,
        [known, index = request.text,
         proposal = request.proposal](const Text::LayoutKey::Value& value) {
          if (value.empty()) {
            return;
          }
          const Text::Layout& layout = value.front().layout;
          GraphicsContext::TextMetrics metrics{.size = layout.extent()};
          if (!layout.empty()) {
            metrics.first_baseline = layout[0].origin().y;
            metrics.last_baseline = layout[layout.size() - 1].origin().y;
          }
          std::vector<GraphicsContext::TextMeasure> next = known.get();
          const auto held = std::ranges::find_if(
              next, [&](const GraphicsContext::TextMeasure& each) {
                return each.text == index && each.proposal == proposal;
              });
          if (held != next.end()) {
            if (held->metrics == metrics) {
              return;
            }
            held->metrics = metrics;
          } else {
            next.push_back(GraphicsContext::TextMeasure{
                .text = index, .proposal = proposal, .metrics = metrics});
          }
          known.set(std::move(next));
        }));
  }
  return made;
}

auto Canvas::rendered(const Renderer& renderer, composition::Size size,
                      bool opaque, GraphicsContext::Resources resources)
    -> composition::Element {
  GraphicsContext context{size, std::move(resources)};
  if (renderer) {
    renderer(context, size);
  }
  composition::Element drawn = std::move(context).finish();
  if (opaque) {
    return std::move(drawn).drawing_group(true);
  }
  return drawn;
}

auto Canvas::Content::type() const noexcept -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

auto Canvas::Content::body(composition::Context& context) const
    -> std::vector<composition::Element> {
  const auto measures = context.state(texts);
  GraphicsContext::Resources resources{
      .library = context.environment(Environment::image_library),
      .theme = std::make_shared<const theme::Theme>(
          context.environment(Environment::theme)),
      .locale = context.environment(Environment::locale),
      .symbols = {},
      .texts = nullptr,
      .environment = context.environment_snapshot()};
  std::vector<composition::Element> measuring;
  if (symbols_) {
    const auto sizes = context.state(measured);
    const std::vector<composition::Size>& known = sizes.get();
    std::vector<composition::Element> members = Group::members(symbols_());
    for (std::size_t index = 0; index < members.size(); ++index) {
      composition::Element& member = members[index];
      resources.symbols.push_back(GraphicsContext::Symbol{
          .tag = member.tag(),
          .make =
              [build = symbols_, index] {
                std::vector<composition::Element> fresh =
                    Group::members(build());
                if (index >= fresh.size()) {
                  return composition::Element{Group()};
                }
                return std::move(fresh[index]);
              },
          .size = index < known.size() ? known[index] : composition::Size{}});
      measuring.push_back(
          std::move(member).fixed_size().hidden().on_geometry_change(
              std::type_identity<composition::Size>{},
              [](const composition::GeometryProxy& proxy) {
                return proxy.size();
              },
              [sizes, index](const composition::Size& extent) {
                std::vector<composition::Size> next = sizes.get();
                if (next.size() <= index) {
                  next.resize(index + 1);
                }
                if (next[index] == extent) {
                  return;
                }
                next[index] = extent;
                sizes.set(std::move(next));
              }));
    }
  }
  composition::Element canvas = GeometryReader(
      [renderer = renderer_, opaque = opaque_, resources = std::move(resources),
       measures](const composition::GeometryProxy& proxy) {
        GraphicsContext::Resources fresh = resources;
        fresh.texts =
            std::make_shared<GraphicsContext::TextLedger>(measures.get());
        composition::Element drawn =
            rendered(renderer, proxy.size(), opaque, fresh);
        std::vector<composition::Element> hidden =
            probes(*fresh.texts, measures);
        if (hidden.empty()) {
          return drawn;
        }
        return std::move(drawn).background(
            composition::Alignment::top_leading,
            ZStack(
                ZStackOptions{.alignment = composition::Alignment::top_leading},
                std::move(hidden))
                .allows_hit_testing(false));
      });
  std::vector<composition::Element> produced;
  if (measuring.empty()) {
    produced.push_back(std::move(canvas));
    return produced;
  }
  produced.push_back(std::move(canvas).background(
      composition::Alignment::top_leading,
      ZStack(ZStackOptions{.alignment = composition::Alignment::top_leading},
             std::move(measuring))
          .allows_hit_testing(false)));
  return produced;
}

}
