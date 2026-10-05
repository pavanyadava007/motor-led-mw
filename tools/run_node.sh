#!/bin/sh
# Boots the node (TF-M + Zephyr, mps2-an521 Cortex-M33) in QEMU with the LAN9118
# Ethernet model on user-mode networking; UDP 5555 is forwarded to 127.0.0.1:5555.
# Console goes to stdout, the TF-M secure-world UART to build/app/tfm_secure.log.
set -e
B=$(cd "$(dirname "$0")/.." && pwd)/build/app
exec docker run --rm -i --name mlmw-node -p 127.0.0.1:5555:5555/udp -v "$B:$B" zqemu:8.2 \
  qemu-system-arm -cpu cortex-m33 -machine mps2-an521 -m 16 -nographic -vga none \
  -nic user,model=lan9118,hostfwd=udp::5555-:5555 \
  -chardev stdio,id=con,mux=on -serial chardev:con -mon chardev=con,mode=readline \
  -serial "file:$B/tfm_secure.log" \
  -device loader,file="$B/zephyr/tfm_merged.hex"
