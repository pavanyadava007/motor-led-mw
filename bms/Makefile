# BMS module: host build and tests, ARM builds (size, PIL, profiling), analysis.
# Host targets need only gcc + python; ARM targets need the Arm GNU toolchain and Docker (QEMU).
PY      ?= python3
CC      ?= gcc
ARMBIN  ?= /opt/arm-gnu-toolchain-13.3.rel1-x86_64-arm-none-eabi/bin
ARMCC   := $(ARMBIN)/arm-none-eabi-gcc
B       := ../build/bms
WARN    := -std=c99 -Wall -Wextra -Werror -pedantic -Wconversion -Wsign-conversion -Wshadow \
           -Wcast-qual -Wstrict-prototypes -Wmissing-prototypes -Wdouble-promotion -ffp-contract=off
LIBSRC  := $(wildcard src/*.c)
TESTSRC := $(wildcard tests/unit/*.c) tests/host/runner.c
HDR     := $(wildcard include/bms/*.h)
TFLAGS  := -std=c99 -Wall -Wextra -Werror -ffp-contract=off -Iinclude -Itests/unit
M4F     := -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard
M33     := -mcpu=cortex-m33 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard

.PHONY: all lib test test-asan pil-host arm clean
all: test

$(B)/host/libbms.a: $(LIBSRC) $(HDR)
	@mkdir -p $(B)/host/obj
	for f in $(LIBSRC); do $(CC) $(WARN) -O2 -Iinclude -c $$f -o $(B)/host/obj/$$(basename $$f .c).o || exit 1; done
	ar rcs $@ $(B)/host/obj/*.o
lib: $(B)/host/libbms.a

$(B)/host/bms_tests: $(LIBSRC) $(TESTSRC) $(HDR) tests/unit/bms_test.h
	@mkdir -p $(B)/host
	$(CC) $(TFLAGS) -O1 $(LIBSRC) $(TESTSRC) -o $@

$(B)/host/bms_tests_asan: $(LIBSRC) $(TESTSRC) $(HDR)
	@mkdir -p $(B)/host
	$(CC) $(TFLAGS) -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all $(LIBSRC) $(TESTSRC) -o $@

test: lib $(B)/host/bms_tests
	$(B)/host/bms_tests

test-asan: $(B)/host/bms_tests_asan
	$(B)/host/bms_tests_asan

# Host side of the processor-in-the-loop SoC run (same C as on the target)
$(B)/host/pil_soc: $(LIBSRC) pil/pil_soc.c pil/host_io.c $(HDR)
	@mkdir -p $(B)/host
	$(CC) $(WARN) -O2 -Iinclude -Ipil $(LIBSRC) pil/pil_soc.c pil/host_io.c -o $@
pil-host: $(B)/host/pil_soc

# ---- emulated Cortex-M4F (QEMU mps2-an386), bare metal with semihosting ----
# startup + semihosting glue (not library code: plain -Wall -Wextra)
TGT := $(B)/arm/startup.o $(B)/arm/io_semihost.o
$(B)/arm/%.o: pil/target/%.c pil/bms_io.h
	@mkdir -p $(B)/arm
	$(ARMCC) -std=c99 -Wall -Wextra -Werror $(M4F) -O2 -ffreestanding -fno-tree-loop-distribute-patterns -Ipil -c $< -o $@
LINK := -nostdlib -nostartfiles -T pil/target/mps2_an386.ld -Wl,--gc-sections
$(B)/arm/pil_soc.elf: $(LIBSRC) pil/pil_soc.c $(TGT) pil/target/mps2_an386.ld $(HDR)
	$(ARMCC) $(WARN) $(M4F) -O2 -ffunction-sections -Iinclude -Ipil $(LINK) $(LIBSRC) pil/pil_soc.c $(TGT) -lgcc -o $@
$(B)/arm/prof.elf: $(LIBSRC) pil/prof_main.c $(TGT) pil/target/mps2_an386.ld $(HDR)
	$(ARMCC) $(WARN) $(M4F) $(PROF_DEFS) -O2 -ffunction-sections -Iinclude -Ipil $(LINK) $(LIBSRC) pil/prof_main.c $(TGT) -lc -lgcc -o $@

# QEMU TCG plugin (instruction counts inside address ranges); header from QEMU v8.2.2
$(B)/qemu/qemu-plugin.h:
	@mkdir -p $(B)/qemu
	curl -sSL -o $@ https://raw.githubusercontent.com/qemu/qemu/v8.2.2/include/qemu/qemu-plugin.h
$(B)/qemu/libinsn_range.so: tools/qemu_plugin/insn_range.c $(B)/qemu/qemu-plugin.h
	$(CC) -O2 -shared -fPIC -I$(B)/qemu $< -o $@
arm: $(B)/arm/pil_soc.elf $(B)/arm/prof.elf $(B)/qemu/libinsn_range.so

clean:
	rm -rf $(B)
