# QEMU full-system emulator for the on-target test runs (mps2-an385, Cortex-M3) and the
# node (mps2-an521, Cortex-M33 with TrustZone). Ubuntu 24.04 ships qemu-system-arm 8.2.2.
FROM ubuntu:24.04
RUN apt-get update \
 && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends qemu-system-arm \
 && rm -rf /var/lib/apt/lists/*
