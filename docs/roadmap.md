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
| 6.2 | In progress (6.2a instrumentation) | Expand `[[charm::...]]` entries: `notrace`, `appwork`, `nokeep`, `exclusive`, `inline`, `local`; document Charm++ v8.0.0 legality and runtime semantics before implementation |
| 6.3 | Design gate | C++-style chare templates, `.ci` template metadata and `CBase_Name<T>`; conservative concrete specialization discovery, explicit registration fallback, duplicate handling, cross-TU limitations; implement only after design review |
| Later | Planned | Variable-size messages, richer entries, multi-file/module integration, source mapping |

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
