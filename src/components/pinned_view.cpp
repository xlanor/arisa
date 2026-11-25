#include <arisa/components/hstack.hpp>
#include <arisa/components/pinned_view.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/composition/scroll_effects.hpp>

#include <utility>
#include <vector>

namespace arisa::components {

PinnedView::PinnedView(composition::Element content, composition::Axis axis,
                       bool trailing)
    : Element(
          axis == composition::Axis::vertical
              ? std::unique_ptr<composition::ElementContent>{std::make_unique<
                    VStack::Content>(VStackOptions{})}
              : std::unique_ptr<composition::ElementContent>{std::make_unique<
                    HStack::Content>(HStackOptions{})},
          [&content] {
            std::vector<composition::Element> children;
            children.push_back(std::move(content));
            return children;
          }()) {
  declared().effects.pin =
      composition::ScrollEffects::Pin{.axis = axis, .trailing = trailing};
  if (axis == composition::Axis::vertical) {
    edit_modifiers().frame.max_width = composition::infinity;
  } else {
    edit_modifiers().frame.max_height = composition::infinity;
  }
}

}
