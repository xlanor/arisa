import pathlib
import shutil
import subprocess
import sys
import time

FUZZERS = ("theme", "image", "text")


def run(command, **options):
    return subprocess.run(["nice", "-n", "10", *command], check=False, **options)


def build(directory):
    configure = subprocess.run(
        ["cmake", "--preset", "host-fuzz"], stdout=subprocess.DEVNULL, check=False
    )
    if configure.returncode != 0:
        return False
    targets = [f"arisa_{name}_fuzz" for name in FUZZERS]
    compiled = run(["cmake", "--build", str(directory), "-j4", "--target", *targets])
    return compiled.returncode == 0


def replay(binary, regressions):
    inputs = sorted(path for path in regressions.glob("*") if path.is_file())
    if not inputs:
        return True
    result = run([str(binary), *map(str, inputs)], stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)
    return result.returncode == 0


def fuzz(directory, name, seconds):
    binary = directory / f"arisa_{name}_fuzz"
    source = pathlib.Path("tests/fuzz")
    work = directory / "fuzz" / name
    if work.exists():
        shutil.rmtree(work)
    corpus = work / "corpus"
    corpus.mkdir(parents=True)
    log = work / "fuzz.log"
    if not replay(binary, source / "regressions" / name):
        print(f"fuzz {name}: a checked-in regression input fails again")
        return False
    started = time.monotonic()
    with log.open("w") as output:
        result = run(
            [
                str(binary),
                str(corpus),
                str(source / "corpus" / name),
                f"-max_total_time={seconds}",
                "-timeout=10",
                "-rss_limit_mb=4096",
                "-print_final_stats=1",
                f"-artifact_prefix={work}/",
            ],
            stdout=output,
            stderr=subprocess.STDOUT,
        )
    elapsed = time.monotonic() - started
    runs = next(
        (line.split()[-1] for line in log.read_text(errors="replace").splitlines() if line.startswith("stat::number_of_executed_units")),
        "?",
    )
    status = "clean" if result.returncode == 0 else f"failed (exit {result.returncode}), see {log}"
    print(f"fuzz {name}: {status}, {runs} runs in {elapsed:.0f} s")
    return result.returncode == 0


def main():
    directory = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else "build/host-fuzz")
    seconds = int(sys.argv[2]) if len(sys.argv) > 2 else 60
    if not build(directory):
        return 1
    results = [fuzz(directory, name, seconds) for name in FUZZERS]
    return 0 if all(results) else 1


if __name__ == "__main__":
    sys.exit(main())
