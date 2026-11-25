#include <arisa/components/text_layout_key.hpp>

#include <arisa/composition/text_environment.hpp>

#include <iterator>
#include <utility>

namespace arisa::components {

const composition::PreferenceKey<Text::LayoutKey::Value> Text::LayoutKey::key{
    "text-layout", Value{}, &Text::LayoutKey::reduce,
    composition::TextEnvironment::publishes_layout};

void Text::LayoutKey::reduce(Value& value, Value next) {
  value.insert(value.end(), std::make_move_iterator(next.begin()),
               std::make_move_iterator(next.end()));
}

}
