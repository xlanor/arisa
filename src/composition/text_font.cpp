#include <arisa/composition/text_font.hpp>

namespace arisa::composition {

const EnvironmentKey<std::optional<theme::Font>> TextFont::font{"font",
                                                                std::nullopt};

}
