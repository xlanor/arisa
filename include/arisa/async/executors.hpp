#pragma once

#include <arisa/async/executor.hpp>

#include <functional>

namespace arisa::async {

struct Executors {
  std::reference_wrapper<Executor> background;
  std::reference_wrapper<Executor> blocking;
};

}
