#pragma once

#include <arisa/conformance/report.hpp>
#include <arisa/provider/factory.hpp>

#include <cstddef>
#include <span>

namespace arisa::conformance {

class FontsSuite {
 public:
  static void run(provider::ProviderFactory& factory, Report& report);

  static auto has_sfnt_tag(std::span<const std::byte> bytes) noexcept -> bool;

 private:
  static void check_standard(const provider::FontProvider& fonts,
                             Report& report);
  static void check_others(const provider::FontProvider& fonts, Report& report);
  static void check_absent(const provider::FontProvider& fonts, Report& report);
};

}
