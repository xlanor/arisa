ARG BASE=switch-ui-devkita64:r30-gcc16.1-ounce20261113
FROM ${BASE}

ARG DRAKO3D_COMMIT=d1f05077667d0eabdd39d8a65483ffcec51807c2
ARG UAM2_COMMIT=b794767cd0e4854199271dfd959798b8714edd23
ARG DRAKOTOOLS_COMMIT=b32fe6e43533e80414f6aa4ad3b899ed077f806c
ARG RUST_VERSION=1.91.0

RUN apt-get update \
 && apt-get install -y --no-install-recommends bison flex pkg-config git \
      python3-venv libdrm-dev libexpat1-dev zlib1g-dev libzstd-dev \
      spirv-tools libclang-dev curl ca-certificates \
 && rm -rf /var/lib/apt/lists/*

RUN python3 -m venv /opt/drako/venv \
 && /opt/drako/venv/bin/pip install --no-cache-dir 'meson==1.9.1' mako pyyaml packaging ninja
ENV PATH=/opt/drako/venv/bin:/opt/drako/bin:/root/.cargo/bin:${PATH}

RUN curl -sSf https://sh.rustup.rs | sh -s -- -y --profile minimal --default-toolchain ${RUST_VERSION} \
 && cargo install --locked bindgen-cli cbindgen \
 && rm -rf /root/.cargo/registry /root/.cargo/git

COPY --from=drakotools . /tmp/drakotools
RUN test "$(git -C /tmp/drakotools rev-parse HEAD)" = "${DRAKOTOOLS_COMMIT}" \
 && meson setup /tmp/drakotools/build /tmp/drakotools --prefix=/opt/drako \
 && ninja -C /tmp/drakotools/build install \
 && rm -rf /tmp/drakotools

COPY --from=drako3d . /tmp/drako3d
RUN test "$(git -C /tmp/drako3d rev-parse HEAD)" = "${DRAKO3D_COMMIT}" \
 && make -C /tmp/drako3d GPU_ARCH=ampere -j4 install \
 && test -f /opt/devkitpro/libounce/lib/libdrako3d.a \
 && test -f /opt/devkitpro/libounce/include/drako3d.hpp \
 && rm -rf /tmp/drako3d

COPY --from=uam2 . /tmp/uam2
RUN test "$(git -C /tmp/uam2 rev-parse HEAD)" = "${UAM2_COMMIT}" \
 && meson setup /tmp/uam2/build /tmp/uam2 --prefix=/opt/drako \
 && ninja -C /tmp/uam2/build -j4 \
 && install -D /tmp/uam2/build/source/uam2 /opt/drako/bin/uam2 \
 && rm -rf /tmp/uam2

RUN printf '#version 460\nlayout(location=0) out vec4 c;\nvoid main(){c=vec4(1.0);}\n' > /tmp/smoke.frag \
 && uam2 -o /tmp/smoke.dksh -s frag /tmp/smoke.frag \
 && test -s /tmp/smoke.dksh \
 && rm /tmp/smoke.frag /tmp/smoke.dksh

LABEL org.opencontainers.image.title="switch-ui-devkita64-ounce-drako" \
      toolchain.drako3d="${DRAKO3D_COMMIT}" \
      toolchain.uam2="${UAM2_COMMIT}" \
      toolchain.drakotools="${DRAKOTOOLS_COMMIT}"
