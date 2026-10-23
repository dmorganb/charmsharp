# Charm# — incremental prototype

Charm# is a lightweight C++20 source-to-source extension to Charm++. It aims to
reduce duplication between a C++ chare class and its `.ci` declaration, while
reusing `charmxi`, `charmc`, and the unmodified Charm++ runtime.

This is an incremental prototype, not a complete implementation of the
`charmsharp-grammar-v0.1.md` design draft. It supports multiple top-level
`chare` classes, one optional `mainchare`, `group`, `nodegroup`,
`array [ND] chare`, and methods under `entry:` with `threaded`/`sync` attributes;
ordinary class members and surrounding C++ are retained as source text.

## Build and test

Requirements: C++20 compiler and `make`. No Charm++ installation is needed for
the translator's unit tests.

```sh
make
make test
```

Translate the example (output files must not already exist):

```sh
./build/charmsharp --module CounterModule --out-dir generated examples/Counter.csh
```

This writes `generated/CounterModule.ci` and
`generated/CounterModule.cpp`. Re-running the command requires `--force` to
replace the generated files; source files are never overwritten.

If Charm++ is installed, compile with its existing tools:

```sh
cd generated
charmc CounterModule.ci
charmc -c CounterModule.cpp
```

`charmc` invokes the Charm++ interface translator and C++ compiler; Charm#
itself does not implement either. This produces an object for linking into an
ordinary Charm++ application. The Counter example is a compile-only chare component.

To run the compile-only integration check once `charmc` is available, use
`make charm-check`. For a full executable example with `mainchare` and a worker,
use `make charm-run`. Use `make charm-objects` for a program that creates a
group, a nodegroup, and chare arrays. Use `make charm-threaded` for a
threaded-entry execution test, which also compiles a synchronous entry method.
GitHub Actions performs all four checks against Charm++ v8.0.0.
Its integration job caches the compiled multicore toolchain with
`actions/cache@v4`; the first run builds the toolchain, and later cache hits
skip the checkout and build steps. The cache key pins the Charm++ version,
architecture, Ubuntu version, and build configuration.

## Phase 1 translation

```cpp
chare Counter {
private:
    int value = 0;
entry:
    Counter() {}
    void increment(int amount) { value += amount; }
};
```

Produces the interface:

```cpp
module CounterModule {
    chare Counter {
        entry Counter();
        entry void increment(int amount);
    };
};
```

The generated `.cpp` replaces `chare` with `class`, adds
`public CBase_Counter`, changes `entry:` to `public:`, and includes the
Charm++-generated `CounterModule.decl.h` and `CounterModule.def.h`.
The rest of the source is preserved byte-for-byte (apart from these replacements
and the two generated include lines).

## Language coverage

For an exact inventory of supported declarations, remaining Charm++ entry
attributes, return-type restrictions, messages/readonly declarations, and the
SDAG backlog, see [language coverage](docs/language-coverage.md). For milestone
sequencing see [the roadmap](docs/roadmap.md).

## Deliberate limitations

- Multiple global-scope `chare`, `group`, `nodegroup`, and `array [ND] chare`
  declarations and at most one `mainchare` are supported. Array dimensions
  are parsed structurally; the Charm++ toolchain validates supported sizes.
  Namespaced types, messages, readonly variables, inheritance, and SDAG remain
  unsupported.
- Entry signatures support constructors, `void` methods, and message-pointer
  returns for `[[charm::sync]]` entries. `[[charm::threaded]]` marks `void`
  methods; both attributes are removed from emitted C++ and lowered to `.ci`
  `[threaded]` / `[sync]`. Threaded constructors, combined attributes, and
  unknown attributes are rejected. Declarations and inline definitions work;
  out-of-line definitions remain untouched ordinary C++.
- Other entry attributes, qualifiers, default arguments, initializer lists, or
  templated entry methods are not yet supported. Parameter types themselves are preserved (including
  syntax the translator cannot validate), but Charm++ must accept their
  marshalling semantics.
- `entry:` must contain methods, not fields, aliases, or nested declarations.
  Ordinary `public:` does not register entries. Any access label closes the
  preceding `entry:` section.
- Comments, quoted literals, raw strings, and preprocessor directives are
  tokenized without treating their contents as Charm# syntax. Preprocessor
  conditionals spanning chare declarations/members are rejected. Macro-generated
  Charm# structural syntax is not supported.
- Source rewriting preserves original whitespace and code spans, but does not
  yet emit `#line` mappings or a source map. C++ and Charm++ compiler diagnostics
  may therefore refer to transformed line numbers.
- The translator validates structural syntax, not arbitrary C++ legality,
  Charm++ message serialization, or all possible entry signatures. Native
  compilers remain responsible for those checks.

Errors name the source location and explain the unsupported feature. There is
no attempt to guess a translation for deferred syntax.

## Layout

```text
include/charmsharp/translator.hpp   public translation API
src/lexer.{hpp,cpp}                 C++-aware tokenization and offsets
src/translator.cpp                  structural parser and range edits
src/main.cpp                        file-based CLI
tests/tests.cpp                     translation and diagnostic regressions
tests/cli_smoke.sh                  output and overwrite CLI test
tests/fake_charm/                  minimal headers for C++ syntax smoke test
tests/charm_smoke.sh                optional Charm++ compile check\ntests/charm_run.sh                  executable Charm++ test
examples/Counter.csh               compile-only input\nexamples/Hello.csh                 executable mainchare and worker\nexamples/Objects.csh               executable group, nodegroup and array\nexamples/Threaded.csh              threaded and sync entry declarations
docs/roadmap.md                     increment plan and acceptance criteria
```

## Design decisions

The grammar document is an **extension grammar**, not a full C++20 grammar.
For the first implementation, the parser recognizes only enough structure to
find top-level parallel types and the boundaries of their entry methods. Opaque C++ method bodies
are left alone. The lexer records offsets into the original input; the rewrite
pass applies only three kinds of edits: parallel type declarations → `class`, generated CBase
inheritance, and `entry:` → `public:`.

This keeps the implementation small while leaving room to introduce SDAG
recognition and `serial` segmentation as separately tested passes.
