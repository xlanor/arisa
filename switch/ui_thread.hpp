#pragma once

#include <functional>

namespace arisa::nx {

class UiThread {
 public:
  static auto run(std::function<int()> body) -> int;
};

}
