# Agent study summary

Mutants: 194 used (339 candidates, 15 do not compile, 130 equivalent by object code). Held-out half: 93 mutants the agent never sees.

Hand-written reference suite (30 tests): 76.8 % of all mutants, 77.4 % of the held-out half.

| model | runs | mode | held-out kill % (mean +- sd) | all mutants % | tests kept | ref + agent, held-out % |
|---|---|---|---|---|---|---|
| qwen2.5-coder:7b | 3 | single | 10.4 +- 9.4 | 10.5 +- 10.1 | 6 | 78.5 +- 1.1 |
| qwen2.5-coder:7b | 3 | loop | 10.8 +- 9.6 | 10.8 +- 10.1 | 7 | 78.5 +- 1.1 |
| qwen2.5-coder:7b | 3 | mutant | 10.8 +- 9.6 | 10.8 +- 10.1 | 7 | 78.5 +- 1.1 |
| qwen3-coder:30b | 3 | single | 34.0 +- 8.2 | 29.6 +- 7.9 | 31 | 81.7 +- 1.1 |
| qwen3-coder:30b | 3 | loop | 46.2 +- 2.1 | 39.9 +- 2.9 | 38 | 82.4 +- 0.6 |
| qwen3-coder:30b | 3 | mutant | 49.5 +- 7.5 | 42.8 +- 6.5 | 44 | 82.8 +- 0.0 |

| model | first-try files that build | first-try files with all tests passing | LLM calls per run | output tokens per run | agent wall time per run (min) |
|---|---|---|---|---|---|
| qwen2.5-coder:7b | 33 % | 0 % | 37 | 105116 | 50.4 |
| qwen3-coder:30b | 92 % | 0 % | 51 | 151541 | 62.8 |
