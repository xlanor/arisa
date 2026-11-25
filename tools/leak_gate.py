import concurrent.futures
import os
import pathlib
import subprocess
import sys

ENVIRONMENT = {
    "MallocStackLogging": "1",
    "SDL_VIDEODRIVER": "offscreen",
    "SDL_VIDEO_DRIVER": "offscreen",
    "SDL_AUDIO_DRIVER": "dummy",
    "ARISA_TEST_BUDGET_MS": "0",
}

SHARDS = {"arisa_gallery_tests": 12, "arisa_runtime_tests": 3, "arisa_async_tests": 2}

WORKERS = 8

TOUR = ["--offscreen", "1280x720", "--tour", "1"]


def targets(build):
    found = []
    for path in sorted(path for path in build.glob("*_tests") if path.is_file()):
        shards = SHARDS.get(path.name, 1)
        found.extend((path, index, shards) for index in range(shards))
    gallery = build / "gallery"
    if gallery.is_file():
        found.append((gallery, 0, 1))
    return found


def label(binary, index, shards):
    return binary.name if shards == 1 else f"{binary.name}.{index}of{shards}"


def check(target, reports):
    binary, index, shards = target
    arguments = TOUR if binary.name == "gallery" else []
    report = reports / f"{label(binary, index, shards)}.txt"
    environment = dict(os.environ)
    environment.update(ENVIRONMENT)
    if shards > 1:
        environment["GTEST_TOTAL_SHARDS"] = str(shards)
        environment["GTEST_SHARD_INDEX"] = str(index)
    with report.open("w") as output:
        result = subprocess.run(
            ["nice", "-n", "10", "leaks", "--atExit", "--", str(binary), *arguments],
            stdout=output,
            stderr=subprocess.STDOUT,
            env=environment,
            check=False,
        )
    return result.returncode, report


def main():
    build = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else "build/host-debug")
    reports = build.parent / "leaks"
    reports.mkdir(parents=True, exist_ok=True)
    failed = []
    found = targets(build)
    found.sort(key=lambda target: target[0].stat().st_size, reverse=True)
    with concurrent.futures.ThreadPoolExecutor(max_workers=WORKERS) as pool:
        results = pool.map(lambda target: (target, *check(target, reports)), found)
        for target, code, report in results:
            name = label(*target)
            if code == 0:
                print(f"leaks: {name} clean")
            else:
                print(f"leaks: {name} failed with {code}, see {report}")
                failed.append(name)
    if failed:
        print(f"leaks: {len(failed)} binaries leaked or failed: {', '.join(failed)}")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
