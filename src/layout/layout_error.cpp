#include <arisa/layout/layout_error.hpp>

namespace arisa::layout {

auto LayoutError::description() const noexcept -> std::string_view {
  switch (code_) {
    case LayoutErrorCode::engine_failure:
      return "the layout engine could not allocate a node or its configuration";
  }
  return "unknown layout error";
}

}
