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
```

Compare any two directory trees:

```sh
tools/charm-metrics/charm-metrics compare \
  --charmpp-dir benchmarks/primes/charmpp \
  --charmsharp-dir benchmarks/primes/charmsharp --format json
```

The benchmark command discovers paired `charmpp/` and `charmsharp/` subdirectories, excludes build artifacts and Makefiles, and measures `.ci`, `.C`, `.cpp`, `.cc`, `.cxx`, `.h`, `.hpp` on the Charm++ side and `.csh` on the Charm# side. It aggregates raw operator/operand frequencies across files **before** evaluating nonlinear Halstead formulas.

The output includes the four raw values (n1,n2,N1,N2), Vocabulary, Length, Volume, Difficulty, Effort, theoretical Time/bugs, and signed percentage change `(Charm# - Charm++) / Charm++ * 100`. A negative change indicates less measured lexical complexity. JSON additionally contains exact token frequency inventories and source paths. The GitHub Actions metrics workflow uploads a JSON and CSV artifact for each run.

**Limitations:** this policy includes punctuation and excludes preprocessor directive text; it does not evaluate preprocessor conditions or expand macros. Both branches of conditional preprocessing are therefore counted if present in raw source. Charm# benchmarks must be behaviorally equivalent to their Charm++ counterparts. The derived Time/Bug quantities are *not measured human time or observed defects*. No source file hashes are stored yet: pin the Git commit SHA when citing a particular measurement run.

In particular, do not infer empirical statistical significance from these four hand-selected benchmark pairs. This is a descriptive comparison; the thesis's programmer-experiment data must be analyzed separately.
