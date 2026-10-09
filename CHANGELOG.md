# Changelog

## v0.2.0 "bms" (2026-10-09)

Added a battery management (BMS) module in `bms/`:

- Portable C99 library (no dynamic memory, no recursion, no OS/libc calls): cell monitoring with debounce and
  latched faults, open-wire / stuck-sensor / NTC plausibility checks, contactor state machine with precharge and
  timeout, Coulomb counting + EKF SoC on a 1-RC ECM (float32, no libm), passive balancing, CAN packing with
  CRC-8 SAE J1850 + alive counter E2E protection and receive checks, DBC file.
- ECM fitted on the McMaster LG 18650HG2 C/20 + HPPC tests (CC BY 4.0); SoC validated on 11 held-out drive
  cycles at 25 and 0 degC; same C code bit-identical on an emulated Cortex-M4F and the host.
- 24 derived requirements with a traceability matrix (`bms/requirements.yaml`, `bms/tools/trace.py`).
- 62 portable unit tests (host, ASan/UBSan, and ztest on an emulated Cortex-M3), 8 DBC/CRC cross-checks,
  host mutation testing, cppcheck + MISRA addon report, emulated instruction-count profiling, size and stack.
- GitHub Actions workflow `.github/workflows/bms.yml`; `make bms-*` targets.

Existing parts (mlmw middleware, node app, test agent) are unchanged. `make lint` now also checks `bms/`,
`tools/build_site.py` renders images and links the BMS section.

## v0.1.0 (2026-10-05)

Motor + LED middleware on Zephyr/TF-M with an on-target AI test agent scored by mutation testing.
