#pragma once

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/provider/gpu/image.hpp>

#include <cstdint>
#include <memory>

#include <string_view>

namespace arisa::examples {

class TypographyPage final : public composition::Component {
 public:
  static constexpr composition::StateKey<int> local_size{"typography-size", 3};
  static constexpr composition::StateKey<float> local_scale{"typography-scale",
                                                            1.0F};
  static constexpr composition::StateKey<int> local_count{"typography-count",
                                                          8};
  static constexpr std::string_view marquee_id = "typography.marquee";
  static constexpr std::string_view alignment_id = "typography.alignment";
  static constexpr std::string_view reserved_id = "typography.reserved";
  static constexpr std::string_view shrinking_id = "typography.shrinking";
  static constexpr std::string_view counter_id = "typography.counter";
  static constexpr std::string_view covers_id = "typography.covers";
  static constexpr float cover_width = 72.0F;
  static constexpr float cover_height = 96.0F;

  auto operator==(const TypographyPage&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;

 private:
  static auto samples() -> composition::Element;

  static auto card(composition::Element content) -> composition::Element;

  static auto modifiers(composition::StateHandle<int> count)
      -> composition::Element;

  static auto truncations() -> composition::Element;

  static auto covers() -> composition::Element;

  static auto cover(std::uint32_t width, std::uint32_t height)
      -> std::shared_ptr<const provider::Image>;
};

}
