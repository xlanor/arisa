ARG BASE=switch-ui-devkita64:r30-gcc16.1
FROM ${BASE}

ARG OUNCE_SDK_SHA256=30d28d0909a0a60b4de87930d773ac9770530d70a1b9b3acd3b0491579368a9b

RUN apt-get update \
 && apt-get install -y --no-install-recommends ccache glslang-tools \
 && rm -rf /var/lib/apt/lists/*

COPY --from=ounce-sdk ounce-sdk.tar.xz /tmp/ounce-sdk.tar.xz
RUN echo "${OUNCE_SDK_SHA256}  /tmp/ounce-sdk.tar.xz" | sha256sum -c - \
 && tar -xJf /tmp/ounce-sdk.tar.xz -C /opt/devkitpro \
 && rm /tmp/ounce-sdk.tar.xz \
 && test -f /opt/devkitpro/libounce/cmake/Switch2.cmake \
 && test -f /opt/devkitpro/portlibs/switch2/lib/libvulkan.a \
 && test ! -L /opt/devkitpro/libnx \
 && test -f /opt/devkitpro/libnx/lib/libnx.a \
 && /opt/devkitpro/devkitA64/bin/aarch64-none-elf-gcc -dumpversion | grep -qx '16.1.0'

COPY ounce-smoke /opt/toolchain-ounce-smoke
RUN cmake -S /opt/toolchain-ounce-smoke -B /tmp/ounce-smoke -G Ninja \
      -DCMAKE_TOOLCHAIN_FILE=/opt/devkitpro/libounce/cmake/Switch2.cmake \
 && cmake --build /tmp/ounce-smoke \
 && test -s /tmp/ounce-smoke/toolchain_ounce_smoke.nro \
 && rm -rf /tmp/ounce-smoke

LABEL org.opencontainers.image.title="switch-ui-devkita64-ounce" \
      toolchain.devkitA64="r30-1" \
      toolchain.gcc="16.1.0" \
      toolchain.ounce-sdk="20261113" \
      toolchain.ounce-sdk.sha256="${OUNCE_SDK_SHA256}"
