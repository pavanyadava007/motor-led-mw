# Every target sources env.sh (Zephyr workspace, west venv, Arm GNU toolchain).
SH := bash -c
ENV := . ./env.sh &&

.PHONY: help qemu-image ref mutants mutation-ref app node demo agent study report size test lint ollama \
	bms-data bms-fit bms-test bms-target bms-soc bms-profile bms-mutation bms-static bms-trace bms-report bms-all bms-ci-image

help:            ## list targets
	@grep -E '^[a-z-]+:.*##' $(MAKEFILE_LIST) | sed 's/:.*##/ -/'

qemu-image:      ## build the zqemu:8.2 container (qemu-system-arm 8.2)
	docker build -t zqemu:8.2 docker

ref:             ## run the hand-written reference suite on the emulated Cortex-M3
	$(SH) '$(ENV) python tools/target.py tests/ref/src'

mutants:         ## generate the mutants (results/mutants.json)
	$(SH) '$(ENV) python tools/mutate.py generate'

mutation-ref:    ## mutation score of the reference suite
	$(SH) '$(ENV) python tools/mutate.py run tests/ref/src --name ref --workers 12'

app:             ## build the node for mps2/an521/cpu0/ns (Zephyr + TF-M)
	$(SH) '$(ENV) west build -b mps2/an521/cpu0/ns app -d build/app'

node:            ## boot the node in QEMU (UDP 5555 on 127.0.0.1)
	tools/run_node.sh

demo:            ## run the host client against a running node
	$(SH) '$(ENV) python tools/node_client.py'

ollama:          ## local model server on 127.0.0.1:11436
	docker run -d --name mlmw-ollama --gpus all -v isa_ollama:/root/.ollama -p 127.0.0.1:11436:11434 \
	  -e OLLAMA_FLASH_ATTENTION=1 -e OLLAMA_KEEP_ALIVE=4h ollama/ollama:latest

agent:           ## one agent run, e.g. make agent MODEL=qwen3-coder:30b SEED=1
	$(SH) '$(ENV) python agent/agent.py --model $(MODEL) --seed $(SEED)'

study:           ## full study: 2 models x 3 seeds
	tools/run_experiments.sh

report:          ## aggregate results into results/summary.md
	$(SH) '$(ENV) python tools/report.py'

size:            ## middleware size for Cortex-M23
	$(SH) '$(ENV) python tools/size_m23.py'

test:            ## host tests of the Python tooling
	$(SH) '$(ENV) python -m pytest -q'

lint:
	$(SH) '$(ENV) ruff check tools agent tests/py bms/tools bms/tests/py'

# ---- BMS module (bms/, see README "BMS") ----
bms-data:        ## download the LG 18650HG2 test data (Mendeley Data, CC BY 4.0)
	$(SH) '$(ENV) python bms/tools/fetch_data.py'

bms-fit:         ## fit the ECM on C/20 + HPPC, regenerate bms/src/bms_ecm_lg_hg2.c
	$(SH) '$(ENV) python bms/tools/fit_ecm.py'

bms-test:        ## host unit tests (+ ASan/UBSan) and Python DBC cross-checks
	$(SH) '$(ENV) python bms/tools/run_host.py --asan-docker'

bms-target:      ## the same unit tests as ztest on the emulated Cortex-M3
	$(SH) '$(ENV) python bms/tools/run_target.py'

bms-soc:         ## held-out SoC evaluation, host + emulated Cortex-M4F parity
	$(MAKE) -C bms pil-host arm
	$(SH) '$(ENV) python bms/tools/soc_eval.py'

bms-profile:     ## size, stack, emulated instruction counts
	$(MAKE) -C bms arm
	$(SH) '$(ENV) python bms/tools/profile.py --crc-bitwise && python bms/tools/profile.py'

bms-mutation:    ## mutation score of the unit tests on bms/src
	$(SH) '$(ENV) python bms/tools/mutate_bms.py && python bms/tools/mutate_bms.py --exclude test_details.c --out mutation_without_detail_tests.json'

bms-static:      ## cppcheck (+ MISRA addon) in the CI image, strict compile
	$(SH) '$(ENV) python bms/tools/static_analysis.py'

bms-ci-image:    ## ubuntu 24.04 image with gcc, cppcheck (same packages as the CI runner)
	docker build -t bms-ci:24.04 -f bms/docker/Dockerfile.ci bms/docker

bms-trace:       ## requirements traceability matrix
	$(SH) '$(ENV) python bms/tools/trace.py'

bms-report:      ## results/bms/RESULTS.md
	$(SH) '$(ENV) python bms/tools/make_results.py'

bms-all: bms-test bms-target bms-soc bms-profile bms-mutation bms-static bms-trace bms-report
