FROM devkitpro/devkita64@sha256:1fc388c3a0d34bd2045a6dadcb1020e069d5f876a187fd705de14b4440c00282

RUN dkp-pacman -Syu --noconfirm \
 && dkp-pacman -S --noconfirm --needed switch-dev deko3d uam switch-cmake \
 && dkp-pacman -Scc --noconfirm \
 && dkp-pacman -Q > /opt/devkitpro/package-manifest.txt

RUN test "$(dkp-pacman -Q devkitA64 | cut -d' ' -f2)" = "r30-1" \
 && /opt/devkitpro/devkitA64/bin/aarch64-none-elf-gcc -dumpversion | grep -qx '16.1.0'

COPY smoke /opt/toolchain-smoke
RUN cmake -S /opt/toolchain-smoke -B /tmp/smoke -G Ninja \
      -DCMAKE_TOOLCHAIN_FILE=/opt/devkitpro/cmake/Switch.cmake \
 && cmake --build /tmp/smoke \
 && test -s /tmp/smoke/toolchain_smoke.nro \
 && rm -rf /tmp/smoke

LABEL org.opencontainers.image.title="switch-ui-devkita64" \
      toolchain.devkitA64="r30-1" \
      toolchain.gcc="16.1.0" \
      toolchain.base="devkitpro/devkita64:20260219"
