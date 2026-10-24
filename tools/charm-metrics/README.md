# Charm++ / Charm# Halstead analyzer

This is a **source-level lexical Halstead measurement instrument** for the thesis, not a compiler or an AST-based complexity estimator. See [classification policy](../../docs/halstead-classification-policy.md) before interpreting results.

Build and test:

```sh
make -C tools/charm-metrics test
make -C tools/charm-metrics charm-metrics
```

Compare all four paired benchmarks (run from repository root):

```sh
tools/charm-metrics/charm-metrics benchmark --root benchmarks --format text
tools/charm-metrics/charm-metrics benchmark --root benchmarks --format json > halstead-results.json
tools/charm-metrics/charm-metrics benchmark --root benchmarks --format csv > halstead-results.csv
# Sensitivity analysis: exclude structural punctuation
tools/charm-metrics/charm-metrics benchmark --root benchmarks --punctuation exclude --format csv > halstead-no-punctuation.csv
```

Compare any two directory trees:

```sh
tools/charm-metrics/charm-metrics compare \
  --charmpp-dir benchmarks/primes/charmpp \
  --charmsharp-dir benchmarks/primes/charmsharp --format json
```

The benchmark command discovers paired `charmpp/` and `charmsharp/` subdirectories, excludes build artifacts and Makefiles, and measures `.ci` plus authored C++ on the Charm++ side and `.csh` plus authored C++ support files on the Charm# side. Each file is tokenized according to its own extension (`.ci`, `.csh`, or C++). It aggregates raw operator/operand frequencies across files **before** evaluating nonlinear Halstead formulas.

Version 0.2 treats boolean/null literals as operands and supports `--punctuation include` (default) or `exclude` (omit precisely `; , ( ) { } [ ] :`). Both variants are labeled in JSON/CSV output and CI artifacts. The output includes the four raw values (n1,n2,N1,N2), Vocabulary, Length, Volume, Difficulty, Effort, theoretical Time/bugs, and signed percentage change `(Charm# - Charm++) / Charm++ * 100`. A negative change indicates less measured lexical complexity. JSON additionally contains exact token frequency inventories and source paths. The GitHub Actions metrics workflow uploads JSON and CSV for both punctuation modes.

**Limitations:** this policy includes punctuation and excludes preprocessor directive text; it does not evaluate preprocessor conditions or expand macros. Both branches of conditional preprocessing are therefore counted if present in raw source. Charm# benchmarks must be behaviorally equivalent to their Charm++ counterparts. The derived Time/Bug quantities are *not measured human time or observed defects*. No source file hashes are stored yet: pin the Git commit SHA when citing a particular measurement run.

In particular, do not infer empirical statistical significance from these four hand-selected benchmark pairs. This is a descriptive comparison; the thesis's programmer-experiment data must be analyzed separately.
