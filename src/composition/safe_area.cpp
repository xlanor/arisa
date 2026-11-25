#include <arisa/composition/safe_area.hpp>

namespace arisa::composition {

const EnvironmentKey<theme::Edges> SafeArea::insets{"safe_area_insets",
                                                    theme::Edges{}};

}
