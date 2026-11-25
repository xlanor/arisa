#pragma once

#include <arisa/composition/element.hpp>
#include <arisa/structure/list.hpp>

#include <concepts>
#include <utility>
#include <vector>

namespace arisa::structure {

class Form : public List {
 public:
  template <class... Children>
    requires(composition::ViewContent<Children> && ...)
  explicit Form(Children&&... children)
      : Form(collect(std::forward<Children>(children)...)) {}

  explicit Form(std::vector<composition::Element> children);
};

}
