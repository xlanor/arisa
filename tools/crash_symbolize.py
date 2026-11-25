import argparse
import json
import os
import pathlib
import re
import shutil
import struct
import subprocess
import sys

DOCKER_IMAGE = "switch-ui-devkita64:r30-gcc16.1"
DEVKIT_ADDR2LINE = "/opt/devkitpro/devkitA64/bin/aarch64-none-elf-addr2line"
HOMEBREW_SYMBOLIZER = "/opt/homebrew/opt/llvm/bin/llvm-symbolizer"
ADDRESS = r"([0-9a-fA-F]{16})"
MODULE = r"(?: \((.+) \+ 0x([0-9a-fA-F]+)\))?"


class Frame:
    def __init__(self, label, address, adjust):
        self.label = label
        self.address = address
        self.adjust = adjust


class Crash:
    def __init__(self):
        self.build_id = None
        self.main_base = None
        self.main_name = None
        self.frames = []
        self.source = ""


class ReportReader:
    @staticmethod
    def read(text):
        crash = Crash()
        crash.source = "arisa report"
        cpu = re.search(r"^Class:\s+CPU Exception$", text, re.M) is not None
        modules = re.findall(
            r"^    Module \d+:\n\s+Address:\s+" + ADDRESS + r"-" + ADDRESS +
            r"\n\s+Name:\s+(.*)\n\s+Build Id:\s+(\S+)\n(\s+Main:\s+yes\n)?",
            text, re.M)
        for base, _end, name, build_id, main in modules:
            if main:
                crash.main_base = int(base, 16)
                crash.main_name = name.strip()
                crash.build_id = None if build_id == "unknown" else build_id.lower()
        if crash.build_id is None:
            found = re.search(r"^Build Id:\s+([0-9a-fA-F]+)$", text, re.M)
            if found:
                crash.build_id = found.group(1).lower()
        pc = re.search(r"^\s+PC:\s+" + ADDRESS, text, re.M)
        lr = re.search(r"^\s+LR:\s+" + ADDRESS, text, re.M)
        traced = re.findall(r"^\s+ReturnAddress\[(\d+)\]:\s+" + ADDRESS, text, re.M)
        if cpu and pc:
            crash.frames.append(Frame("pc", int(pc.group(1), 16), False))
            if lr and int(lr.group(1), 16) != 0:
                crash.frames.append(Frame("lr", int(lr.group(1), 16), True))
        for index, address in traced:
            value = int(address, 16)
            adjust = not (cpu and index == "00")
            crash.frames.append(Frame("#" + index, value, adjust))
        return crash


class ErptReader:
    KEYS = ("StackBacktrace64", "ProgramMappedAddr64", "GeneralRegisterAarch64",
            "ExceptionInfoAarch64")

    @staticmethod
    def numbers(value):
        if isinstance(value, list):
            return [ErptReader.number(each) for each in value]
        if isinstance(value, (int, str)):
            return [ErptReader.number(each) for each in re.split(r"[\s,]+", str(value).strip()) if each]
        return []

    @staticmethod
    def number(value):
        if isinstance(value, int):
            return value
        return int(str(value), 0) if str(value).lower().startswith("0x") else int(str(value), 16)

    @staticmethod
    def fields(text):
        stripped = text.strip()
        if stripped.startswith("{"):
            return {key: ErptReader.numbers(value) for key, value in json.loads(stripped).items()
                    if key in ErptReader.KEYS}
        found = {}
        for line in text.splitlines():
            match = re.match(r"^\s*(\w+)\s*[:=]\s*(.*)$", line)
            if match and match.group(1) in ErptReader.KEYS:
                found.setdefault(match.group(1), []).extend(ErptReader.numbers(match.group(2)))
        return found

    @staticmethod
    def read(text):
        fields = ErptReader.fields(text)
        crash = Crash()
        crash.source = "erpt fields"
        mapped = fields.get("ProgramMappedAddr64", [])
        crash.main_base = mapped[0] if mapped else None
        registers = fields.get("GeneralRegisterAarch64", [])
        pc = registers[32] if len(registers) > 32 else None
        cpu = any(value != 0 for value in fields.get("ExceptionInfoAarch64", []))
        for index, address in enumerate(fields.get("StackBacktrace64", [])):
            if address == 0:
                continue
            adjust = not (index == 0 and (cpu or address == pc))
            crash.frames.append(Frame("#%02d" % index, address, adjust))
        return crash

    @staticmethod
    def matches(text):
        return "StackBacktrace64" in text


class ElfBuildId:
    @staticmethod
    def read(path):
        data = pathlib.Path(path).read_bytes()
        if data[:4] != b"\x7fELF" or data[4] != 2:
            return None
        shoff = struct.unpack_from("<Q", data, 0x28)[0]
        shentsize, shnum, shstrndx = struct.unpack_from("<HHH", data, 0x3A)
        sections = [struct.unpack_from("<IIQQQQIIQQ", data, shoff + index * shentsize)
                    for index in range(shnum)]
        names = sections[shstrndx]
        for section in sections:
            start = names[4] + section[0]
            name = data[start:data.index(b"\0", start)].decode()
            if name != ".note.gnu.build-id":
                continue
            offset = section[4]
            namesz, descsz, _kind = struct.unpack_from("<III", data, offset)
            desc = offset + 12 + ((namesz + 3) & ~3)
            return data[desc:desc + descsz].hex()
        return None


class Symbolizer:
    def __init__(self, elf):
        self.elf = os.path.abspath(elf)
        self.command = self.choose()

    def choose(self):
        chosen = os.environ.get("ARISA_SYMBOLIZER")
        if chosen:
            return [chosen]
        for candidate in (shutil.which("llvm-symbolizer"), HOMEBREW_SYMBOLIZER):
            if candidate and os.path.exists(candidate):
                return [candidate]
        for candidate in (shutil.which("aarch64-none-elf-addr2line"), DEVKIT_ADDR2LINE):
            if candidate and os.path.exists(candidate):
                return [candidate]
        if shutil.which("docker"):
            folder = os.path.dirname(self.elf)
            return ["docker", "run", "--rm", "-v", folder + ":/elf:ro", DOCKER_IMAGE,
                    DEVKIT_ADDR2LINE]
        sys.exit("no llvm-symbolizer, aarch64-none-elf-addr2line or docker found")

    def elf_argument(self):
        if self.command[0] == "docker":
            return "/elf/" + os.path.basename(self.elf)
        return self.elf

    def lines(self, address):
        tool = os.path.basename(self.command[-1])
        if "llvm-symbolizer" in tool:
            arguments = self.command + ["--obj=" + self.elf_argument(), "--inlines", "--demangle",
                                        "--output-style=GNU", hex(address)]
        else:
            arguments = self.command + ["-e", self.elf_argument(), "-f", "-C", "-i", hex(address)]
        result = subprocess.run(arguments, capture_output=True, text=True, check=False)
        output = [line for line in result.stdout.splitlines() if line.strip()]
        return list(zip(output[0::2], output[1::2]))


class CrashSymbolizer:
    @staticmethod
    def main():
        parser = argparse.ArgumentParser(
            description="Symbolize an arisa crash report or erpt fields against the matching ELF")
        parser.add_argument("report", help="arisa crash report (.log) or erpt fields (JSON or text)")
        parser.add_argument("elf", help="the ELF the NRO was built from")
        parser.add_argument("--force", action="store_true", help="symbolize even if the build id differs")
        options = parser.parse_args()
        text = pathlib.Path(options.report).read_text(encoding="utf-8", errors="replace")
        crash = ErptReader.read(text) if ErptReader.matches(text) else ReportReader.read(text)
        if crash.main_base is None:
            sys.exit("no main module base in the input")
        elf_id = ElfBuildId.read(options.elf)
        if crash.build_id is None:
            print("build id: not in the input (%s), not checked" % crash.source)
        elif elf_id is None:
            print("build id: the ELF has no .note.gnu.build-id, not checked")
        elif elf_id != crash.build_id:
            print("build id mismatch: report %s, ELF %s" % (crash.build_id, elf_id))
            if not options.force:
                sys.exit(1)
        else:
            print("build id: %s matches" % elf_id)
        symbolizer = Symbolizer(options.elf)
        base = crash.main_base
        for frame in crash.frames:
            offset = frame.address - base
            if offset < 0 or offset >= 1 << 32:
                print("%-5s %016x  (outside the main module)" % (frame.label, frame.address))
                continue
            print("%-5s %016x  %s + 0x%x" % (frame.label, frame.address, crash.main_name or "main", offset))
            looked = offset - 4 if frame.adjust else offset
            entries = symbolizer.lines(looked)
            for index, (function, location) in enumerate(entries):
                inlined = "  (inlined)" if index + 1 < len(entries) else ""
                print("        %s at %s%s" % (function, location, inlined))
        return 0


if __name__ == "__main__":
    sys.exit(CrashSymbolizer.main())
