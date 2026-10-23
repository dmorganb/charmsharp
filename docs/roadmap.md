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
| 4.0 | In progress | Port Primes to Charm# and compare compiled output against unmodified Charm++ reference in CI |
| 4.1 | In progress | Support `[[charm::reductiontarget]]` and port Reduction; compare runtime results |
| 4.2 | Planned | Preserve constructor initializer lists in entry declarations |
| 4.3 | Planned | Support variable-length marshalled entry parameters; port Jacobi2D |
| 4.4 | Planned | Implement benchmark-driven SDAG constructs; port Prefix Sum |
| 5 | Planned | Extended SDAG references, `overlap`, `case`, lifetime diagnostics and expanded equivalence tests |
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

## Later SDAG slice

1. Pick one executable fixture with a mainchare and a chare using a single
   `when` receive.
2. Generate conventional SDAG in the `.ci` file, including a receive entry;
   preserve the non-SDAG C++ parts and insert only the C++ glue required by
   Charm++.
3. Compare against an equivalent handwritten Charm++ SDAG example.
4. Reject ambiguous C++ constructs and invalid cross-`when` local lifetimes
   rather than guessing.

## Known parser boundaries

- Lexical balance is not a full C++ parser; templates, lambdas, requires
  clauses, macros and qualified signatures need targeted tests.
- Conditional preprocessing of parallel declarations is rejected.
- Diagnostics use source offsets but emitted sources still lack `#line`
  mapping or a dedicated source map.
- Marshalled parameter legality and valid PUP/message semantics remain
  responsibilities of Charm++ until semantic validation is explicitly added.
