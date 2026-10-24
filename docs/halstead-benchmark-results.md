# Halstead benchmark comparison — classification policy v0.2

Source: [GitHub Actions run 40078571652](https://github.com/dmorganb/charmsharp/actions/runs/40078571652), PR #30. The uploaded `halstead-results` artifact includes **four** files: JSON and CSV for both punctuation-including and punctuation-excluding modes. Input manifests and operator/operand frequency inventories are in the JSON. Pin this run's commit when citing results.

Measurement: developer-authored source only, aggregating `.ci` plus C++ for Charm++ and `.csh` plus authored C++ support files for Charm#. Each file uses an extension-appropriate keyword set. The [v0.2 policy](halstead-classification-policy.md) counts `true`, `false`, `nullptr` as operands, unlike v0.1.

## Default: include structural punctuation

| Benchmark | Charm++ volume | Charm# volume | Volume change | Charm++ difficulty | Charm# difficulty | Difficulty change | Charm++ effort | Charm# effort | Effort change |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Jacobi2D | 8196.129 | 7694.267 | -6.12% | 116.477 | 115.343 | -0.97% | 954662.718 | 887479.992 | -7.04% |
| Prefix Sum | 2232.174 | 1828.019 | -18.11% | 44.250 | 40.194 | -9.17% | 98773.680 | 73475.178 | -25.61% |
| Primes | 2231.318 | 1863.317 | -16.49% | 60.500 | 52.500 | -13.22% | 134994.726 | 97824.161 | -27.53% |
| Reduction | 1194.000 | 968.319 | -18.90% | 28.171 | 25.455 | -9.64% | 33636.686 | 24648.129 | -26.72% |

## Sensitivity: exclude only `; , ( ) { } [ ] :`

| Benchmark | Volume change | Difficulty change | Effort change |
| --- | ---: | ---: | ---: |
| Jacobi2D | -5.60% | -0.55% | -6.12% |
| Prefix Sum | -17.11% | -9.17% | -24.71% |
| Primes | -16.40% | -15.39% | -29.27% |
| Reduction | -16.42% | -8.29% | -23.35% |

Change = `(Charm# - Charm++) / Charm++ * 100`; a negative value means a reduction for Charm#.

## Interpretation and limitations

The direction of the lexical comparison is stable across both punctuation policies for all four paired applications. This is **descriptive evidence** about source notation, not a statistically powered developer study, comprehension-time measurement, or observed defect count.

Jacobi2D has a large numerical-computation body that is almost unchanged by Charm#, which plausibly explains its smaller relative change.

The analyzer does not expand macros, evaluate `#if` branches, or parse full C++ semantics; it ignores preprocessor directive text and measures the remaining raw source tokens. A separate audit of the benchmark inputs and inventories remains important before publication.

## Historical comparability

The earlier [v0.1 run 40039139632](https://github.com/dmorganb/charmsharp/actions/runs/40039139632) treated the `false` literal as an operator. Its Primes Difficulty/Effort are superseded by these v0.2 results. The other three benchmark scores did not change. Do not mix results from different policies in one statistical dataset.
