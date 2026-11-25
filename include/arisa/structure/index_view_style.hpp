#pragma once

#include <arisa/composition/element.hpp>
#include <arisa/composition/environment_key.hpp>
#include <arisa/structure/page_index_view_style.hpp>

#include <utility>

namespace arisa::structure {

class IndexViewStyle {
 public:
  using BackgroundDisplayMode = PageIndexViewStyle::BackgroundDisplayMode;

  static const composition::EnvironmentKey<IndexViewStyle> key;

  constexpr IndexViewStyle(PageIndexViewStyle style) noexcept : page_(style) {}

  static constexpr auto page(BackgroundDisplayMode background_display_mode =
                                 BackgroundDisplayMode::automatic) noexcept
      -> IndexViewStyle {
    return IndexViewStyle{PageIndexViewStyle{background_display_mode}};
  }

  constexpr auto background_display_mode() const noexcept
      -> BackgroundDisplayMode {
    return page_.background_display_mode();
  }

  constexpr auto backs(bool interacting) const noexcept -> bool {
    switch (page_.background_display_mode()) {
      case BackgroundDisplayMode::always:
        return true;
      case BackgroundDisplayMode::never:
        return false;
      case BackgroundDisplayMode::automatic:
      case BackgroundDisplayMode::interactive:
        return interacting;
    }
    return false;
  }

  friend constexpr auto operator==(const IndexViewStyle&, const IndexViewStyle&)
      -> bool = default;

 private:
  PageIndexViewStyle page_;
};

}

namespace arisa::composition {

template <class Style>
auto Element::index_view_style(Style style) && -> Element {
  return std::move(*this).environment(structure::IndexViewStyle::key,
                                      structure::IndexViewStyle{style});
}

}
