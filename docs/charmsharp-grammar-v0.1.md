# Charm# — Draft Grammar and Translation Specification

**Version:** 0.1 (design draft)  
**Status:** Design draft; a subset of declarations has been implemented and compiler-validated, but inheritance and SDAG remain proposed  
**Target:** The existing Charm++ interface translator (`charmxi`), native C++ compiler, and unmodified Charm++ runtime

## 1. Purpose and compatibility

Charm# (pronounced *Charm Sharp*) is a lightweight, source-to-source extension of Charm++. It moves parallel-object declarations, entry-method metadata, and SDAG source into a C++-like source representation. The front end generates conventional `.ci` and C++ code; existing Charm++ tools perform the real compilation.

All ordinary Charm++ project layouts (`.ci`, `.h`, `.C`/`.cpp`) remain supported through pass-through compilation. New Charm# source has the same execution semantics as the Charm++ program generated from it. The project accepts the minor source incompatibility caused by reserving additional keywords that may have been used as C++ identifiers. Charm# does **not** introduce tasks, coroutines, automatic parallelization, permanent joint-entry rules, or a new runtime.

The EBNF below formally describes the **extension layer**, not the complete ISO C++ grammar or all of Charm++'s existing `.ci` grammar. The `CPP_*` terminals are delegation boundaries with separately specified recognition requirements. This matters: it would be misleading to call this an independently complete grammar for arbitrary modern C++.

## 2. Notation and lexical contract

- `=` defines a production; `|` is alternation; `[ ... ]` is optional; `{ ... }` is repetition; `(...)` groups alternatives; quoted text is a literal token.
- `IDENT`, `INTEGER`, `CPP_DECL`, `CPP_STATEMENT`, `CPP_EXPRESSION`, `CPP_TYPE`, and `CPP_PARAMETER_LIST` are abstract terminals. Their contents are accepted/preserved by the C++ frontend or an opaque source-span recognizer, not parsed from this EBNF alone.
- `CPP_STATEMENT` must be a **single balanced C++ statement**, including its necessary trailing semicolon or braced body. `CPP_DECL` must be a **single balanced C++ member declaration or definition**.
- The structural recognizer must lexically distinguish comments, string/character/raw-string literals, preprocessor directives, parentheses, brackets, and braces. It cannot find method boundaries by counting `{` and `}` in raw bytes.
- At the first implementation stage, preprocessor macros that expand into Charm# structural syntax are unsupported. Existing macros in ordinary C++ bodies should be passed through wherever possible. Conditional compilation spanning the delimiters of a Charm# structural construct is outside v0.1.
- Candidate new reserved tokens in Charm# source: `chare`, `mainchare`, `entry`, `message`, `group`, `nodegroup`, `readonly`, `when`, `overlap`, `case`. The identifiers `array` and `D` also have grammatical roles in array declarations. A final lexer specification must settle whether those are globally reserved or context-recognized; the implementation may reserve them in v0.1.

## 3. Extension EBNF

```ebnf
charmsharp-unit        = { top-level-item } ;
top-level-item         = cpp-top-level | parallel-type | message-type
                       | readonly-declaration ;
cpp-top-level          = CPP_TOP_LEVEL ;

parallel-type          = parallel-kind, IDENT, [ inheritance-clause ],
                         "{", { member-item }, "}", ";" ;
parallel-kind          = "chare" | "mainchare"
                       | "array", "[", dimension, "]", "chare"
                       | "group" | "nodegroup" ;
dimension              = INTEGER, "D" ;
inheritance-clause     = ":", CPP_BASE_SPECIFIER_LIST ;

member-item            = access-label | entry-method | cpp-member ;
access-label           = "private", ":" | "protected", ":"
                       | "public", ":" | "entry", ":" ;
cpp-member             = CPP_DECL ;
entry-method           = { entry-attribute }, CPP_METHOD_DECL_OR_DEF ;
entry-attribute        = "[[", "charm", "::", IDENT,
                         { ",", CPP_ATTRIBUTE_ARGUMENT }, "]]" ;

message-type           = "message", IDENT, [ inheritance-clause ],
                         "{", { cpp-member }, "}", ";" ;
readonly-declaration   = "readonly", CPP_VARIABLE_DECL, ";" ;

(* A method body recognized as SDAG-aware uses the following grammar.
   A method without SDAG constructs has an ordinary C++ body. *)
sdag-method-body       = "{", { sdag-statement }, "}" ;
sdag-statement         = ordinary-run | when-statement | overlap-statement
                       | case-statement | sdag-if | sdag-for
                       | sdag-while | sdag-forall ;
ordinary-run           = CPP_STATEMENT, { CPP_STATEMENT } ;

when-statement         = "when", receive-signature,
                         { ",", receive-signature }, sdag-method-body ;
receive-signature      = IDENT, [ "[", CPP_EXPRESSION, "]" ],
                         "(", [ CPP_PARAMETER_LIST ], ")" ;

overlap-statement      = "overlap", "{", { sdag-statement }, "}" ;
case-statement         = "case", "{", when-statement,
                         { when-statement }, "}" ;

sdag-if                = "if", "(", CPP_EXPRESSION, ")",
                         sdag-statement, [ "else", sdag-statement ] ;
sdag-for               = "for", "(", CPP_FOR_CLAUSES, ")",
                         sdag-statement ;
sdag-while             = "while", "(", CPP_EXPRESSION, ")",
                         sdag-statement ;
sdag-forall            = "forall", "[", IDENT, "]", "(",
                         CPP_EXPRESSION, ":", CPP_EXPRESSION, ",",
                         CPP_EXPRESSION, ")", sdag-statement ;
```

### Context rules not expressible by the EBNF alone

1. **Entry section.** `entry:` is only valid in a parallel type, and remains in effect until the next access label. It implies ordinary C++ `public:` and remote entry registration for eligible functions. Fields, type aliases, and nested types are rejected in that section in v0.1. A function body is optional: declarations may be defined out of line in ordinary C++.
2. **Parallel types.** Each parallel type lowers to an ordinary C++ class with the appropriate generated `CBase_Name` base relationship, and a matching `.ci` declaration. User-provided inheritance follows the classification and lowering rules in Section 3.1; never silently discard bases. Array dimensionality is an index-shape declaration, not an instance size.
3. **Entry attributes.** `[[charm::threaded]]`, `[[charm::sync]]`, and other supported attributes lower to Charm++ `.ci` entry attributes. Attribute spelling, argument forms, and applicable combinations must follow the target Charm++ version. An attribute never invents new runtime behavior. No `entry [threaded]:` section syntax is included in v0.1.
4. **Message and readonly declarations.** They lower to the corresponding Charm++ interface metadata and normal C++ definitions. `readonly` has Charm++ initialization semantics, not ordinary C++ `const` or `constexpr` semantics. Advanced variable-size message layouts are deferred.
5. **Implicit receive declarations.** A `when` references one or more entry points. If an equivalent explicit declaration is absent, Charm# generates the corresponding `.ci` entry declaration. An explicit declaration is permitted and validated for signature compatibility; a separate C++ implementation for a `when`-managed entry point is rejected. A receive signature is **not** an ordinary function definition.
6. **SDAG detection.** A method body containing a structurally recognized `when`, `overlap`, `case`, or `forall` is treated as SDAG-aware. Ordinary C++ methods remain ordinary C++ methods. A robust implementation must reject ambiguous/mixed constructs rather than guess.
7. **Implicit `serial`.** Each maximal eligible contiguous run of ordinary C++ statements inside an SDAG-aware method becomes one `serial { ... }` block in generated `.ci`. A run must not cross an SDAG control-flow construct. C++ text inside these runs is preserved. The grammar above is an *abstract* source grammar: correctly identifying the maximal runs in all C++ control-flow configurations requires a stated recognition subset and tests.
8. **Variable lifetime.** Variables whose values must live across a `when` suspension must already be members of the chare (or otherwise persistent values supported by Charm++). Charm# v0.1 does not lift C++ locals into coroutine frames or implicitly promote them into class fields.
9. **Matching.** `when receive[ref](...)` keeps Charm++'s existing reference-number semantics. No implicit correlation to the sender of a preceding proxy invocation. `overlap`, `case`, loops, and nested waits preserve SDAG semantics, including message buffering and matching rules.
10. **Pass-through.** A project may still contain existing `.ci` files. Their contents are not reinterpreted by Charm#; generated modules must avoid collisions with those existing declarations.

### 3.1 Inheritance semantics (proposed v0.1 rules)

Charm# reuses C++ base-specifier syntax, but a base may be either an **ordinary C++ type** or a **Charm++ parallel type**. This distinction matters because inheriting an entry method from a parallel base must also be represented to `charmxi` so the derived proxy exposes it. The generated `CBase_Derived` supplies the C++ inheritance relationship for parallel bases: repeating a parallel base in the C++ base list would risk redundant or ambiguous inheritance.

1. **Classify each explicitly named base before lowering.** A base that resolves to a known Charm# parallel type is a *parallel base*. A base positively identified as a normal C++ type is an *ordinary base*. Resolve declarations regardless of source order, including parallel types declared later in the same translation unit. Do not classify a base as ordinary merely because its name is absent from the local parallel-type table: it could be an external Charm++ type.
2. **Ordinary C++ bases.** Preserve the original C++ base specifiers (including access and permitted C++ syntax), after inserting the generated `public CBase_Derived` first. Ordinary bases do **not** appear in the generated `.ci` inheritance clause.
3. **Parallel bases.** Emit the supported parallel-base relationship in the generated `.ci` declaration. Omit that base from the final C++ class base-specifier list, where `CBase_Derived` already represents it. Charm++ `.ci` inheritance is implicitly public, so a parallel base specified as `private` or `protected` in Charm# is rejected rather than silently changed.
4. **Mixed bases.** Apply both transformations independently. For example, given a known parallel `Worker` and an ordinary `Logging` class, `chare SpecializedWorker : public Worker, public Logging` lowers to `.ci` `chare SpecializedWorker : Worker` and C++ `class SpecializedWorker : public CBase_SpecializedWorker, public Logging`.
5. **Classification across files/modules.** A future module/import description must explicitly identify external parallel types and their corresponding Charm++ modules. External ordinary bases must likewise be recognizable without needing to parse arbitrary included C++ headers. **Until such metadata or another unambiguous classification mechanism exists, unresolved base types are unsupported and produce a diagnostic.** This is a deliberate v0.1 restriction, not a claim that external C++ inheritance is illegal.
6. **No speculative lowering.** Ambiguous, conflicting, cyclic, or unsupported inheritance graphs produce a diagnostic. Do not silently drop a base or assume a parallel-base relationship based on naming conventions.

Example with an ordinary C++ base:

```cpp
class Logging { /* application-defined operations */ };
chare Worker : public Logging {
entry:
    Worker();
};
```

The `.ci` declaration is `chare Worker { entry Worker(); };`, while generated C++ has `class Worker : public CBase_Worker, public Logging`.

Example with a parallel base and an ordinary base:

```cpp
chare BaseWorker {
entry:
    BaseWorker();
    void process();
};
class Logging { /* ordinary C++ */ };
chare SpecializedWorker : public BaseWorker, public Logging {
entry:
    SpecializedWorker();
    void specializedProcess();
};
```

The generated `.ci` uses `chare SpecializedWorker : BaseWorker`, while the generated C++ inherits from `CBase_SpecializedWorker` and `Logging`. The Charm++ generated proxy for `SpecializedWorker` can then expose inherited `BaseWorker` entry methods, subject to the existing Charm++ inheritance rules.

**Initial supported subset and open questions:** Accept at most one parallel base per parallel type until multiple parallel inheritance is proven supported by the pinned Charm++ version. Multiple ordinary C++ bases may be kept, but name resolution and their C++ legality remain an explicit validation boundary. Virtual inheritance, access subtleties, dependent/template bases, and cross-module inheritance require targeted design and tests. Preserve or correctly translate constructor forwarding through generated `CBase_` constructors; do not silently discard initializer lists. Message-type inheritance must be handled separately because Charm++ permits a message to inherit from an ordinary C++ class but not another Charm++ message. These rules remain **design proposals**, not implemented behavior, until validated by compile/run tests.

## 4. Illustrative translations

### 4.1 Ordinary chare and entry access

Charm# input:

```cpp
chare Counter {
private:
    int value = 0;
public:
    int localValue() const { return value; }
entry:
    Counter();
    void increment(int amount) { value += amount; }
};
```

Generated `.ci` (module name supplied by build/module configuration):

```cpp
module CounterModule {
    chare Counter {
        entry Counter();
        entry void increment(int amount);
    };
};
```

Generated C++ class (schematic; required `.decl.h` / `.def.h` inclusion and declaration ordering are handled by the code generator):

```cpp
class Counter : public CBase_Counter {
private:
    int value = 0;
public:
    int localValue() const { return value; }
public:
    Counter();
    void increment(int amount) { value += amount; }
};
```

### 4.2 Array chare

```cpp
array [1D] chare Particle {
entry:
    Particle();
    void update(double dt);
};
```

Lowers to `.ci` `array [1D] Particle { entry Particle(); entry void update(double dt); };` and an ordinary `class Particle : public CBase_Particle` declaration.

### 4.3 Implicit SDAG `serial` and explicit `when` / `overlap`

Charm# input:

```cpp
chare Coordinator {
entry:
    void start() {
        workerA.request(thisProxy);
        workerB.request(thisProxy);

        overlap {
            when receiveA(int a) { processA(a); }
            when receiveB(int b) { processB(b); }
        }

        continueWork();
    }
};
```

Generated `.ci` SDAG fragment (plus inferred `receiveA` and `receiveB` declarations):

```cpp
entry void start() {
    serial {
        workerA.request(thisProxy);
        workerB.request(thisProxy);
    }
    overlap {
        when receiveA(int a) serial { processA(a); }
        when receiveB(int b) serial { processB(b); }
    }
    serial { continueWork(); }
};
entry void receiveA(int a);
entry void receiveB(int b);
```

The generated C++ class contains appropriate SDAG code hooks, not another user-written `start()` implementation. The existing Charm++ translator generates its state machine. In this example, `continueWork()` occurs after both branches complete; `processA` and `processB` may be triggered in either order.

### 4.4 Reference-matched iterations

```cpp
array [1D] chare Worker {
private:
    int round = 0;
    double total = 0;
entry:
    void run() {
        for (round = 0; round < 10; ++round) {
            neighbor.request(round, thisProxy);
            when result[round](int id, double value) {
                total += value;
            }
        }
        finish(total);
    }
};
```

This lowers to an SDAG `for` containing a `serial` send and a reference-number-matched `when`, followed by a `serial` completion. Both `round` and `total` are persistent chare fields rather than short-lived method locals.

## 5. Deliberately deferred and unresolved

These issues should be resolved before claiming a complete normative grammar or implementing broad C++ compatibility:

1. **Precise C++ declaration recognizer.** How to extract signatures with templates, trailing return types, attributes, operators, constraints, default arguments, macros, and qualified names; how much to delegate to a C++ parser.
2. **Preprocessing order.** Which source regions are transformed before/after the C preprocessor, and how diagnostic source locations are preserved.
3. **Modules and headers.** Naming of generated `.ci` modules, `mainmodule` selection, include placement, namespace handling, multiple chares per file, and separate compilation.
4. **SDAG scope and C++ flow.** A formal permitted subset for ordinary statements around `when`; how C++ `if`, `for`, `while`, braces, declarations, `return`, and exceptions interact with implicit `serial`. The prototype should reject unsupported cross-suspension control flow.
5. **Entry-method legality.** Enforcement (or pass-through diagnostics) for constructor signatures, marshallable parameters, entry return types, message classes, migration support, and allowable attributes.
6. **Chare inheritance.** Implement and validate the Section 3.1 rules, including classification of external bases, constructor forwarding, and Charm++ proxy inheritance; currently deferred.
7. **Complete `.ci` coverage.** Advanced message declarations, `PUPable`, initialization hooks, namespaces/modules, specialized entry attributes, and experimental features are available through legacy `.ci` pass-through but do not yet have native Charm# syntax in this draft.
8. **Token compatibility.** Whether reserved words are global or structural-context-specific, and the migration diagnostics for collisions.

## 6. Suggested conformance tests

- Translate a basic chare and an array chare; compare generated `.ci` signatures and compile/run against a pinned Charm++ version.
- Verify `entry:` implies public C++ access and produces proxy entry methods; ordinary `public:` does not.
- Verify an explicit declaration corresponding to an implicit `when` declaration is accepted exactly once; incompatible signatures and duplicate implementations are rejected.
- Verify implicit `serial` segmentation for sends before a `when`, bodies of `when`, `overlap` branches, and code after synchronization.
- Verify `case`, nested `if`/loops, early message arrival, out-of-order delivery, and reference-number matching behave like equivalent handwritten SDAG.
- Verify cross-`when` local variables fail with an actionable diagnostic in v0.1, while chare fields compile.
- Verify ordinary Charm++ projects without Charm# syntax can be passed unchanged to the original toolchain.

## 7. Design summary

Charm# is a small extension surface over the established Charm++ compiler/runtime: `chare`-like declarations, an `entry:` access section, integrated interface metadata, and SDAG constructs embedded in method bodies with *implicit `serial`*. Its research promise is reduced duplication and increased readability **without changing runtime or SDAG behavior**. The joint-entry / persistent join-pattern proposal remains explicitly outside v0.1.
