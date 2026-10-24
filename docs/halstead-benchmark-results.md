# Initial Halstead benchmark comparison (measurement policy v0.1)

Source: [GitHub Actions run 40039139632](https://github.com/dmorganb/charmsharp/actions/runs/40039139632), PR #29 (tested merged evaluation ref `ddd8c64a0b72eec087876013207f405a2564e6b9`, PR head `e24a7dd3ae89cb5e4ea8d8a2ff505308b8e88e4d`). The associated `halstead-results` artifact contains machine-readable JSON (including token inventories and file lists) and CSV.

The tool uses [lexical policy 0.1](halstead-classification-policy.md). These are calculations over authored source, not compiler-generated output.

| Benchmark | Charm++ volume | Charm# volume | Volume change | Charm++ difficulty | Charm# difficulty | Difficulty change | Charm++ effort | Charm# effort | Effort change |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Jacobi2D | 8196.129 | 7694.267 | -6.12% | 116.477 | 115.343 | -0.97% | 954662.718 | 887479.992 | -7.04% |
| Prefix Sum | 2232.174 | 1828.019 | -18.11% | 44.250 | 40.194 | -9.17% | 98773.680 | 73475.178 | -25.61% |
| Primes | 2231.318 | 1863.317 | -16.49% | 63.158 | 54.971 | -12.96% | 140925.334 | 102429.217 | -27.32% |
| Reduction | 1194.000 | 968.319 | -18.90% | 28.171 | 25.455 | -9.64% | 33636.686 | 24648.129 | -26.72% |

Percentage change is `(Charm# - Charm++) / Charm++ * 100`; negative is lower under the specified measurement policy.

## Interpretation

All four paired source implementations exhibit lower lexical Volume, Difficulty, and Effort for Charm# in this initial run. The size of the effect varies across benchmarks. Do not present an arithmetic average as a statistically independent study: this is a four-pair descriptive sample.

The classification policy counts punctuation as operators, excludes preprocessor directive lines, does not expand macros, and counts all conditional branches in raw source. These methodological choices are especially relevant for the thesis. The Charm++ side includes handwritten `.ci` and C++ files; Charm# includes authored `.csh` files.

Before thesis publication, independently audit the token inventories, ensure benchmark implementations are equivalent, record the code and analyzer commit SHA, and run a punctuation-policy sensitivity analysis. No claim is made here about measured comprehension time, developer productivity, defect rates, or statistical significance.
