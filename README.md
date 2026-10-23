# Charm# — phase 1 prototype

Charm# is a lightweight C++20 source-to-source extension to Charm++. It aims to
reduce duplication between a C++ chare class and its `.ci` declaration, while
reusing `charmxi`, `charmc`, and the unmodified Charm++ runtime.

This is **milestone 1**, not a complete implementation of the
`charmsharp-grammar-v0.1.md` design draft. The prototype supports exactly one
top-level `chare` class and simple methods under `entry:`; ordinary class
members and the surrounding C++ are retained as source text.

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
ordinary Charm++ application. The example is a chare component, not a
standalone executable (there is no `mainchare` yet).

To run that compile-only integration check automatically once `charmc` is
available, use `make charm-check`.

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

## Deliberate limitations

- Exactly one global-scope `chare`; no namespaces, `mainchare`, arrays, groups,
  nodegroups, messages, readonly variables, inheritance, or SDAG yet.
- Entry signatures must be constructors `Counter(...)` or `void method(...)`.
  Declarations and inline definitions work; out-of-line definitions remain
  untouched ordinary C++.
- No entry attributes, qualifiers, default arguments, initializer lists, or
  templated entry methods. Parameter types themselves are preserved (including
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
tests/charm_smoke.sh                optional Charm++ compile check
examples/Counter.csh               minimal input
docs/roadmap.md                     increment plan and acceptance criteria
```

## Design decisions

The grammar document is an **extension grammar**, not a full C++20 grammar.
For the first implementation, the parser recognizes only enough structure to
find one chare and the boundaries of its entry methods. Opaque C++ method bodies
are left alone. The lexer records offsets into the original input; the rewrite
pass applies only three kinds of edits: `chare` → `class`, generated CBase
inheritance, and `entry:` → `public:`.

This keeps the implementation small while leaving room to introduce SDAG
recognition and `serial` segmentation as separately tested passes.
