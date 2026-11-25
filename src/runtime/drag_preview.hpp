#pragma once

#include <arisa/composition/element.hpp>
#include <arisa/composition/tree.hpp>

#include <interaction/drag_session.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>

namespace arisa::runtime {

class DragPreview {
 public:
  static constexpr std::uint64_t overlay =
      std::numeric_limits<std::uint64_t>::max();
  static constexpr float lifted_opacity = 0.9F;
  static constexpr float outline = 2.0F;
  static constexpr std::size_t max_copies = 3;
  static constexpr float copy_offset = 6.0F;

  void present(composition::Tree& tree,
               const std::optional<interaction::DragSession::View>& view);

 private:
  static auto preview(const composition::Tree& tree,
                      const interaction::DragSession::View& view)
      -> composition::Element;

  static auto formed(const composition::Tree& tree,
                     const interaction::DragSession::View& view)
      -> composition::Element;

  std::optional<std::uint64_t> shown_;
};

}
