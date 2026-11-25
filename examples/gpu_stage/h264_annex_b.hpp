#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace arisa::examples {

class H264AnnexB {
 public:
  enum class NalType : std::uint8_t {
    slice = 1,
    idr = 5,
    sei = 6,
    sps = 7,
    pps = 8,
    delimiter = 9,
  };

  static constexpr std::size_t start_code_size = 4;

  [[nodiscard]] static auto parse(std::span<const std::byte> stream)
      -> std::expected<H264AnnexB, std::string>;

  [[nodiscard]] static auto load(const std::filesystem::path& path)
      -> std::expected<H264AnnexB, std::string>;

  auto parameter_sets() const noexcept -> std::span<const std::byte> {
    return parameter_sets_;
  }

  auto access_units() const noexcept -> std::size_t { return units_.size(); }

  auto access_unit(std::size_t index) const noexcept
      -> std::span<const std::byte>;

  auto keyframe(std::size_t index) const noexcept -> bool;

  static auto nal_type(std::span<const std::byte> nal) noexcept -> std::uint8_t;

  static auto nal_units(std::span<const std::byte> stream)
      -> std::vector<std::span<const std::byte>>;

 private:
  struct Unit {
    std::size_t offset{0};
    std::size_t size{0};
    bool keyframe{false};
  };

  H264AnnexB(std::vector<std::byte> parameter_sets,
             std::vector<std::byte> samples, std::vector<Unit> units) noexcept;

  static auto starts_picture(std::span<const std::byte> nal) noexcept -> bool;

  static auto vcl(std::uint8_t type) noexcept -> bool;

  static auto opens_unit(std::uint8_t type) noexcept -> bool;

  static void append(std::vector<std::byte>& bytes,
                     std::span<const std::byte> nal);

  std::vector<std::byte> parameter_sets_;
  std::vector<std::byte> samples_;
  std::vector<Unit> units_;
};

}
