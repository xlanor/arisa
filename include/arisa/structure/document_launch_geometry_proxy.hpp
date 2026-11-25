#pragma once

#include <arisa/composition/rect.hpp>

namespace arisa::structure {

struct DocumentLaunchGeometryProxy {
  composition::Rect frame{};
  composition::Rect title_view_frame{};

  friend auto operator==(const DocumentLaunchGeometryProxy&,
                         const DocumentLaunchGeometryProxy&) -> bool = default;
};

}
