# motor-led-mw

Demo page: https://huggingface.co/spaces/pavanyadava07/motor-led-mw

Reusable C middleware for a small automotive motor + LED node, integrated on Zephyr with
TrustZone (TF-M), an Ethernet stack and PSA Crypto, plus an AI agent that writes the unit tests
and runs them on the target. Test quality is measured by mutation testing on the emulated
target, not by "the agent wrote tests".

Everything runs in QEMU on a cloud VM (Arm MPS2 boards). No real board, motor or LED was used:
the motor and the LED board are simple software models (see "Limits").

## What is in it

| Part | What it does | Where |
|---|---|---|
| `mlmw/` middleware | Plain C99, no OS headers, no dynamic memory. LIN 2.x protected id and classic/enhanced checksum; six-step BLDC block commutation (hall decode, gate pattern, speed from hall period, ramp, PI with anti-windup, latched over-current and stall faults); RGB LED mixing (gamma, per-channel calibration, thermal derating); authenticated command frames with replay protection. Crypto is a callback, so the library stays portable. | `mlmw/include/mlmw/*.h` (the spec is in the header comments) |
| Node application | Zephyr 4.2 on `mps2/an521/cpu0/ns` (Cortex-M33). Zephyr runs non-secure, TF-M 2.2 runs secure. 1 kHz speed loop, 20 kHz hall commutation on a simulated motor, LED thermal model, telemetry every 100 ms. Commands arrive over UDP on the LAN9118 Ethernet model; the 8-byte AES-128 CMAC tag is checked with PSA Crypto inside TF-M, and the key lives in TF-M Internal Trusted Storage. | `app/` |
| Target test harness | Builds any folder of ztest files with the middleware for `mps2/an385` (Cortex-M3), runs it in QEMU and parses the result. | `tools/target.py`, `tests/harness/` |
| Mutation testing | Generates every single-change mutant of `mlmw/src/*.c` (operator swaps, constants +-1, statement deletion), drops mutants that do not compile or whose object code equals the original, and runs a suite against each mutant on the target. | `tools/mutate.py` |
| Test agent | A local code model writes one ztest file per module from the header and source. Three modes: `single` (one shot), `loop` (sees compiler errors and on-target failures, up to 4 fixes), `mutant` (also sees surviving mutants from a TRAIN half, 2 rounds). Scored on a HELD-OUT half it never sees. | `agent/agent.py` |
| Host client | Sends signed frames to the node and checks acks and telemetry. | `tools/node_client.py` |

## Results

All numbers come from files in `results/` and can be regenerated with the Makefile.

### Size on Cortex-M23

`tools/size_m23.py`, arm-none-eabi-gcc 13.3.1, `-mcpu=cortex-m23 -Os`: **1,030 bytes of code, 0 bytes of RAM data**,
largest stack frame 48 bytes (`results/size_m23.json`). Not counted: libgcc helpers for the 64-bit multiply in the
PI controller. Cortex-M23 is the core in Infineon's MOTIX TLE99xx and LITIX Interior parts; this is a compile-size
check only, nothing ran on those parts.

### Node demo (QEMU mps2-an521, TF-M + Zephyr)

`make app node`, then `make demo` (`results/node_demo.json`). Non-secure Zephyr image: 72.7 kB flash.

| Check over UDP | Expected | Result |
|---|---|---|
| set LED, set speed +2000 rpm | OK | OK |
| replay of the same frame | rejected (replay) | rejected |
| one tag bit flipped | rejected (MAC) | rejected |
| frame signed with a wrong key | rejected (MAC) | rejected |
| speed 5000 rpm (limit 3000) | rejected (argument) | rejected |
| unknown command | rejected (command) | rejected |
| reverse to -1500 rpm, stop, LED white | OK | OK |

10/10 checks pass. On the simulated motor, +2000 rpm settles within 5 % after 0.77 s of node time, the reversal to
-1500 rpm after 1.65 s, and stop brings it to 0. With the LED at full white the board model heats to 98 C and the
derating cuts the red duty from 65531 to 49559, which is what the formula in `led.h` gives. Acks come back in about
1.5 ms. These times are QEMU on a cloud VM, not hardware timings.

### Mutation testing and the test agent

`make mutants mutation-ref`, `make study report` (`results/mutants.json`, `results/ref_mutation.json`,
`results/agent/*/summary.json`, `results/summary.md`).

339 single-change mutants were generated from `mlmw/src/*.c`. 15 do not compile and 130 produce the same object code
as the original (equivalent), so **194 mutants** are used. They are split by a hash of their id into a TRAIN half (101),
which the agent may see in `mutant` mode, and a HELD-OUT half (93), which only the scoring uses.

The hand-written reference suite (30 tests in `tests/ref/`) kills **149 of 194 (76.8 %)**, 72 of the 93 held-out
mutants (77.4 %). Each suite runs on the emulated Cortex-M3 in about 0.5 s; a full mutation run takes under 3 minutes
with 12 parallel QEMU workers.

Agent study, 3 seeds per model, temperature 0.2, models served locally by Ollama (4-bit) on one NVIDIA L4:

| Model | Mode | Held-out mutants killed (mean +- sd) | Tests kept | Reference + agent, held-out |
|---|---|---|---|---|
| Qwen3-Coder 30B | single (one shot) | 34.0 % +- 8.2 | 31 | 81.7 % +- 1.1 |
| Qwen3-Coder 30B | loop (compiler + on-target feedback) | 46.2 % +- 2.1 | 38 | 82.4 % +- 0.6 |
| Qwen3-Coder 30B | mutant (+ surviving TRAIN mutants) | 49.5 % +- 7.5 | 44 | **82.8 % +- 0.0** |
| Qwen2.5-Coder 7B | single | 10.4 % +- 9.4 | 6 | 78.5 % +- 1.1 |
| Qwen2.5-Coder 7B | loop | 10.8 % +- 9.6 | 7 | 78.5 % +- 1.1 |
| Qwen2.5-Coder 7B | mutant | 10.8 % +- 9.6 | 7 | 78.5 % +- 1.1 |
| hand-written reference | - | 77.4 % | 30 | - |

What this shows:

- **On its own the agent is weaker than a hand-written suite** (best 49.5 % vs 77.4 %). No first-try file passed
  completely: the 30B model builds 92 % of its files first time but gets some expected values wrong when it works them
  out by hand from the header (LIN parity bits, CMAC frame layouts). Small tests matter: the cleanup removes a failing
  test, so one wrong number in a big test loses all its good checks.
- **On-target feedback is the step that helps most**: one shot to loop goes from 34.0 % to 46.2 % and the spread
  across seeds drops from 8.2 to 2.1 points. Mutant feedback adds a little more on average (49.5 %), with high variance.
- **Added to the hand-written suite, the agent's tests catch 5 of the 21 held-out mutants the hand suite missed, in
  every seed** (82.8 %). Three of them come up in all three seeds: an off-by-one in the LIN checksum loop that reads
  one byte past the buffer, a truncated reverse-commutation table, and an over-current debounce counter that is not
  reset. The other two differ by seed (a deleted clear-fault handler, a wrong sequence-number byte, a deleted LED
  calibration step, and others).
- The 7B model mostly fails to build (33 % of first-try files) and does not recover with feedback; at this size the
  agent loop does not pay off.

How the feedback works: the harness header `tests/harness/include/mlmw_test.h` gives `EXPECT_EQ(actual, expected)`,
which prints both values on failure. When a test fails on the current code, the agent sees the printed values and is
asked to recompute the expectation from the spec. So the final suites are regression tests of the current behaviour:
they protect what the code does now, and a spec bug that the agent accepts would need human review to catch.
Each agent run (30B) takes about an hour, 51 model calls and 150k output tokens.

## Run it

Needs: Linux, Docker, an NVIDIA GPU for the agent (tested on one L4), Arm GNU Toolchain 13.3, Python 3.11.

```bash
# one-time: Zephyr workspace (west, narrow module set) and the QEMU container
uv venv -p 3.11 ~/workspace/zephyr-ws/.venv && . ~/workspace/zephyr-ws/.venv/bin/activate
uv pip install west ninja && cd ~/workspace/zephyr-ws
git clone --depth 1 --branch v4.2.0 https://github.com/zephyrproject-rtos/zephyr
west init -l zephyr
west config manifest.project-filter -- '-.*,+cmsis,+cmsis_6,+mbedtls,+trusted-firmware-m,+mcuboot'
west update --narrow -o=--depth=1
uv pip install -r zephyr/scripts/requirements-base.txt -r modules/tee/tf-m/trusted-firmware-m/tools/requirements.txt
cd - && make qemu-image
# env.sh puts tools/bin/qemu-system-arm (runs zqemu:8.2) on PATH

make ref            # reference suite on the emulated Cortex-M3
make mutants mutation-ref
make app node       # terminal 1: boot the node
make demo           # terminal 2: signed commands over UDP
make ollama study report   # agent study (about 4 h on one L4)
make test lint      # host tests of the Python tooling
```

## Limits

- Emulation only: QEMU MPS2 boards, a LAN9118 Ethernet model and software models of the motor and LED board.
  Commutation, timing and current numbers say nothing about a real inverter.
- The node imports the demo key from firmware on first boot. A product would provision it at end of line.
- The 8-byte truncated CMAC with a 16-bit counter is a demo protocol, not a reviewed security design.
- The middleware is small on purpose. It is not MISRA-checked or ISO 26262 qualified.
- Mutation score depends on the operator set; mutants with identical object code are treated as equivalent, others
  may still be equivalent in behaviour.
- The agent study uses two local models (Qwen3-Coder 30B and Qwen2.5-Coder 7B, 4-bit) and three seeds each.

## Layout

```
mlmw/            middleware (include/ = spec, src/ = implementation)
app/             Zephyr + TF-M node for mps2/an521/cpu0/ns
tests/ref/       hand-written reference ztest suite
tests/harness/   generic ztest image for any test folder
tests/py/        host tests of the tooling
tools/           target runner, mutation testing, node client, size check, report
agent/           test-writing agent
results/         every number in this README
```

Licence: MIT.
