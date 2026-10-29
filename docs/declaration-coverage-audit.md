# Phase 3.4 — Charm++ interface declaration coverage audit

Status: 2026-10-23. Baseline: **Charm++ v8.0.0** and Charm# grammar v0.1.
This is a **documentation audit**, not an implementation milestone or a claim
of full Charm++ compatibility.

## Method and status definitions

Compared the pinned [charmxi grammar](https://github.com/charmplusplus/charm/blob/v8.0.0/src/xlat-i/xi-grammar.y),
the [Charm++ manual](https://charm.readthedocs.io/en/latest/charm%2B%2B/manual.html),
the Charm# [design grammar](charmsharp-grammar-v0.1.md), the translator, unit
tests, and executable examples invoked by [CI](../.github/workflows/ci.yml).

- **Runtime-validated subset (R):** generated output for at least one representative
  form is compiled and executed with pinned Charm++ v8.0.0 in CI.
- **Translation tested (T):** generated text is checked by unit tests, but the
  *particular variant* has not been executed through Charm++.
- **Deferred (D):** no native Charm# lowering; in several cases the translator
  explicitly rejects the syntax.
- **Pass-through (P):** ordinary C++ is copied as text. This does **not** imply
  an equivalent Charm++ `.ci` declaration is synthesized.

Charm#'s v0.1 grammar is intentionally an extension grammar, not the full
`.ci` grammar. “R” never means every valid C++ signature or arrangement is
supported.

## Entity and storage declarations

| Charm++ `.ci` family | Charm# status | Current boundary / missing form |
| --- | --- | --- |
| `module`, `mainmodule` | **R, limited** | One generated module per input; automatic `mainmodule` if it contains a `mainchare`. No explicit multi-module syntax or reachability graph. |
| `chare`, `mainchare` | **R** | Simple top-level classes, one mainchare. |
| `group`, `nodegroup` | **R** | Basic constructors and entries; specialized flags not generated. |
| `array [1D]`, `array [2D]` | **R** | Other numeric dimensions lexed, but not individually integration-tested; named index types unsupported. |
| `message M;` (fixed-size) | **R, limited** | `message M { ... };` emits `message M;` plus C++ `CMessage_M` class; no field-level serialization/lifetime proof. |
| Variable-size `message` with `T field[]` | **R, limited (7.1)** | Restricted primitive member lowering, ordered `.ci` metadata and C++ pointers; pinned v8.0.0 runtime validated with single/two arrays, zero/unequal lengths, remote delivery/cleanup, and handwritten reference. |
| Message attributes `[packed]`, `[varsize]`; `conditional` fields | **D** | Present in pinned parser; `packed`/`varsize` are parsed as compatibility no-ops in v8.0.0 grammar. Not a reason to implement them early. |
| `readonly Type x;` and `readonly Type a[N];` | **R, limited** | Single-token types; decimal fixed bounds; runtime initialization in mainchare. Complex types, multidimensional bounds and declaration initializers unsupported. |
| `readonly message M *x;` | **D** | Requires explicit message-pointer handling and initialization on PEs. |
| Chare inheritance: ordinary C++ bases | **R, limited** | Simple, earlier-defined local `class`/`struct`, multiple bases/access labels; external and dependent bases deferred. |
| Chare inheritance: `.ci` parallel base | **R, limited** | One explicitly public, earlier-defined local `chare` base; inherited proxy call tested. No multi-base parallel hierarchy, forward/cross-file resolution, or other parallel kinds. |
| Message inheritance: ordinary C++ bases | **R, limited** | One or more simple earlier-defined ordinary bases; message-to-message inheritance rejected. |
| Scalar `template<typename T> chare Name` and concrete registrations | **R, limited (Phase 6.3)** | One type parameter; tested separate registration module, shared `<module>_templates.h`, proxy inference, multiple specializations and cross-TU consumer. See [template audit](chare-template-coverage-audit.md). Other templated parallel kinds and messages remain deferred. |
| `namespace`, `using`, `using namespace` in `.ci` | **D** | Charm# parallel declarations inside namespaces are rejected; ordinary C++ namespace code is only passed through. |
| `class Name;` in `.ci` | **D** | Explicit interface forward declaration distinct from ordinary C++ `class`. |
| `extern module`, `extern` entity/entry, `include` | **D** | Multi-module generated-header dependencies, reachable module registration, externally declared entries. |
| `PUPable Class;` | **D** | Runtime serialization polymorphism/registration; should integrate with existing `PUPable_decl` C++ conventions. |
| `initnode`, `initproc` | **D** | Global and class-static startup registration hooks; `initcall` is deprecated. |
| Chare attributes `[migratable]`, `[python]`; array `[python]` | **D** | Pinned grammar recognizes these; placement/semantics need validation. |
| Preprocessor-conditionals around interface declarations | **D** | Charm# rejects conditional parallel declarations; ordinary C++ directives remain pass-through. |

A message's fixed-size classification is **not proven by its current parser**:
the scanner explicitly rejects unsized `[]`, but arbitrary copied C++ fields
could still require custom serialization or be semantically invalid. Likewise,
readonly variable assignment restrictions are mostly enforced by Charm++ usage
conventions rather than static Charm# analysis.

## Entry declarations, attributes and signatures

| Area | Status | Missing / limitation |
| --- | --- | --- |
| `entry` constructors and ordinary `void` methods | **R, limited** | Simple names, text-copied parameters, declaration or inline definition. |
| `[threaded]` and `[sync]` | **R, limited** | `[[charm::threaded]]`; `[[charm::sync]]` with simple pointer return; additional combinations and rich return types deferred. |
| `[exclusive]`, `[nokeep]`, `[notrace]`, `[appwork]` | **D** | Concurrency, ownership, instrumentation semantics. |
| `[immediate]`, `[skipsched]`, `[inline]`, `[local]` | **D** | Scheduling and local-call rules; `local` permits different return-type semantics. |
| `[reductiontarget]`, `[aggregate]`, `[whenidle]` | **D** | Reduction/aggregation, special idle callback signature. |
| `[iget]`, `[createhere]`, `[createhome]`, `[memcritical]`, `[python]` | **D** | Explicit tokens in v8.0.0 grammar; specialized semantics. |
| `[expedited]` | **Unverified in pinned parser** | Mentioned in current manual, but not listed among v8.0.0 `EAttrib` grammar alternatives; test support against pinned charmxi before claiming it. |
| `[stacksize]`, `[virtual]`/advanced entry syntax | **D** | Supported parser spellings and legal combinations require a dedicated conformance fixture. |
| Advanced method signatures and return types | **D** | Qualified/template/trailing returns, defaults, ctor initializer lists, local non-void, synchronous serializable values, overload equivalence. |
| Parameter marshalling/PUP and message lifetime validation | **D** | Syntax is passed through; generated Charm++ code/compiler decides validity. |

The pinned parser's `EAttrib` alternatives include: `threaded`,
`whenidle`, `sync`, `iget`, `exclusive`, `createhere`, `createhome`,
`nokeep`, `notrace`, `appwork`, `immediate`, `skipsched`, `inline`,
`local`, `python`, `memcritical`, `reductiontarget`, and `aggregate`.
This list is an inventory of parser productions—not a claim that every
combination is supported or advisable.

## Structural SDAG declarations (separate language phase)

**D** for native Charm# lowering of `when`, `serial`, `overlap`,
`case`, `for`, `while`, `forall`, conditional SDAG control flow,
reference-matched receives, and implicit receive-entry declarations.

These constructs are in Charm# v0.1's design, but deliberately outside Phase
3 declaration lowering. Their sequencing and local-variable lifetime rules
are the main research/implementation challenge in Phase 4 and beyond.

## Prioritized follow-up: coverage versus thesis goals

| Priority | Item | Why / proposed acceptance |
| --- | --- | --- |
| **P0 — before claiming language coverage** | Keep docs, examples and negative diagnostics aligned; add an audit/checklist to PR reviews | Existing docs had become stale as implementation advanced; no feature implementation required. |
| **P1 — cheap, useful declarations** | `initnode`/`initproc` hooks and selected `[[charm::...]]` attributes | Common interface boilerplate. Each needs generated `.ci`, negative tests, and a real charmc compile/run or startup-order assertion. |
| **P1 — correctness-sensitive** | `readonly message` and variable-size messages | Deserve separate allocations, serialization, and multi-PE tests; do not infer correctness from text rewriting. |
| **P1 — architecture** | `extern module`, `include`, multi-file builds and source mapping | Necessary for real projects and LSP diagnostics, but substantially larger than one-declaration lowering. |
| **P2 — specialized** | `PUPable`, template entities, additional chare attributes and namespace support | Implement when benchmarks or reference applications demonstrate demand. |
| **P3 — niche/compatibility** | Python/accelerator facilities and deprecated `initcall` or message modifier tokens | Defer unless study corpus or toolchain tests show need. |

**Historical Phase 3 recommendation:** close Phase 3 as a **common-declarations subset**, not a
complete translation of all `.ci` syntax. Start the agreed SDAG work next,
while treating P1 declaration gaps as narrowly scoped follow-up PRs. Use the
four benchmark implementations to drive which gaps actually block the thesis
experiments; that evidence is more useful than syntactic parity for its own sake.

## Sources

1. [Charm++ v8.0.0 charmxi grammar — authoritative parser productions](https://github.com/charmplusplus/charm/blob/v8.0.0/src/xlat-i/xi-grammar.y)
2. [Charm++ language manual](https://charm.readthedocs.io/en/latest/charm%2B%2B/manual.html)
3. [Charm# implementation coverage](language-coverage.md), [roadmap](roadmap.md)
   and [design grammar](charmsharp-grammar-v0.1.md)

## Phase 6.3 follow-up (2026-10-29)

The template row above is updated from the original Phase 3 snapshot. Scalar
chare template support is implemented and pinned-toolchain tested, but it is
not equivalent to complete Charm++ template compatibility. Explicit complex
argument strings are parsed structurally; default/multiple/non-type template
parameters, other template entity kinds, inheritance and whole-program proxy
usage inference remain deferred. See the [Phase 6.3.4e audit](chare-template-coverage-audit.md).
