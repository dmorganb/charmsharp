# Charm# — a lightweight Charm++ source-to-source transpiler

Charm# is a C++20-based language extension that lets Charm++ programmers put
parallel object declarations, entry methods, messages, and Structured Dagger
(SDAG) control flow in a single `.csh` file. The transpiler generates ordinary
Charm++ `.ci` and C++ sources and delegates interface generation, compilation,
linking, and execution to the existing Charm++ toolchain.

Its goal is to reduce the amount of handwritten and duplicated source code
needed for Charm++ programs, while preserving ordinary C++ wherever possible.
Charm# modernizes the Charm++ language frontend while retaining the existing
Charm++ compiler and runtime infrastructure.

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

Alternatively, translate and compile in one command (without running):

```sh
./build/charmsharp --build --out-dir generated examples/Hello.csh -o Hello
./Hello +p2
```

`--build` invokes `charmc` on the generated `.ci`, then on the `.cpp` to
compile and link. It requires `charmc` on `PATH` and stops if either step
fails, displaying the toolchain diagnostics. Without `--build`, translation
behaves as before; `-o` is only valid with `--build`.

Generated files remain in `--out-dir` (the current directory by default).
The executable defaults to the module name in the invoking directory;
`-o` paths are also relative to that directory. Use `--force` to overwrite
existing generated sources or an executable. The input source directory is
added to the C++ include search path for local headers. The build driver
uses POSIX process APIs (Linux/macOS).

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

## Compiler architecture

Charm# separates parsing and semantic analysis from output generation.
Its **internal** `SemanticModel` captures validated parallel declarations,
entry metadata, messages, readonly variables, marshalled parameters, and
analyzed SDAG, alongside byte ranges into the original `.csh` source.
Ordinary C++ stays opaque rather than being reconstructed from a partial C++
abstract syntax tree.

```text
Charm# source (.csh)
        |
        v
Tokenizer + structural/SDAG parsing and semantic analysis
        |
        v
    SemanticModel
        |
        +--------------------+
        |                    |
        v                    v
    CIEmitter             CppEmitter
        |                    |
        v                    v
  Charm++ .ci          Generated .cpp
        |                    |
        +---------+----------+
                  |
                  v
      Existing Charm++ toolchain
      (charmxi, C++ compiler, runtime)
```

- `src/translator.cpp` recognizes Charm# extensions and validates the
  supported subset before invoking either backend.
- `src/semantic.hpp` defines the model and source spans used by both emitters.
- `src/ci_emitter.cpp` renders Charm++ interface declarations and SDAG code
  without repeating frontend semantic analysis.
- `src/cpp_emitter.cpp` applies source-range edits for ordinary C++ classes,
  entry metadata, marshalled parameters, SDAG glue, and generated header
  inclusion. It does not reparse Charm# syntax.
- `charmsharp::translate()` remains the public translation API. The model and
  individual emitters are internal implementation details, not stable public
  interfaces.

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

Unit and CLI tests run with both GCC and Clang in GitHub Actions. A separate
Clang job runs them with AddressSanitizer, leak detection, and
UndefinedBehaviorSanitizer; sanitizer reports fail CI even when a CLI test
expects a nonzero exit status.

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

## Source complexity comparison

The standalone [Halstead analyzer](tools/charm-metrics/README.md) compares
source-level lexical complexity between Charm++ and Charm# programs, including
the four paired benchmarks.
It aggregates operator and operand frequencies across **developer-authored**
files before calculating Halstead vocabulary, length, volume, difficulty,
and effort. It excludes generated `.ci`, transpiled C++, and Charm++-generated
headers. Its [classification policy v0.2](docs/halstead-classification-policy.md)
documents token handling and two punctuation policies.

Build, test, and compare the benchmark sources from the repository root:

```sh
make -C tools/charm-metrics test
make -C tools/charm-metrics charm-metrics
tools/charm-metrics/charm-metrics benchmark --root benchmarks --format text
```

See the [benchmark results](docs/halstead-benchmark-results.md) for
the observed volume, difficulty, and effort differences, reproducible CI
artifacts, and the structural-punctuation sensitivity analysis. These
results describe source-level complexity for four paired programs; they do
not establish improvements in human comprehension time or observed defect rates.

## Repository layout

```text
src/                    Tokenizer, parsers, semantic model, .ci/C++ emitters, CLI
include/charmsharp/     Public translation API
examples/               Small runnable and compile-only Charm# programs
tests/                  C++ unit tests, CLI tests, Charm++ integration fixtures
benchmarks/             Four upstream Charm++/Charm# program pairs
tools/charm-metrics/     Standalone Halstead analyzer and benchmark CLI
docs/                   Grammar, coverage audit, roadmap, measurement policy
.github/workflows/      Unit tests, output-equivalence checks, Charm++ integration, metrics
```

## Future work

A possible backend could use Charm++'s XI-Builder facilities to construct an
XLAT-I representation directly from the semantic model instead of emitting
textual `.ci` files first. This integration has not been implemented or
validated. It would retain Charm++'s generated C++ bindings and existing
runtime.


