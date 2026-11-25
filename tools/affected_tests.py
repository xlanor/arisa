import os
import pathlib
import re
import subprocess
import sys
import time


def stamp_path(build):
    return build / ".affected_tests_stamp"


def test_binaries(build):
    return sorted(path for path in build.glob("arisa_*_tests") if path.is_file())


def registered_prefixes(build):
    listing = subprocess.run(
        ["ctest", "--test-dir", str(build), "-N"],
        capture_output=True,
        text=True,
        check=True,
    ).stdout
    prefixes = {}
    for line in listing.splitlines():
        match = re.search(r"Test\s+#\d+:\s+(\S+)/shard_\d+_of_\d+", line)
        if match:
            prefixes.setdefault(match.group(1), None)
    return list(prefixes)


def suite_of(binary, prefixes):
    stem = binary.name.removeprefix("arisa_").removesuffix("_tests")
    for prefix in prefixes:
        if prefix.replace("/", "_") == stem:
            return prefix
    return None


def main():
    build = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else "build/host-debug")
    jobs = sys.argv[2] if len(sys.argv) > 2 else str(os.cpu_count() or 4)
    stamp = stamp_path(build)
    since = stamp.stat().st_mtime if stamp.exists() else 0.0
    started = time.time()
    prefixes = registered_prefixes(build)
    changed = [
        suite
        for binary in test_binaries(build)
        if binary.stat().st_mtime > since
        and (suite := suite_of(binary, prefixes)) is not None
    ]
    if not changed:
        print("affected: no test binary changed since the last run")
        stamp.touch()
        return 0
    pattern = "^(" + "|".join(re.escape(suite) + "/" for suite in changed) + ")"
    print(f"affected: {', '.join(changed)}")
    result = subprocess.run(
        ["nice", "-n", "10", "ctest", "--test-dir", str(build),
         "--output-on-failure", "-j", jobs, "-R", pattern],
        check=False,
    )
    if result.returncode == 0:
        stamp.touch()
        os.utime(stamp, (started, started))
    return result.returncode


if __name__ == "__main__":
    sys.exit(main())
