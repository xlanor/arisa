#include <covers_page.hpp>

#include <arisa/components/async_image.hpp>
#include <arisa/components/content_unavailable_view.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/grid_item.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/composition/redaction_reasons.hpp>
#include <arisa/markup/markup.hpp>
#include <arisa/theme/radius.hpp>
#include <arisa/theme/space.hpp>

#include <stb_image_write.h>

#include <array>
#include <cmath>
#include <format>
#include <optional>
#include <ranges>
#include <span>
#include <string>

namespace arisa::examples {

using namespace markup;
using composition::Element;
using composition::infinity;
using theme::ColourRole;
using theme::RoundedCornerStyle;
using theme::RoundedRectangle;

void CoversPage::append(void* context, void* data, int size) {
  auto& bytes = *static_cast<std::vector<std::byte>*>(context);
  const std::span<const std::byte> written{static_cast<const std::byte*>(data),
                                           static_cast<std::size_t>(size)};
  bytes.insert(bytes.end(), written.begin(), written.end());
}

auto CoversPage::encode(int index) -> std::vector<std::byte> {
  constexpr std::size_t channels = 4;
  std::vector<unsigned char> pixels(std::size_t{cover_width} * cover_height *
                                    channels);
  const float hue = std::fmod(static_cast<float>(index) * 0.618034F, 1.0F);
  const std::array<float, 3> base{
      0.5F + (0.5F * std::cos(6.2831853F * hue)),
      0.5F + (0.5F * std::cos(6.2831853F * (hue - 0.3333333F))),
      0.5F + (0.5F * std::cos(6.2831853F * (hue - 0.6666667F)))};
  for (std::uint32_t y = 0; y < cover_height; ++y) {
    const float shade = 0.55F + (0.45F * static_cast<float>(y) /
                                 static_cast<float>(cover_height));
    const bool band = y > (cover_height * 2) / 3 && y < (cover_height * 5) / 6;
    for (std::uint32_t x = 0; x < cover_width; ++x) {
      const std::size_t at = ((std::size_t{y} * cover_width) + x) * channels;
      for (std::size_t channel = 0; channel < 3; ++channel) {
        const float value = band ? 0.96F : base[channel] * shade;
        pixels[at + channel] = static_cast<unsigned char>(value * 255.0F);
      }
      pixels[at + 3] = 255;
    }
  }
  std::vector<std::byte> encoded;
  stbi_write_png_to_func(
      &CoversPage::append, &encoded, static_cast<int>(cover_width),
      static_cast<int>(cover_height), static_cast<int>(channels), pixels.data(),
      static_cast<int>(cover_width * channels));
  return encoded;
}

auto CoversPage::serve(int index, bool failed)
    -> async::Task<components::ImageSource::Bytes> {
  if (failed) {
    co_return std::vector<std::byte>{std::byte{0x89}, std::byte{0x50}};
  }
  co_return encode(index);
}

auto CoversPage::source(int index, bool failed, int generation)
    -> components::ImageSource {
  return components::ImageSource::loader(
      std::format("cover-{}-{}-{}", generation, failed ? 1 : 0, index),
      [index, failed] { return serve(index, failed); });
}

auto CoversPage::cover(int index, bool failed, int generation) -> Element {
  return VStack(alignment::leading, spacing(theme::space::xs),
                frame({.max_width = infinity,
                       .alignment = composition::Alignment::leading}),
                focusable())(
      Element{
          components::AsyncImage(
              source(index, failed, generation),
              components::AsyncImageOptions{},
              [](const components::AsyncImagePhase& phase) -> Element {
                if (std::optional<components::Image> image = phase.image()) {
                  return std::move(*image).resizable().scaled_to_fill();
                }
                if (phase.error().has_value()) {
                  return ZStack(
                      frame({.max_width = infinity, .max_height = infinity}),
                      background(ColourRole::secondary_system_fill,
                                 theme::Shape::rectangle()))(
                      Image(font::title,
                            foreground_style(ColourRole::tertiary_label))(
                          system_name{"broken_image"}));
                }
                return components::AsyncImage::placeholder();
              })}
          .frame({.max_width = infinity})
          .aspect_ratio(static_cast<float>(cover_width) /
                        static_cast<float>(cover_height))
          .clip_shape(RoundedRectangle(theme::radius::card,
                                       RoundedCornerStyle::continuous)),
      Text(font::footnote, line_limit(1))(std::format("Cover {}", index + 1)));
}

auto CoversPage::Cell::operator()(int index) const -> Element {
  return cover(index, failed, generation);
}

auto CoversPage::body(composition::Context& context) const -> Element {
  const auto skeleton = context.state(skeleton_key);
  const auto empty = context.state(empty_key);
  const auto failed = context.state(failed_key);
  const auto generation = context.state(generation_key);
  const std::optional<components::ImageCache>& cache =
      context.environment(components::Environment::image_cache);
  const auto shown = shown_;
  const bool failing = failed.get();
  const int round = generation.get();
  Element content =
      empty.get() ? Element{components::ContentUnavailableView("No Covers",
                                                               "photo_library")}
                        .frame({.max_width = infinity, .max_height = infinity})
                  : Element{ScrollView(
                        id(covers_view),
                        frame({.max_width = infinity, .max_height = infinity}),
                        content_margins(theme::space::sm))(LazyVGrid(
                        columns(std::vector<GridItem>{
                            GridItem{GridItem::Size::adaptive(cover_minimum),
                                     theme::space::md}}),
                        spacing(theme::space::md), estimated_extent(250.0F),
                        overscan(120.0F))(ForEach(
                        std::views::iota(0, cover_count),
                        [](int index) { return index; },
                        Cell{.failed = failing, .generation = round})))};
  if (skeleton.get()) {
    content = std::move(content).redacted(RedactionReasons::placeholder);
  }
  return VStack(
      alignment::leading, spacing(theme::space::md), padding(theme::space::lg),
      frame({.max_width = infinity,
             .max_height = infinity,
             .alignment = composition::Alignment::leading}),
      background(ColourRole::system_background, theme::Shape::rectangle()))(
      HStack(spacing(theme::space::md))(
          Button(id(close_button),
                 action([shown] { shown.set(false); }))("Close"),
          Button(id(skeleton_button), action([skeleton] {
                   skeleton.set(!skeleton.get());
                 }))("Placeholder"),
          Button(id(empty_button),
                 action([empty] { empty.set(!empty.get()); }))("Empty"),
          Button(id(failed_button),
                 action([failed] { failed.set(!failed.get()); }))("Failed"),
          Button(id(cold_button), action([cache, generation] {
                   if (cache.has_value()) {
                     cache->clear();
                   }
                   generation.set(generation.get() + 1);
                 }))("Cold"),
          Spacer(),
          Element{components::AsyncImage(
                      components::ImageSource::asset(std::string{banner_path}),
                      components::AsyncImageOptions{},
                      [](const components::AsyncImagePhase& phase) -> Element {
                        if (std::optional<components::Image> image =
                                phase.image()) {
                          return std::move(*image).resizable().scaled_to_fill();
                        }
                        return components::AsyncImage::placeholder();
                      })}
              .frame({.width = 128.0F, .height = 72.0F})
              .clip_shape(RoundedRectangle(theme::radius::control,
                                           RoundedCornerStyle::continuous))),
      std::move(content));
}

}
