#!/bin/sh
# Full agent study: 3 seeds per model, each run gives the single / loop / mutant snapshots.
set -e
cd "$(dirname "$0")/.."
. ./env.sh
for model in qwen3-coder:30b qwen2.5-coder:7b; do
  for seed in 1 2 3; do
    python agent/agent.py --model "$model" --seed "$seed" --fix-iters 4 --mutant-rounds 2
  done
done
