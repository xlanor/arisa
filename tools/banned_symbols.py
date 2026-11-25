import pathlib
import re
import sys

CPP_SUFFIXES = {".cpp", ".hpp", ".h", ".cc"}
SCANNED_DIRECTORIES = ("include", "src", "tests", "switch", "cmake", "examples")
EXCLUDED_DIRECTORIES = ("tests/lib",)
THREADING_PROVIDER_FILES = {
    "include/arisa/provider/standard/std_threading.hpp",
    "src/provider/standard/std_threading.cpp",
}

CPP_RULES = (
    (re.compile(r"\bnew\b(?!_)"), "new expression"),
    (re.compile(r"(?<![=\s])\s*\bdelete\b|^\s*\bdelete\b"), "delete expression"),
    (re.compile(r"\b(?:malloc|calloc|realloc|free)\s*\("), "malloc/free call"),
    (re.compile(r"\b(?:init|deinit|shutdown)\s*\("), "two-phase init member"),
    (re.compile(r"\bthread_local\b"), "thread_local"),
)

ALLOCATION_OPERATOR = re.compile(r"\boperator\s+(?:new|delete)\b")

THREAD_RULE = (
    re.compile(r"\bstd::(?:thread|jthread|async)\b|\bpthread_create\b|\bthreadCreate\b"),
    "thread creation outside the threading provider",
)


def strip_cpp(text):
    code = []
    comments = []
    index = 0
    line = 1
    length = len(text)
    while index < length:
        character = text[index]
        following = text[index + 1] if index + 1 < length else ""
        if character == "/" and following == "/":
            comments.append(line)
            while index < length and text[index] != "\n":
                index += 1
            continue
        if character == "/" and following == "*":
            comments.append(line)
            end = text.find("*/", index + 2)
            end = length if end == -1 else end + 2
            newlines = text.count("\n", index, end)
            code.append("\n" * newlines)
            line += newlines
            index = end
            continue
        raw = re.match(r'R"([^(\s]{0,16})\(', text[index:index + 20])
        if raw and (index == 0 or not (text[index - 1].isalnum() or text[index - 1] == "_")):
            terminator = ")" + raw.group(1) + '"'
            end = text.find(terminator, index + raw.end())
            end = length if end == -1 else end + len(terminator)
            newlines = text.count("\n", index, end)
            code.append('""' + "\n" * newlines)
            line += newlines
            index = end
            continue
        if character in "\"'":
            if character == "'" and index > 0 and text[index - 1].isalnum():
                code.append(character)
                index += 1
                continue
            end = index + 1
            while end < length and text[end] != character and text[end] != "\n":
                end += 2 if text[end] == "\\" else 1
            code.append(character * 2)
            index = end + 1
            continue
        if character == "\n":
            line += 1
        code.append(character)
        index += 1
    return "".join(code), comments


def cmake_comment_lines(text):
    found = []
    for number, content in enumerate(text.splitlines(), start=1):
        in_string = False
        for position, character in enumerate(content):
            if character == '"' and (position == 0 or content[position - 1] != "\\"):
                in_string = not in_string
            elif character == "#" and not in_string:
                found.append(number)
                break
    return found


def scanned_files(root):
    for directory in SCANNED_DIRECTORIES:
        base = root / directory
        if not base.is_dir():
            continue
        for path in sorted(base.rglob("*")):
            relative = path.relative_to(root).as_posix()
            if not path.is_file() or relative.startswith(EXCLUDED_DIRECTORIES):
                continue
            yield path, relative
    top_level = root / "CMakeLists.txt"
    if top_level.is_file():
        yield top_level, "CMakeLists.txt"


def check_cpp(path, relative, violations):
    code, comments = strip_cpp(path.read_text(encoding="utf-8"))
    violations.extend(f"{relative}:{line}: comment" for line in comments)
    rules = list(CPP_RULES)
    if relative not in THREADING_PROVIDER_FILES:
        rules.append(THREAD_RULE)
    for number, content in enumerate(code.splitlines(), start=1):
        content = ALLOCATION_OPERATOR.sub("operator_allocation", content)
        for pattern, label in rules:
            if pattern.search(content):
                violations.append(f"{relative}:{number}: {label}")


def check_cmake(path, relative, violations):
    text = path.read_text(encoding="utf-8")
    violations.extend(f"{relative}:{line}: comment" for line in cmake_comment_lines(text))


def check_python(path, relative, violations):
    for number, content in enumerate(path.read_text(encoding="utf-8").splitlines(), start=1):
        if content.lstrip().startswith("#"):
            violations.append(f"{relative}:{number}: comment")


def main():
    root = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()
    violations = []
    for path, relative in scanned_files(root):
        if path.suffix in CPP_SUFFIXES:
            check_cpp(path, relative, violations)
        elif path.suffix == ".cmake" or path.name == "CMakeLists.txt":
            check_cmake(path, relative, violations)
    for tool in sorted((root / "tools").glob("*.py")):
        check_python(tool, tool.relative_to(root).as_posix(), violations)
    for violation in violations:
        print(violation)
    if violations:
        print(f"{len(violations)} banned symbol(s) found", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
