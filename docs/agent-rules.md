# arisa agent rules

These are the user's standing rules. Breaking any of them means the work is redone.

## Code
- **No comments in code at all.** No inline comments, block comments, Doxygen or section dividers anywhere: headers, sources, tests, CMake or scripts. The only exception is that vendored third-party code keeps its licence headers. Explanations go in README.md or your report.
- Write idiomatic modern C++23 for GCC 16.
  - RAII owns every resource, through move-only handles. Reuse `arisa::support::UniqueHandle` and `ScopedCallback`.
  - Never use `new`, `delete`, `malloc` or `free`.
  - No `init()`, `deinit()` or `shutdown()` methods. If construction can fail, make the constructor private and add a static `create()` that returns `std::expected<T, E>`.
- Failures come back as `std::expected` or `std::optional`. Never a status enum beside loose fields, never nullptr meaning error, never out-params (wrap C APIs that use out-params inside the implementation).
- Use strong types (`arisa::support::StrongValue`, `BoundedValue`) for distinct values.
- Use namespaces, not prefixes: `arisa::provider::horizon`, `arisa::provider::sdl3`, `arisa::render`, and so on.
- `final` only on classes with virtual functions. `[[nodiscard]]` only on error-carrying returns.
- A type with state or rules is a class with private members, valid from construction. An aggregate `struct` is only for plain data.
- No loose helpers. In a class's files every function is a member; internal ones are private or static. No anonymous-namespace helper functions in `.cpp` files.
- Files follow meaning. One owning class per header, and no splitting files for size.
- Prefer the standard library: ranges, span, expected, `move_only_function`, `stop_token`, `flat_map`, print.
- Exceptions and RTTI are off everywhere.
- Threads are created only inside ThreadingProvider implementations. Never use `thread_local`.
- Match the existing arisa code's naming, formatting (`.clang-format`) and structure. Read it before writing.

## Process
- Do not commit, stage or push. The orchestrator reviews and commits. Never touch `.claude/`.
- Do not modify `/Users/jingkai/Documents/public/*`. Never run git or gh against those repositories.
- **Build one thing at a time.** Never run builds in parallel; use `nice -n 10` and `-j4`. The user's desktop froze once from parallel builds.
- **No test loops.** Batch every edit, build only when you need the compiler, and run the full verification once at the very end. If that run fails, fix everything in one pass and run it once more. Fix failures properly: no disabled warnings, no NOLINT, no comments.
- Never open visible windows on the user's Mac. Desktop tests use SDL's offscreen/dummy video and dummy audio drivers.
- Network use is limited to cloning third-party source you are told to vendor.
- Per round, verify with `make fast` only, then build the console NRO with `make switch` once. `make check`, `make asan`, `make tsan`, `make leaks` and `make package-check` run only at milestone ends. Never rerun a gate; report failures instead.
- Report concisely: files added, public API signatures, decisions the brief did not settle, exact verification results, and anything not done.
