# Incremental implementation roadmap

This roadmap tracks the implementation against `charmsharp-grammar-v0.1.md`.
See [language coverage](language-coverage.md) and the [Phase 3.4 audit](declaration-coverage-audit.md) for supported syntax,
unsupported entry attributes, return types, and remaining declaration features.

| Milestone | Status | Scope / acceptance |
| --- | --- | --- |
| Phase 1 | Complete | Handwritten lexer and structural parser; source-range edits; one chare, entry declarations, `.ci` and C++ emission; regression tests |
| 2.1 / 2.2 | Complete | Multiple chares and `mainchare`/`mainmodule`; runnable Hello World |
| 2.3 | Complete (basic forms) | Groups, nodegroups, 1D and 2D arrays compiled and exercised against Charm++ |
| 2.4 | Initial subset complete | `threaded` and `sync` attributes; `void` and simple synchronous message-pointer returns |
| 2.5 | Complete as infrastructure | Charm++ v8.0.0 pinned integration: compile, link and execute programs on GitHub Actions, with cached toolchain |
| 3.1 | Complete | Fixed-size `message` declarations and executable send/receive integration |
| 3.2 | Complete | `readonly` global declarations and initialization semantics |
| 3.3.1 | Complete | Ordinary C++ inheritance with local base resolution, multiple bases, and Charm++ integration |
| 3.3.2 | Complete | Message inheritance, separately validated against Charm++ |
| 3.3.3 | Complete | Local public chare-to-chare inheritance and proxy entry forwarding; cross-file and other base forms deferred |
| 3.4 | Complete (audit) | Version-pinned declaration coverage matrix, remaining constructs, and prioritized backlog; no new lowering |
| 4.0 | Complete | Port Primes to Charm# and compare compiled output against unmodified Charm++ reference in CI |
| 4.1 | Complete | Support `[[charm::reductiontarget]]` and port Reduction; compare runtime results |
| 4.2 | Complete | Structurally parse constructor initializer lists and retain them verbatim in C++ while emitting signature-only `.ci` entries; validate with Charm++ runtime |
| 4.3 | Complete | Implement entry `T data[n]` marshalling syntax: preserve bounds in `.ci`, lower C++ signatures to `T* data`, validate named/literal bounds and remote copying; port Jacobi2D |
| 5.0 | Complete | Recognize SDAG bodies and build source-ranged structural nodes; no SDAG emission |
| 5.1 | Complete (single-when slice) | Single `when`, receive entry registration, SDAG C++ glue, and runtime comparison |
| 5.2 | Complete (sequential subset) | Implicit `serial` segmentation and statement-boundary preservation |
| 5.3 | Complete (if/for/ref subset) | SDAG `if`, `for`, reference-matched `when` and asynchronous behavior tests |
| 5.4 | Complete | Port Prefix Sum, verify every expected prefix with varying PEs and message arrival orders |
| 5.5 | Complete (restricted subset) | Extend to `overlap`, `case`, `while`, `forall`, and lifetime diagnostics |
| 6.1 | Complete (restricted convention) | Infer scalar `chare Main` as `mainchare`; preserve explicit declarations and diagnose conflicting mains; executable Charm++ test |
| 6.2 | Complete (restricted subset) | Added `notrace`, `appwork`, `nokeep`, `exclusive`, `inline`, `local` with conservative legality checks and executable pinned-runtime tests |
| 6.3 | Complete (restricted scalar-chare subset; 6.3.4e audit) | One type parameter, explicit and inferred concrete registrations, multiple specializations, generated shared header, cross-TU executable tests; [limitations documented](chare-template-coverage-audit.md) |
| 7.1 | Complete (restricted primitive-array subset) | Restricted variable-size message fields, CI metadata and pointer lowering, pinned-runtime validation |
| 7.2 | Planned | Readonly message pointers and multi-PE initialization |
| 7.3 | Planned | `initnode` and `initproc` initialization hooks |
| 7.4 | Planned | Restricted PUPable registration after v8.0.0 semantics audit |
| Later | Planned | Richer entries, multi-file/module integration, source mapping |

## Fixed-size message slice (phase 3.1)

1. Lower top-level `message Name { ... };` to `message Name;` in `.ci` and
   `class Name : public CMessage_Name { ... };` in C++.
2. Preserve fixed-size fields and ordinary member methods; reject unsized
   arrays. Ordinary message inheritance is now supported for local simple bases.
3. Compile and execute a program transferring a custom message in CI.

## Readonly globals (phase 3.2)

1. Lower `readonly Type name;` and `readonly Type name[N];` into the corresponding
   `.ci` metadata while preserving ordinary global C++ definitions.
2. Keep initialization explicit in the mainchare constructor: Charm++ broadcasts
   assigned values to processing elements during startup.
3. Reject pointers, initializers and multi-token types until a later slice.

## Parallel inheritance slice (phase 3.3.3)

- Support one previously defined local `chare` base in `: public Base`, emit
  `chare Derived : Base` in `.ci`, and omit the parallel base from generated C++
  because it is already represented by `CBase_Derived`.
- Allow mixed ordinary bases and validate inherited proxy method calls in CI.
- Reject forward-defined parallel bases, more than one parallel base, other
  parallel type kinds, and private/protected parallel inheritance until their
  semantics and C++ ordering are addressed.

## Marshalled array parameter slice (phase 4.3)

1. Recognize simple `T data[n]` parameter declarators in Charm# entry method declarations and definitions; limit supported array-bound expressions to those accepted by pinned Charmxi.
2. Generate `.ci` entry parameters with the original `data[n]` length metadata and ordinary C++ pointer parameters (`T* data`) for the method implementation. Preserve other parameter types and method bodies. Rewrite matching declarations/definitions consistently.
3. Do not infer serialization lengths from bare pointers; reject unsupported or ambiguous array declarators rather than silently emitting incorrect marshalling.
4. Validate named and literal array bounds, invoke the entry through a proxy, and compare transferred element values against a handwritten Charm++ fixture before porting Jacobi2D. Keep `std::span` and a dedicated length attribute outside v0.1.

## Phase 5: SDAG slices

1. **5.0:** Introduce a separate structural SDAG parser that distinguishes ordinary
   C++ from SDAG-specific forms and retains byte ranges. It initially provided
   diagnostics only; the parser now feeds the limited Phase 5.1 lowering.
2. **5.1:** Lower a single `when` into conventional Charm++ SDAG `.ci`, add
   implicit or validated explicit receive entry declarations, and emit the
   Charm++ `_SDAG_CODE` C++ glue. Test message arrival before/after a wait.
3. **5.2:** Coalesce safe ordinary statement runs into `serial` blocks,
   preserve original C++ source ranges, and reject unsafe scope crossings.
4. **5.3:** Support SDAG `if` and `for` plus explicit reference matching.
   Test buffered, out-of-order and repeated deliveries across loop iterations.
5. **5.4:** Port the pinned Prefix Sum benchmark; validate all expected prefix
   values rather than relying on asynchronous output line ordering.
6. **5.5:** Lower SDAG `overlap`, `case`, `while` and `forall` recursively;
   compare their generated `.ci` and runtime behavior against handwritten
   Charm++ on both early and late message arrivals. Cross-suspension variable
   lifetime analysis remains future work.

Every emitting slice must compare its `.ci` structure to a handwritten
Charm++ fixture and compile and execute through the pinned Charm++ CI toolchain.

## SDAG suspension and variable lifetime

Phase 5.3 exposed a limitation in the underlying Charmxi-generated continuation
methods: a loop-local index declared as `for (int step = ... )` may not remain
in scope after a `when`. The handwritten Charm++ reference produced the same
compilation error. Charm# v0.1 uses a persistent chare field instead and
conservatively rejects common unsupported loop-local declarations.

The workaround is not per-invocation storage: overlapping SDAG invocations can
conflict if they mutate the same chare field. For the observed error, tested
workaround, concurrency tradeoffs, and possible future compiler enhancement,
see [SDAG variable lifetime](sdag-variable-lifetime.md).

## Known parser boundaries

- Lexical balance is not a full C++ parser; templates, lambdas, requires
  clauses, macros and qualified signatures need targeted tests.
- Conditional preprocessing of parallel declarations is rejected.
- Diagnostics use source offsets but emitted sources still lack `#line`
  mapping or a dedicated source map.
- Marshalled parameter legality and valid PUP/message semantics remain
  responsibilities of Charm++ until semantic validation is explicitly added.

### Phase 5.1 supported subset

- A `void` entry method body consisting of exactly one `when receive(Type name) { ... }`
  or `when receive(Type name) serial { ... }` statement. Its serial body must
  consist of ordinary semicolon-terminated C++ statements.
- A receive is implicitly registered as a Charm++ entry, or matched to an
  explicit entry declaration with the same signature. The receive method is
  implemented by Charmxi's generated SDAG code, not a user-written C++ body.
- The generated `.ci` contains the SDAG entry body; the generated C++ class
  has its ordinary methods preserved and includes `Name_SDAG_CODE`.
- Test both queued (early) and late arrivals against a handwritten Charm++
  fixture on two processing elements.
- Multiple waits, top-level serial code, nested conditions, reference matches,
  non-void SDAG entries, and constructor SDAG are reserved for later phases.

### Phase 5.2 supported subset

- Consecutive ordinary semicolon-terminated C++ statements in a method with SDAG
  are grouped into implicit `serial { ... }` blocks, using the original source
  ranges without reformatting the statements.
- Multiple sequential top-level `when` receives, including explicit `serial`
  bodies, are supported. Receive metadata remains in generated `.ci` and their
  method implementations are supplied by Charmxi.
- Nested SDAG control constructs (`if`, `for`, `overlap`, `case`, `forall`,
  `while`) remain unsupported. Common built-in local declarations before a subsequent wait are
  rejected conservatively; general cross-suspension C++ lifetime analysis is
  deferred. Values that must persist across waits should be chare fields.
- Compare with a handwritten Charm++ fixture that contains serial work before,
  between and after two waits, for both queued and post-wait arrivals.

### Phase 5.3 supported subset

- Lower source-ranged SDAG `if`/`else` and `for` bodies recursively, preserving
  ordinary C++ statements in implicit `serial` blocks and explicit `serial` verbatim.
- Recognize `when receive[expression](...)` reference matching and retain the
  reference expression in `.ci`; register each receive entry only once even if
  used in multiple `when` sites. For parameter-marshalled entries Charm++ uses
  the first argument to match the reference expression.
- Require the SDAG `for` iteration index to be a persistent chare field;
  reject common typed loop-local declarations that cannot survive a suspension.
- Test two messages sent in reverse iteration order, both queued before
  `start()` and delivered after `start()`, against a handwritten Charm++ fixture.
  The result encodes arrival consumption order so a reference-matching failure
  cannot be masked by commutative addition.
- Nested `when` bodies, `while`, `forall`, `overlap`, and `case` are still
  deferred. The compiler does not implement C++ variable capture or promote
  pre-wait local variables into persistent storage.

### Phase 5.4 Prefix Sum benchmark

- Keep the pinned `benchmarks/prefix-sum/charmpp/` sources unchanged.
- Port the same parallel prefix algorithm to a single Charm# `.csh` source,
  using already implemented `readonly`, 1D array, reduction target, SDAG
  `for`/`if` and reference-matched `when`.
- Compile both forms with Charm++ v8.0.0 and verify the inclusive prefix of
  input `1, 2, ..., N`: element `i` must produce `(i+1)*(i+2)/2` exactly once.
- Normalize results by index (messages can arrive in different orders), reject
  missing/duplicate/incorrect outputs, and compare both implementations.
- Exercise multiple element counts on +p1 and +p2, and +p4 when four
  processing cores are available; avoid Charm++ multicore oversubscription on
  two-core CI runners. Original jittered sends vary communication timing.
- **Complete:** the C++20 tests and Charm++ v8.0.0 integration job pass for
  the pinned reference and transpiled port, with every prefix checked.

### Phase 5.5 extended SDAG controls

- `while (condition) { ... }` preserves its C++ condition and recursively lowers
  its body; a `when` may suspend between iterations.
- `overlap { ... }` lowers independent child flows recursively and waits for
  all branches. `case { when ... when ... }` lowers alternative receives, of
  which Charm++ commits to one satisfied branch.
- `forall [index] (min:max, stride) { ... }` preserves Charm++'s **inclusive**
  upper bound and requires the bracket to contain exactly one identifier.
  The index must be a persistent chare field as expected by Charmxi; the
  transpiler does not currently perform full member lookup or lifetime analysis.
- Receive bodies currently accept ordinary C++ work, not nested SDAG waits;
  one `when` signature may not list multiple comma-separated receives.
- The pinned Charm++ integration test compares an independently handwritten
  `.ci` implementation and Charm# output in both queued and late-arrival
  scenarios. It covers all four constructs in one executable.

## Phase 6: Remaining language surface

### 6.1 — Implicit mainchare convention

- A top-level scalar `chare Main` is classified as `mainchare Main` in the
  semantic model, generating a `mainmodule` and the conventional `.ci` entry.
  Its generated C++ remains `class Main : public CBase_Main`.
- An explicit `mainchare Name` remains supported. A different explicit
  mainchare alongside `chare Main` is diagnosed regardless of source order.
- The rule is exact and case-sensitive; it does **not** apply to `group Main`,
  `nodegroup Main`, or `array [ND] chare Main`. Translation units without
  any mainchare still generate `module` rather than `mainmodule`.
- Acceptance: parser/model tests and executable Charm++ v8.0.0 integration,
  while the existing explicitly declared mainchare program remains green.

### 6.2 — Additional entry attributes (research before implementation)

1. Cross-check `notrace`, `appwork`, `nokeep`, `exclusive`, `inline`,
   and `local` against pinned Charm++ v8.0.0 parser behavior, legal entry
   signatures, parallel-type applicability, and meaningful combinations.
2. Record effects and restrictions for tracing, instrumentation, message
   ownership, scheduling/exclusion and local invocation. Avoid guessing
   semantics from attribute names or silently normalizing combinations.
3. Implement one independently testable attribute slice at a time using the
   existing `[[charm::attribute]]` syntax; emit corresponding `.ci` metadata,
   remove the attribute from C++, and preserve the C++ method body.
4. Add accepted/rejected parser fixtures, matching Charmxi checks, and runtime
   tests for attributes where behavior changes; update grammar and audit.

### 6.3 — Chare templates (design review required)

- Design C++-style `template<typename T> chare Worker { ... };` syntax and
  compatible Charm++ `.ci` template declarations before implementation.
- Determine correct generated `CBase_Worker<T>` and proxy names, includes,
  instantiation and registration placement, and compatibility with SDAG.
- Discover straightforward concrete proxy uses such as `CProxy_Worker<int>`
  only where unambiguous; deduplicate registrations and provide explicit
  registration as a fallback. Do not implement a general C++ instantiator.
- Document dependent template arguments and cross-translation-unit limits.
  Require an executable Charm++ v8.0.0 template fixture before release.

### 6.2a — Instrumentation attributes

- Support `[[charm::notrace]]`, `[[charm::appwork]]`, and their pair on
  non-constructor entry methods, emitting `.ci` `[notrace]`, `[appwork]`,
  or `[notrace, appwork]` and preserving ordinary C++ implementations.
- Test accepted/rejected attribute syntax and pinned Charm++ v8.0.0
  compile/run behavior. This is metadata only: Charm# adds no instrumentation.
- Document pending ownership and scheduling semantics separately in
  [entry attribute compatibility](entry-attribute-semantics.md).

### 6.2b — Message ownership and nodegroup exclusion

- Support standalone `[[charm::nokeep]]` on entry methods: the Charm++
  runtime retains ownership of incoming messages. Do not delete, mutate or
  retain the argument after the handler completes.
- Support standalone `[[charm::exclusive]]` only on nodegroup entry methods;
  this is node-local exclusion among similarly annotated entries, **not**
  general exclusion against ordinary methods.
- Keep additional combinations (including documented `threaded+nokeep`)
  deferred until pinned toolchain and ownership checks are added.

### 6.2c — Inline/local entry invocation

- Support standalone `[[charm::inline]]` on entries, preserving Charm++'s
  local direct-call versus remote queued-delivery semantics.
- Support standalone `[[charm::local]]` only on group/array entries. Calls
  require a local recipient and may return a simple non-void C++ type;
  non-local calls abort in the Charm++ runtime. The source-to-source translator
  intentionally does not attempt to prove locality.
- Reject untested multi-attribute combinations. Unit tests cover kind/signature
  restrictions; pinned-toolchain integration exercises a same-PE `local`
  return and `inline` dispatch. Broader non-PUPable parameter coverage is
  deferred.

### 6.3.1 — Templated scalar chare declarations

- Parse a single ordinary C++ type template parameter (`typename T` or
  `class T`) immediately preceding a scalar `chare` declaration.
- Preserve the original C++ template declaration; emit corresponding templated
  `.ci` metadata and derive from `CBase_Name<T>`.
- This slice validates uninstantiated declarations with Charm++ v8.0.0.
  Concrete specialization registrations, `CK_TEMPLATES_ONLY` header handling
  and executable template instantiations belong to 6.3.2–6.3.4.
- Multi-parameter templates, defaults, templated groups/arrays/nodegroups,
  dependent arguments and cross-TU specialization discovery remain deferred.

### 6.3.2 — Explicit template specialization registration

- Accept `template chare Worker<int>;` for a previously defined scalar
  template chare and emit `chare Worker<int>;` in the generated `.ci`.
- The registration declaration is Charm# syntax, not C++ explicit class
  instantiation; remove it from generated C++ without otherwise rewriting
  the user's template definition. Detect repeated registrations.
- Emit a separate `<module>_specializations.ci` and compile both interfaces;
  import each module as required by Charmxi and include generated template
  definitions under `CK_TEMPLATES_ONLY` before registration definitions.
- Validate a runnable specialization against Charm++ v8.0.0, including
  compile, link, runtime registration and successful proxy construction.

### 6.3.3 — Automatic concrete specialization discovery

- Infer a Charm++ specialization registration from tokenized source uses of
  `CProxy_Worker<int>` (and other supported concrete built-in C++ type
  arguments) when `Worker` is a scalar template chare declared in this unit.
- Reuse the 6.3.2 `<module>_specializations.ci` emission, generated header
  inclusion and `CK_TEMPLATES_ONLY` integration. Do not alter proxy usages
  in the generated C++.
- Deduplicate inferred occurrences and explicit `template chare Worker<T>;`
  registrations regardless of whitespace; explicit registrations retain their
  original source spelling.
- Intentionally do **not** infer dependent arguments (`T`), nested arguments
  (`std::vector<int>`), or arbitrary user-defined names in this slice. These
  require explicit registrations and/or later semantic type resolution.
- Detection is local to a single translation unit and excludes comments,
  string literals and preprocessor directives. Cross-unit proxy uses are not
  discovered. Validate compiled executable behavior against Charm++ v8.0.0.


### 6.3.4 — Template validation and hardening (restricted subset complete)

- **6.3.4a/b:** Exercise multiple independent concrete registrations (initially
  built-in `int` and `double`) for the same template chare, verify deduplication,
  generated registration ordering, compile/link, and both runtime deliveries.
  Keep executable output validation in pinned Charm++ v8.0.0 CI.
- **Pinned-toolchain finding:** A templated entry signature referring to
  `CProxy_Main` requires `Main` to appear first in the generated `.ci` module;
  placing `Worker` first makes its generated `.decl.h` refer to an undeclared
  proxy type. The executable fixture uses the supported declaration order.
- **6.3.4c (complete):** Independent translation units and generated
  `<module>_templates.h`, with `CK_TEMPLATES_ONLY` after complete class
  definitions. An external consumer can instantiate the proxy from the shared
  header; it still requires explicit specialization registration in the input.
- **6.3.4d:** Validate the ordering of multiple chare template definitions
  before the `CK_TEMPLATES_ONLY` inclusion, including an executable two-template
  fixture against Charm++ v8.0.0. Reject inheritance on templated chares for now:
  ordinary bases defined only in the source unit are not automatically visible
  in the generated shared header, and templated parallel-base semantics need
  independent Charmxi validation. Keep ordinary non-template inheritance intact.
  Full template inheritance requires shared-header base resolution and an
  explicitly validated generated `.ci` inheritance declaration.
- **6.3.4e (complete — audit):** [Coverage matrix](chare-template-coverage-audit.md)
  distinguishes pinned runtime-tested cases from parser-only behavior and
  deferred user-defined/nested/dependent arguments, inheritance and multi-TU
  discovery. The templated `chare Main` implicit-mainchare convention
  interaction is addressed in a focused follow-up diagnostic and regression test.

### 6.3.4c — Cross-translation-unit template validation

- Test a standalone Charm# file containing a template definition and an
  explicit `template chare Worker<int>;` registration, plus a separately
  compiled ordinary C++ source file that uses `CProxy_Worker<int>`.
- Validate that the generated primary and specialization `.ci` modules, the
  generated C++ implementation, and the independent C++ consumer can compile,
  link and execute through pinned Charm++ v8.0.0.
- The current CLI translates and optionally builds **one** `.csh` source at
  a time. Auto-discovery cannot see uses in another source file. The explicit
  registration is therefore required in the definition unit; no whole-program
  analysis, globally shared specialization database, or multi-file build command
  is implied by this test.
- Generate `<module>_templates.h` containing complete template definitions
  and a guarded `CK_TEMPLATES_ONLY` inclusion of `<module>.def.h` after the
  definitions. Include this header in both the producer and independent C++
  consumers, while retaining the ordinary `.def.h` inclusion in one producer.
- Keep runtime registrations in `<module>_specializations.ci`; no separate
  `specializations.h` file is required. Template class definitions presently
  must be self-contained enough to move to the shared header; arbitrary
  dependencies on prior local C++ definitions remain unsupported.

## Phase 7 — Message and initialization declarations

1. **7.1:** Recognize ordered standalone primitive `T name[];` message fields; emit conventional `.ci` message member metadata and C++ pointer fields, plus friend access for generated Charmxi serialization code. Preserve fixed fields, ordinary member functions and local ordinary inheritance. Reject ambiguous/dependent/aliased/compound declarators. Validate allocation order, one/two arrays, mixed fields, cross-PE delivery, ownership and compare against handwritten Charm++ v8.0.0 reference. Do not generate custom marshalling.
2. **7.2:** Verify `readonly message M *name;` syntax and initialization semantics, then validate on multiple PEs; no general readonly pointers.
3. **7.3:** Verify signatures/registration and lower `[[charm::initnode]]` / `[[charm::initproc]]` to native declarations; count calls across PEs/nodes.
4. **7.4:** Investigate `PUPable` and `PUP::able` macros/registration; support only verified explicit class registrations, never implicit `pup()` implementations.

Keep each slice independently buildable; complete and verify 7.1 before beginning 7.2.
