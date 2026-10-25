# Docker Desktop alternative for Windows hosts with no separate Ubuntu WSL distro.
# The Limine binary is still separately pinned and hash-checked by build.sh.
FROM ubuntu:24.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    clang lld libclang-rt-dev cmake ninja-build curl ca-certificates xorriso \
    build-essential make tar coreutils python3 && \
    rm -rf /var/lib/apt/lists/*
WORKDIR /work
ENTRYPOINT ["/bin/bash"]
