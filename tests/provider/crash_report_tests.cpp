#include <arisa/provider/diagnostics/backtrace.hpp>
#include <arisa/provider/diagnostics/crash_report.hpp>
#include <arisa/provider/diagnostics/crash_reporter.hpp>
#include <arisa/provider/diagnostics/exception_dump.hpp>
#include <arisa/provider/diagnostics/loaded_modules.hpp>
#include <arisa/provider/diagnostics/memory_space.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace arisa::provider {

class SyntheticMemory final : public MemorySpace {
 public:
  void map(MemoryRegion region) {
    blocks_.push_back(
        Block{.region = region,
              .words = std::vector<std::uint64_t>(region.size / 8, 0)});
    std::ranges::sort(blocks_, {},
                      [](const Block& each) { return each.region.base; });
  }

  void poke(std::uint64_t address, std::uint64_t value) {
    for (Block& each : blocks_) {
      if (each.region.holds(address, 8)) {
        each.words[(address - each.region.base) / 8] = value;
      }
    }
  }

  void bytes(std::uint64_t address, std::string_view text) {
    for (std::size_t index = 0; index < text.size(); ++index) {
      const std::uint64_t at = address + index;
      const std::uint64_t aligned = at - at % 8;
      for (Block& each : blocks_) {
        if (each.region.holds(aligned, 8)) {
          std::uint64_t& word = each.words[(aligned - each.region.base) / 8];
          const std::uint64_t shift = (at % 8) * 8;
          word &= ~(std::uint64_t{0xFF} << shift);
          word |= std::uint64_t{static_cast<unsigned char>(text[index])}
                  << shift;
        }
      }
    }
  }

  auto region(std::uint64_t address) const noexcept
      -> std::optional<MemoryRegion> override {
    std::uint64_t gap = 0;
    for (const Block& each : blocks_) {
      if (address < each.region.base) {
        return MemoryRegion{.base = gap, .size = each.region.base - gap};
      }
      if (address < each.region.end()) {
        return each.region;
      }
      gap = each.region.end();
    }
    return MemoryRegion{
        .base = gap,
        .size = std::numeric_limits<std::uint64_t>::max() - gap + 1};
  }

 private:
  struct Block {
    MemoryRegion region;
    std::vector<std::uint64_t> words;
  };

  auto fetch(std::uint64_t address) const noexcept -> std::uint64_t override {
    for (const Block& each : blocks_) {
      if (each.region.holds(address, 8)) {
        return each.words[(address - each.region.base) / 8];
      }
    }
    ADD_FAILURE() << "fetch outside a mapped block at " << address;
    return 0;
  }

  std::vector<Block> blocks_;
};

class CrashReportTest : public ::testing::Test {
 protected:
  static constexpr std::uint64_t text = 0x8000000;
  static constexpr std::uint64_t rodata = 0x8004000;
  static constexpr std::uint64_t data = 0x8006000;
  static constexpr std::uint64_t stack = 0x10000;
  static constexpr std::array<std::uint8_t, 20> build_id{
      0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
      0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10};

  void SetUp() override {
    memory_.map({.base = text,
                 .size = 0x4000,
                 .readable = true,
                 .executable = true,
                 .module_image = true});
    memory_.map({.base = rodata,
                 .size = 0x2000,
                 .readable = true,
                 .module_image = true});
    memory_.map({.base = data,
                 .size = 0x1000,
                 .readable = true,
                 .writable = true,
                 .module_image = true});
    memory_.map(
        {.base = stack, .size = 0x1000, .readable = true, .writable = true});
    constexpr std::string_view path = "sdmc:/switch/gallery.nro";
    memory_.poke(rodata, std::uint64_t{path.size()} << 32U);
    memory_.bytes(rodata + 8, path);
    const std::uint64_t note = rodata + 0x2000 - 0x40;
    memory_.poke(note, (std::uint64_t{20} << 32U) | 4U);
    memory_.poke(note + 8, (std::uint64_t{LoadedModules::gnu_name} << 32U) |
                               LoadedModules::build_id_note);
    memory_.bytes(note + 16, std::string_view{
                                 reinterpret_cast<const char*>(build_id.data()),
                                 build_id.size()});
  }

  void chain(std::uint64_t fp, std::uint64_t caller, std::uint64_t returned) {
    memory_.poke(fp, caller);
    memory_.poke(fp + 8, returned);
  }

  SyntheticMemory memory_;
};

TEST_F(CrashReportTest, WalksAFramePointerChain) {
  chain(stack + 0x100, stack + 0x200, text + 0x100);
  chain(stack + 0x200, stack + 0x300, text + 0x200);
  chain(stack + 0x300, 0, text + 0x300);
  const Backtrace trace = Backtrace::walk(memory_, text + 0x50, stack + 0x100);
  const std::vector<std::uint64_t> frames{trace.frames().begin(),
                                          trace.frames().end()};
  EXPECT_EQ(frames, (std::vector<std::uint64_t>{text + 0x50, text + 0x100,
                                                text + 0x200, text + 0x300}));
}

TEST_F(CrashReportTest, AbortWalkStartsAtTheCallerRecord) {
  chain(stack + 0x100, stack + 0x200, text + 0x100);
  chain(stack + 0x200, 0, text + 0x200);
  const Backtrace trace = Backtrace::walk(memory_, std::nullopt, stack + 0x100);
  const std::vector<std::uint64_t> frames{trace.frames().begin(),
                                          trace.frames().end()};
  EXPECT_EQ(frames, (std::vector<std::uint64_t>{text + 0x100, text + 0x200}));
}

TEST_F(CrashReportTest, StopsOnAFramePointerIntoUnmappedMemory) {
  chain(stack + 0x100, stack + 0x200, text + 0x100);
  chain(stack + 0x200, 0xDEAD'0000, text + 0x200);
  const Backtrace trace = Backtrace::walk(memory_, text + 0x50, stack + 0x100);
  EXPECT_EQ(trace.frames().size(), 3U);
  EXPECT_EQ(trace.frames().back(), text + 0x200);
}

TEST_F(CrashReportTest, StopsOnALoopingChain) {
  chain(stack + 0x100, stack + 0x200, text + 0x100);
  chain(stack + 0x200, stack + 0x100, text + 0x200);
  const Backtrace trace = Backtrace::walk(memory_, text + 0x50, stack + 0x100);
  EXPECT_EQ(trace.frames().size(), 3U);
}

TEST_F(CrashReportTest, StopsOnAMisalignedOrReadOnlyFramePointer) {
  chain(stack + 0x100, stack + 0x203, text + 0x100);
  EXPECT_EQ(Backtrace::walk(memory_, text, stack + 0x100).frames().size(), 2U);
  chain(stack + 0x100, rodata + 0x100, text + 0x100);
  EXPECT_EQ(Backtrace::walk(memory_, text, stack + 0x100).frames().size(), 2U);
  EXPECT_EQ(Backtrace::walk(memory_, text, text + 0x100).frames().size(), 1U);
}

TEST_F(CrashReportTest, StopsOnAReturnAddressOutsideCode) {
  chain(stack + 0x100, stack + 0x200, text + 0x100);
  chain(stack + 0x200, stack + 0x300, stack + 0x10);
  const Backtrace trace = Backtrace::walk(memory_, text + 0x50, stack + 0x100);
  EXPECT_EQ(trace.frames().size(), 2U);
}

TEST_F(CrashReportTest, CapsTheTraceAtItsCapacity) {
  for (std::uint64_t index = 0; index < 64; ++index) {
    chain(stack + 0x10 * index, stack + 0x10 * (index + 1), text + index * 4);
  }
  const Backtrace trace = Backtrace::walk(memory_, text, stack);
  EXPECT_EQ(trace.frames().size(), Backtrace::capacity);
}

TEST_F(CrashReportTest, DiscoversTheModuleWithNameAndBuildId) {
  const LoadedModules modules = LoadedModules::discover(memory_, text);
  ASSERT_EQ(modules.modules().size(), 1U);
  const LoadedModule& module = modules.modules().front();
  EXPECT_EQ(module.base, text);
  EXPECT_EQ(module.size, 0x7000U);
  EXPECT_EQ(module.label(), "gallery.nro");
  EXPECT_TRUE(module.main);
  EXPECT_TRUE(std::ranges::equal(module.build_id_bytes(), build_id));
  EXPECT_TRUE(modules.find(text + 0x6FFF).has_value());
  EXPECT_FALSE(modules.find(text + 0x7000).has_value());
}

TEST_F(CrashReportTest, NamesAModuleWithoutAHeader) {
  memory_.poke(rodata, 0x1234);
  const LoadedModules modules = LoadedModules::discover(memory_, 0);
  ASSERT_EQ(modules.modules().size(), 1U);
  EXPECT_EQ(modules.modules().front().label(), "module");
  EXPECT_FALSE(modules.main().has_value());
}

TEST_F(CrashReportTest, FormatsACpuExceptionLikeAtmosphere) {
  chain(stack + 0x100, stack + 0x200, text + 0x100);
  chain(stack + 0x200, 0, text + 0x200);
  memory_.poke(stack + 0x80, 0x1122'3344'5566'7788);
  ExceptionDump dump{};
  dump.error_desc = 0x104;
  dump.gprs[0] = 0x2A;
  dump.gprs[28] = text + 0x40;
  dump.fp = stack + 0x100;
  dump.lr = text + 0x30;
  dump.sp = stack + 0x80;
  dump.pc = text + 0x2C;
  dump.esr = 0xF200'03E8;
  dump.far = 0xDEAD;
  CrashSnapshot snapshot = dump.snapshot();
  snapshot.app = "gallery";
  snapshot.unix_time = 1795169472;
  snapshot.thread_id = 0x4F;
  const LoadedModules modules = LoadedModules::discover(memory_, text);
  const Backtrace trace = Backtrace::walk(memory_, dump.pc, dump.fp);
  std::vector<char> storage(CrashReporter::report_capacity);
  CrashReport report{storage};
  const std::string text_report{
      report.compose(snapshot, modules, trace, memory_)};
  for (const std::string_view expected :
       {std::string_view{"arisa Crash Report (v1):\n"},
        std::string_view{"App:                             gallery\n"},
        std::string_view{"Build Id:                        "
                         "deadbeef0102030405060708090a0b0c0d0e0f10\n"},
        std::string_view{"Time:                            "
                         "2026-11-20 10:11:12 UTC\n"},
        std::string_view{"Class:                           CPU Exception\n"},
        std::string_view{"Thread Id:                       0x4f\n"},
        std::string_view{"        Type:                    "
                         "Undefined Instruction (0x104)\n"},
        std::string_view{"        Syndrome:                BRK Instruction\n"},
        std::string_view{"        ESR:                     0xf20003e8\n"},
        std::string_view{"        FAR:                     "
                         "0x000000000000dead\n"},
        std::string_view{"        X[00]:                   "
                         "000000000000002a\n"},
        std::string_view{"        X[28]:                   "
                         "0000000008000040 (gallery.nro + 0x40)\n"},
        std::string_view{"        PC:                      "
                         "000000000800002c (gallery.nro + 0x2c)\n"},
        std::string_view{"        ReturnAddress[00]:       "
                         "000000000800002c (gallery.nro + 0x2c)\n"},
        std::string_view{"        ReturnAddress[02]:       "
                         "0000000008000200 (gallery.nro + 0x200)\n"},
        std::string_view{"        0000000000010080:        "
                         "1122334455667788 0000000000000000\n"},
        std::string_view{"    Module 00:\n"},
        std::string_view{"        Address:                 "
                         "0000000008000000-0000000008007000\n"},
        std::string_view{"        Name:                    gallery.nro\n"},
        std::string_view{"        Main:                    yes\n"}}) {
    EXPECT_NE(text_report.find(expected), std::string::npos)
        << expected << "\n---\n"
        << text_report;
  }
}

TEST_F(CrashReportTest, FormatsAnAbortWithMessageAndResult) {
  chain(stack + 0x100, 0, text + 0x100);
  const CrashSnapshot snapshot{.app = "gallery",
                               .unix_time = std::nullopt,
                               .crash_class = CrashClass::diag_abort,
                               .exception_type = 0,
                               .result = 0x35C,
                               .message = "diagAbortWithResult called",
                               .thread_id = 1,
                               .general = std::nullopt,
                               .fp = stack + 0x100,
                               .lr = 0,
                               .sp = stack + 0xFF8,
                               .pc = text + 0x100};
  const LoadedModules modules = LoadedModules::discover(memory_, text);
  const Backtrace trace = Backtrace::walk(memory_, std::nullopt, snapshot.fp);
  std::vector<char> storage(CrashReporter::report_capacity);
  CrashReport report{storage};
  const std::string text_report{
      report.compose(snapshot, modules, trace, memory_)};
  EXPECT_NE(text_report.find("Class:                           Diag Abort\n"),
            std::string::npos);
  EXPECT_NE(text_report.find("    Message:                     "
                             "diagAbortWithResult called\n"),
            std::string::npos);
  EXPECT_NE(text_report.find("    Result:                      0x0000035c "
                             "(2348-0001)\n"),
            std::string::npos);
  EXPECT_NE(text_report.find("Time:                            unknown\n"),
            std::string::npos);
  EXPECT_EQ(text_report.find("Exception Info:"), std::string::npos);
  EXPECT_EQ(text_report.find("X[00]"), std::string::npos);
  EXPECT_NE(text_report.find("        0000000000011000:        "
                             "???????????????? ????????????????\n"),
            std::string::npos)
      << text_report;
}

TEST_F(CrashReportTest, NeverWritesPastItsStorage) {
  const LoadedModules modules = LoadedModules::discover(memory_, text);
  const Backtrace trace = Backtrace::walk(memory_, text, 0);
  std::array<char, 80> storage{};
  storage.back() = '#';
  CrashReport report{std::span{storage}.first(79)};
  const std::string_view written =
      report.compose(CrashSnapshot{}, modules, trace, memory_);
  EXPECT_EQ(written.size(), 79U);
  EXPECT_EQ(storage.back(), '#');
}

TEST(CrashReportStampTest, FormatsUtcStamps) {
  const auto epoch = CrashReport::stamp(0);
  EXPECT_EQ((std::string_view{epoch.data(), epoch.size()}),
            "1970-01-01_00-00-00");
  const auto later = CrashReport::stamp(1795169472);
  EXPECT_EQ((std::string_view{later.data(), later.size()}),
            "2026-11-20_10-11-12");
}

TEST(CrashReportStampTest, NamesTheTrapClass) {
  EXPECT_EQ(CrashReport::class_name(CrashClass::trap), "Trap");
}

TEST(CrashReportStampTest, NamesResultCodes) {
  const auto code = CrashReport::result_code(CrashReporter::crash_result);
  EXPECT_EQ((std::string_view{code.data(), code.size()}), "2430-0001");
}

TEST(CrashReporterHostTest, InstallsAsANoOpOnTheHost) {
  auto reporter = CrashReporter::install(
      {.directory = "sdmc:/switch/test/crashes", .app = "test"});
  ASSERT_TRUE(reporter.has_value());
  EXPECT_FALSE(CrashReporter::armed());
  EXPECT_EQ(CrashReporter::exception_delivery(),
            ExceptionDelivery::unsupported);
}

}
