#pragma once

#include <arisa/composition/axis.hpp>
#include <arisa/composition/element.hpp>

namespace arisa::components {

class PinnedView : public composition::Element {
 public:
  PinnedView(composition::Element content, composition::Axis axis,
             bool trailing);
};

}
