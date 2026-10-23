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
| 5.2 | Planned | Implicit `serial` segmentation and statement-boundary preservation |
| 5.3 | Planned | SDAG `if`, `for`, reference-matched `when` and asynchronous behavior tests |
| 5.4 | Planned | Port Prefix Sum, verify every expected prefix with varying PEs and message arrival orders |
| 5.5 | Planned | Extend to `overlap`, `case`, `while`, `forall`, and lifetime diagnostics |
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
6. **5.5:** Expand syntax and diagnostics, particularly `overlap`, `case`,
   `while`, `forall`, and cross-suspension variable lifetime.

Every emitting slice must compare its `.ci` structure to a handwritten
Charm++ fixture and compile and execute through the pinned Charm++ CI toolchain.

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
