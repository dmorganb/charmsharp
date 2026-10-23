# Charm# — a lightweight Charm++ source-to-source transpiler

Charm# is a C++20-based language extension that lets Charm++ programmers put
parallel object declarations, entry methods, messages, and Structured Dagger
(SDAG) control flow in a single `.csh` file. The transpiler generates ordinary
Charm++ `.ci` and C++ sources and delegates interface generation, compilation,
linking, and execution to the existing Charm++ toolchain.

Its goal is to reduce the amount of handwritten and duplicated source code
needed for Charm++ programs, while preserving ordinary C++ wherever possible.
Charm# is a **research prototype** for a thesis, not a replacement for Charm++
or a complete C++ parser. Its implementation uses a handwritten tokenizer,
a small structural parser, and source-range rewriting.

## Quick start

Requirements: a C++20 compiler and GNU Make. The transpiler and unit tests do
not require Charm++.

```sh
make
make test
```

Translate the executable Hello World example:

```sh
./build/charmsharp --module Hello --out-dir generated examples/Hello.csh
```

This produces `generated/Hello.ci` and `generated/Hello.cpp`. By default,
existing output files are protected; use `--force` to regenerate them.

With Charm++ installed and `charmc` on `PATH`, compile and run the result:

```sh
cd generated
charmc Hello.ci
charmc Hello.cpp -o Hello
./Hello +p2
```

A convenient end-to-end check from the repository root is `make charm-run`.
For a compile-only example, see `examples/Counter.csh` and `make charm-check`.

## A single-source chare

```cpp
chare Counter {
    int value = 0;
entry:
    Counter() {}
    void increment(int amount) { value += amount; }
};
```

The `entry:` section generates ordinary Charm++ interface declarations:

```cpp
module CounterModule {
    chare Counter {
        entry Counter();
        entry void increment(int amount);
    };
};
```

In the emitted C++, `chare` becomes a class inheriting from the generated
`CBase_Counter`, and `entry:` becomes public access. Ordinary C++ method
bodies remain in C++. The generated C++ includes the conventional
`CounterModule.decl.h` and `CounterModule.def.h` files created by Charm++.

## Implemented language features

These are validated **subsets**, not exhaustive compatibility with the
Charm++ interface language:

| Area | Supported Charm# forms |
| --- | --- |
| Parallel objects | `chare`, one `mainchare`, `group`, `nodegroup`, and array chares (1D and 2D exercised in CI) |
| Entries | `entry:` declarations and inline definitions, constructors and initializer lists, `void` methods, simple synchronous message-pointer returns |
| Entry attributes | `[[charm::threaded]]`, `[[charm::sync]]`, `[[charm::reductiontarget]]` |
| Messages and globals | Fixed-size `message` definitions, scalar and fixed-array `readonly` declarations |
| Inheritance | Restricted ordinary C++ bases for chares/messages; one previously defined local parallel chare base |
| Marshalled parameters | Simple `T data[n]` entry arrays: retained in `.ci`, emitted as `T* data` in C++ |
| SDAG | `when`, implicit/explicit `serial`, sequential waits, reference matching, `if`/`else`, `for`, `while`, `forall`, `overlap`, and `case` |
| Ordinary C++ | Kept through source-range rewriting outside recognized Charm# extensions |

SDAG bodies are emitted in the `.ci` source, and the generated C++ class
receives Charm++'s `_SDAG_CODE` glue. For example, the source of
[the Prefix Sum port](benchmarks/prefix-sum/charmsharp/prefix.csh) includes
an SDAG loop and a reference-matched `when` receive.

The precise syntax and restrictions are in the
[grammar](docs/charmsharp-grammar-v0.1.md),
[language coverage](docs/language-coverage.md), and
[declaration coverage audit](docs/declaration-coverage-audit.md).

## Benchmarks and validation

Four algorithms have **both** a pinned, unmodified upstream Charm++ reference
and a Charm# port:

| Algorithm | Main feature exercised | End-to-end command |
| --- | --- | --- |
| [Primes](benchmarks/primes/) | Mainchare, asynchronous entry messages | `make charm-benchmark-primes` |
| [Reduction](benchmarks/reduction/) | Array chares, reduction callback | `make charm-benchmark-reduction` |
| [Jacobi2D](benchmarks/jacobi2d/) | 2D arrays, marshalled halo exchanges | `make charm-benchmark-jacobi2d` |
| [Prefix Sum](benchmarks/prefix-sum/) | SDAG loop, conditional, tagged receives | `make charm-benchmark-prefix-sum` |

All four comparisons are run in GitHub Actions against **Charm++ v8.0.0**.
Tests compile and execute the original and transpiled implementations and
compare their numerical or observable results. Prefix Sum checks every
inclusive prefix for several array sizes and PE counts; its four-PE case
runs only on machines with at least four available cores.

Additional runtime integration fixtures compare SDAG behavior with
handwritten Charm++ for early and late messages, including `while`,
`forall`, `overlap`, and `case`.

The GitHub Actions job caches compiled Charm++ artifacts so normal runs need
not rebuild the toolchain. See [benchmark provenance and testing](benchmarks/README.md)
for upstream revision and test details.

## Known limitations

- The structural parser is intentionally **not** a full C++ parser or type
  checker. Charm++ and the C++ compiler still validate many declaration,
  serialization, and runtime rules.
- The `entry:` subset excludes arbitrary templates, default arguments,
  qualifiers, complex return types, and most Charm++ entry attributes.
  Namespaced parallel declarations and general multi-module dependency
  discovery are not implemented.
- Variable-size messages, complicated readonly types, and general C++
  inheritance resolution are outside the tested subset.
- SDAG receive bodies do not yet support nested SDAG waits or
  comma-separated multi-receive dependencies. For SDAG `for` and `forall`,
  indices that must survive suspension should be persistent chare fields.
  General cross-suspension variable-lifetime analysis is not implemented;
  see [the documented Charmxi limitation](docs/sdag-variable-lifetime.md).
- Preprocessor conditionals spanning Charm# structural declarations are
  rejected. Generated sources have no `#line` mapping or source maps yet.
- One module is generated per translation input; generated output is not
  a standalone compiler or runtime.

Unsupported constructs generally produce source-located diagnostics rather
than speculative translations.

## Thesis measurement work

The repository also contains an **early-stage**, separate Halstead metrics
instrument under `tools/charm-metrics/`. Its token classifier and draft
[classification policy](docs/halstead-classification-policy.md) exist, but
full program-level metrics, reporting and benchmark comparisons are **not yet
complete**. The planned measurement counts developer-authored `.ci` + C++
for Charm++ and the corresponding `.csh` for Charm#, never generated files.
See the [roadmap](docs/roadmap.md).

## Repository layout

```text
src/                    Charm# tokenizer, SDAG parser, translator, CLI
include/charmsharp/     Public translation API
examples/               Small runnable and compile-only Charm# programs
tests/                  C++ unit tests, CLI tests, Charm++ integration fixtures
benchmarks/             Four upstream Charm++/Charm# program pairs
tools/charm-metrics/     Separate experimental Halstead token classifier
docs/                   Grammar, coverage audit, roadmap, measurement policy
.github/workflows/      C++20 tests and cached Charm++ integration
```

## Design philosophy

Charm# is an **extension grammar** over C++, not a reimplementation of C++20.
The translator recognizes only the additional Charm# structure, retains
source byte ranges for ordinary C++, emits conventional Charm++ artifacts,
and relies on the existing toolchain. This makes each increment independently
testable against equivalent handwritten Charm++ code.
