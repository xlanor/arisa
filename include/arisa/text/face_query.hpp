#pragma once

#include <arisa/text/font_name.hpp>
#include <arisa/text/font_weight.hpp>
#include <arisa/text/text_style.hpp>
#include <arisa/text/typeface.hpp>

#include <optional>

namespace arisa::text {

struct FaceQuery {
  FontWeight weight{font_weight::regular};
  FontDesign design{FontDesign::text};
  bool italic{false};
  Typeface typeface{Typeface::standard};
  float stretch{1.0F};
  std::optional<FontName> name{};

  friend auto operator==(const FaceQuery&, const FaceQuery&) -> bool = default;
};

}
