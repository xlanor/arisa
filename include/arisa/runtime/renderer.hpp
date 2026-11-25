#pragma once

#include <arisa/composition/node.hpp>

namespace arisa::runtime {

class Renderer {
 public:
  virtual ~Renderer() = default;

  virtual void render(const composition::Node& root) = 0;
};

}
