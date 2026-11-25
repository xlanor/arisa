#pragma once

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>

#include <string_view>
#include <utility>

namespace arisa::examples {

class LibraryPage final : public composition::Component {
 public:
  static constexpr int cover_count = 5000;
  static constexpr float cover_minimum = 180.0F;
  static constexpr std::string_view library_view = "library-scroll";
  static constexpr std::string_view reload_button = "library-reload";
  static constexpr std::string_view close_button = "library-close";
  static constexpr std::string_view sections_button = "library-sections";
  static constexpr int section_count = 26;
  static constexpr composition::StateKey<int> loads_key{"library-loads", 0};
  static constexpr composition::StateKey<bool> sectioned_key{
      "library-sectioned", false};

  explicit LibraryPage(composition::StateHandle<bool> shown) noexcept
      : shown_(std::move(shown)) {}

  auto operator==(const LibraryPage& other) const -> bool {
    return shown_.identity() == other.shown_.identity();
  }

  auto body(composition::Context& context) const
      -> composition::Element override;

  static auto cover_key(int index) -> int { return index; }

 private:
  static auto cover(int index, int load) -> composition::Element;

  static auto grid(int load, bool sectioned) -> composition::Element;

  composition::StateHandle<bool> shown_;
};

}
