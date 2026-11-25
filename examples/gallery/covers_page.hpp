#pragma once

#include <arisa/async/task.hpp>
#include <arisa/components/image_source.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

namespace arisa::examples {

class CoversPage final : public composition::Component {
 public:
  static constexpr int cover_count = 500;
  static constexpr std::uint32_t cover_width = 120;
  static constexpr std::uint32_t cover_height = 180;
  static constexpr float cover_minimum = 150.0F;
  static constexpr std::string_view covers_view = "covers-scroll";
  static constexpr std::string_view close_button = "covers-close";
  static constexpr std::string_view skeleton_button = "covers-skeleton";
  static constexpr std::string_view empty_button = "covers-empty";
  static constexpr std::string_view failed_button = "covers-failed";
  static constexpr std::string_view cold_button = "covers-cold";
  static constexpr std::string_view banner_path =
      "arisa/images/london_1280.jpg";
  static constexpr composition::StateKey<bool> skeleton_key{"covers-skeleton",
                                                            false};
  static constexpr composition::StateKey<bool> empty_key{"covers-empty", false};
  static constexpr composition::StateKey<bool> failed_key{"covers-failed",
                                                          false};
  static constexpr composition::StateKey<int> generation_key{
      "covers-generation", 0};

  explicit CoversPage(composition::StateHandle<bool> shown) noexcept
      : shown_(std::move(shown)) {}

  auto operator==(const CoversPage& other) const -> bool {
    return shown_.identity() == other.shown_.identity();
  }

  auto body(composition::Context& context) const
      -> composition::Element override;

  static auto encode(int index) -> std::vector<std::byte>;

  static auto source(int index, bool failed, int generation)
      -> components::ImageSource;

 private:
  struct Cell {
    bool failed{false};
    int generation{0};

    auto operator()(int index) const -> composition::Element;

    friend auto operator==(const Cell&, const Cell&) -> bool = default;
  };

  static auto cover(int index, bool failed, int generation)
      -> composition::Element;

  static auto serve(int index, bool failed)
      -> async::Task<components::ImageSource::Bytes>;

  static void append(void* context, void* data, int size);

  composition::StateHandle<bool> shown_;
};

}
