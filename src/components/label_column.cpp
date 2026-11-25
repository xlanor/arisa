#include <arisa/components/label_column.hpp>

namespace arisa::components {

const composition::EnvironmentKey<std::optional<LabelColumn>> LabelColumn::key{
    "label_column", std::nullopt};

}
