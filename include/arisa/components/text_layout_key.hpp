#pragma once

#include <arisa/components/text.hpp>
#include <arisa/components/text_layout.hpp>
#include <arisa/composition/anchor.hpp>
#include <arisa/composition/point.hpp>
#include <arisa/composition/preference_key.hpp>

#include <vector>

namespace arisa::components {

class Text::LayoutKey {
 public:
  struct AnchoredLayout {
    composition::Anchor<composition::Point> origin{};
    Layout layout{};

    friend auto operator==(const AnchoredLayout&, const AnchoredLayout&)
        -> bool = default;
  };

  using Value = std::vector<AnchoredLayout>;

  static const composition::PreferenceKey<Value> key;

  static auto default_value() -> Value { return {}; }

  static void reduce(Value& value, Value next);
};

}
