#include <arisa/composition/preferred_color_scheme.hpp>

namespace arisa::composition {

const PreferenceKey<std::optional<theme::ColorScheme>>
    PreferredColorScheme::key{"preferred-color-scheme", std::nullopt,
                              [](std::optional<theme::ColorScheme>& value,
                                 std::optional<theme::ColorScheme> next) {
                                if (!value.has_value()) {
                                  value = next;
                                }
                              }};

}
