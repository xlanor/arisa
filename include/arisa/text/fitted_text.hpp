#pragma once

#include <arisa/text/text_style.hpp>

#include <string>

namespace arisa::text {

struct FittedText {
  std::string text;
  TextStyle style;
};

}
