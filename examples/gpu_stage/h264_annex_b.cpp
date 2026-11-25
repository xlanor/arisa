#include <h264_annex_b.hpp>

#include <algorithm>
#include <array>
#include <fstream>
#include <ios>
#include <optional>
#include <system_error>
#include <utility>

namespace arisa::examples {

H264AnnexB::H264AnnexB(std::vector<std::byte> parameter_sets,
                       std::vector<std::byte> samples,
                       std::vector<Unit> units) noexcept
    : parameter_sets_(std::move(parameter_sets)),
      samples_(std::move(samples)),
      units_(std::move(units)) {}

auto H264AnnexB::nal_type(std::span<const std::byte> nal) noexcept
    -> std::uint8_t {
  if (nal.empty()) {
    return 0;
  }
  return std::to_integer<std::uint8_t>(nal.front() & std::byte{0x1F});
}

auto H264AnnexB::vcl(std::uint8_t type) noexcept -> bool {
  return type >= std::to_underlying(NalType::slice) &&
         type <= std::to_underlying(NalType::idr);
}

auto H264AnnexB::opens_unit(std::uint8_t type) noexcept -> bool {
  constexpr std::uint8_t first_reserved = 14;
  constexpr std::uint8_t last_reserved = 18;
  return (type >= std::to_underlying(NalType::sei) &&
          type <= std::to_underlying(NalType::delimiter)) ||
         (type >= first_reserved && type <= last_reserved);
}

auto H264AnnexB::starts_picture(std::span<const std::byte> nal) noexcept
    -> bool {
  return nal.size() > 1 && (nal[1] & std::byte{0x80}) != std::byte{0};
}

void H264AnnexB::append(std::vector<std::byte>& bytes,
                        std::span<const std::byte> nal) {
  constexpr std::array<std::byte, start_code_size> start{
      std::byte{0}, std::byte{0}, std::byte{0}, std::byte{1}};
  bytes.insert(bytes.end(), start.begin(), start.end());
  bytes.insert(bytes.end(), nal.begin(), nal.end());
}

auto H264AnnexB::nal_units(std::span<const std::byte> stream)
    -> std::vector<std::span<const std::byte>> {
  std::vector<std::span<const std::byte>> units;
  std::optional<std::size_t> begin;
  const auto close = [&units, &begin, stream](std::size_t end) {
    if (!begin.has_value()) {
      return;
    }
    while (end > *begin && stream[end - 1] == std::byte{0}) {
      --end;
    }
    if (end > *begin) {
      units.push_back(stream.subspan(*begin, end - *begin));
    }
  };
  std::size_t at = 0;
  while (at + 2 < stream.size()) {
    if (stream[at] == std::byte{0} && stream[at + 1] == std::byte{0} &&
        stream[at + 2] == std::byte{1}) {
      close(at);
      begin = at + 3;
      at += 3;
    } else {
      ++at;
    }
  }
  close(stream.size());
  return units;
}

auto H264AnnexB::parse(std::span<const std::byte> stream)
    -> std::expected<H264AnnexB, std::string> {
  std::vector<std::byte> parameter_sets;
  std::vector<std::span<const std::byte>> known;
  bool has_sps = false;
  bool has_pps = false;
  std::vector<std::byte> samples;
  std::vector<Unit> units;
  std::optional<Unit> current;
  bool picture = false;
  const auto finish = [&samples, &units, &current, &picture] {
    if (!current.has_value()) {
      return;
    }
    if (picture) {
      units.push_back(*current);
    } else {
      samples.resize(current->offset);
    }
    current.reset();
    picture = false;
  };
  for (const std::span<const std::byte> nal : nal_units(stream)) {
    const std::uint8_t type = nal_type(nal);
    const bool parameter = type == std::to_underlying(NalType::sps) ||
                           type == std::to_underlying(NalType::pps);
    if (parameter && std::ranges::none_of(known, [nal](const auto& seen) {
          return std::ranges::equal(seen, nal);
        })) {
      known.push_back(nal);
      append(parameter_sets, nal);
      has_sps = has_sps || type == std::to_underlying(NalType::sps);
      has_pps = has_pps || type == std::to_underlying(NalType::pps);
    }
    const bool opens =
        picture && (opens_unit(type) || (vcl(type) && starts_picture(nal)));
    if (!current.has_value() || opens) {
      finish();
      current = Unit{.offset = samples.size(), .size = 0, .keyframe = false};
    }
    append(samples, nal);
    if (vcl(type)) {
      picture = true;
      current->keyframe =
          current->keyframe || type == std::to_underlying(NalType::idr);
    }
    current->size = samples.size() - current->offset;
  }
  finish();
  if (!has_sps || !has_pps) {
    return std::unexpected(
        std::string{"the stream has no sequence or picture parameter set"});
  }
  if (units.empty()) {
    return std::unexpected(std::string{"the stream has no pictures"});
  }
  return H264AnnexB{std::move(parameter_sets), std::move(samples),
                    std::move(units)};
}

auto H264AnnexB::load(const std::filesystem::path& path)
    -> std::expected<H264AnnexB, std::string> {
  std::error_code error;
  const std::uintmax_t size = std::filesystem::file_size(path, error);
  if (error) {
    return std::unexpected("the stream " + path.string() +
                           " cannot be read: " + error.message());
  }
  std::ifstream file{path, std::ios::binary};
  std::vector<std::byte> bytes(size);
  const auto length = static_cast<std::streamsize>(size);
  if (!file || !file.read(reinterpret_cast<char*>(bytes.data()), length)) {
    return std::unexpected("the stream " + path.string() + " cannot be read");
  }
  return parse(bytes);
}

auto H264AnnexB::access_unit(std::size_t index) const noexcept
    -> std::span<const std::byte> {
  if (index >= units_.size()) {
    return {};
  }
  return std::span<const std::byte>{samples_}.subspan(units_[index].offset,
                                                      units_[index].size);
}

auto H264AnnexB::keyframe(std::size_t index) const noexcept -> bool {
  return index < units_.size() && units_[index].keyframe;
}

}
