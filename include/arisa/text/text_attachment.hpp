#pragma once

namespace arisa::text {

struct TextAttachment {
  float width{0.0F};
  float height{0.0F};
  float descent{0.0F};

  friend constexpr auto operator==(const TextAttachment&, const TextAttachment&)
      -> bool = default;
};

}
