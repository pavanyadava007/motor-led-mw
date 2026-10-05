# Source before any build: Zephyr workspace, west venv, host Arm GNU toolchain.
export ZEPHYR_WS=${ZEPHYR_WS:-$HOME/workspace/zephyr-ws}
export ZEPHYR_BASE=$ZEPHYR_WS/zephyr
export ZEPHYR_TOOLCHAIN_VARIANT=gnuarmemb
export GNUARMEMB_TOOLCHAIN_PATH=${GNUARMEMB_TOOLCHAIN_PATH:-/opt/arm-gnu-toolchain-13.3.rel1-x86_64-arm-none-eabi}
. $ZEPHYR_WS/.venv/bin/activate
export PATH=$(cd "$(dirname "${BASH_SOURCE:-$0}")" && pwd)/tools/bin:$PATH
