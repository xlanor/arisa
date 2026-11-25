#pragma once

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>

#include <array>
#include <string>
#include <string_view>
#include <utility>

namespace arisa::examples {

class RichTextPage final : public composition::Component {
 public:
  class Link;

  static constexpr std::string_view release_notes =
      "**Version 2.4** brings *smoother* streaming, a new `--low-latency` "
      "flag and ~~beta~~ stable controller remapping. Read the "
      "[changelog](https://example.com/changelog) or the "
      "[guide](https://example.com/guide).";
  static constexpr std::array<std::string_view, 5> log_lines{
      "12:04:01.118  stream   connected 1080p60",
      "12:04:01.452  input    pad 1 bound",
      "12:04:02.007  audio    48000 Hz stereo",
      "12:04:09.913  network  rtt 18 ms",
      "12:04:11.240  stream   idr requested"};
  static constexpr std::string_view custom_face = "Bumbbled";
  static constexpr std::string_view close_button = "rich-text-close";
  static constexpr std::string_view notes_view = "rich-text-notes";
  static constexpr std::string_view log_view = "rich-text-log";
  static constexpr std::string_view price_view = "rich-text-price";
  static constexpr std::string_view joined_view = "rich-text-joined";
  static constexpr std::string_view custom_view = "rich-text-custom";
  static constexpr std::string_view faces_view = "rich-text-faces";

  explicit RichTextPage(composition::StateHandle<bool> shown) noexcept
      : shown_(std::move(shown)) {}

  auto operator==(const RichTextPage& other) const -> bool {
    return shown_.identity() == other.shown_.identity();
  }

  auto body(composition::Context& context) const
      -> composition::Element override;

  static auto face_key(std::string_view name) -> std::string;

  static auto log_key(std::size_t index) -> std::string;

 private:
  static auto section(std::string_view title, composition::Element content)
      -> composition::Element;

  static auto notes() -> composition::Element;

  static auto log() -> composition::Element;

  static auto prices() -> composition::Element;

  static auto faces() -> composition::Element;

  composition::StateHandle<bool> shown_;
};

class RichTextPage::Link final : public composition::Component {
 public:
  static constexpr composition::StateKey<bool> shown_key{"rich-text-shown",
                                                         false};
  static constexpr std::string_view button = "typography-rich-text-button";

  auto operator==(const Link&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;
};

}
