#pragma once

#include <arisa/text/font_face.hpp>
#include <arisa/text/font_role.hpp>
#include <arisa/text/text_style.hpp>
#include <arisa/text/typeface.hpp>

#include <memory>
#include <string>

namespace arisa::text {

struct FontEntry {
  FontRole role{FontRole::latin};
  std::shared_ptr<const FontFace> face;
  FontDesign design{FontDesign::text};
  Typeface typeface{Typeface::standard};
  bool italic{false};
  std::string name{};
};

}
