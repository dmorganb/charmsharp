# Incremental implementation roadmap

This roadmap tracks the implementation against `charmsharp-grammar-v0.1.md`.
See [language coverage](language-coverage.md) for the detailed supported syntax,
unsupported entry attributes, return types, and remaining declaration features.

| Milestone | Status | Scope / acceptance |
| --- | --- | --- |
| Phase 1 | Complete | Handwritten lexer and structural parser; source-range edits; one chare, entry declarations, `.ci` and C++ emission; regression tests |
| 2.1 / 2.2 | Complete | Multiple chares and `mainchare`/`mainmodule`; runnable Hello World |
| 2.3 | Complete (basic forms) | Groups, nodegroups, 1D and 2D arrays compiled and exercised against Charm++ |
| 2.4 | Initial subset complete | `threaded` and `sync` attributes; `void` and simple synchronous message-pointer returns |
| 2.5 | Complete as infrastructure | Charm++ v8.0.0 pinned integration: compile, link and execute programs on GitHub Actions, with cached toolchain |
| 3.1 | Complete | Fixed-size `message` declarations and executable send/receive integration |
| 3.2 | In progress | `readonly` global declarations and initialization semantics |
| 3.3 | Next | Parallel-type inheritance and other declaration forms |
| 4 | Planned | Single SDAG `when`; implicit receive entry declaration; first safe `serial` lowering |
| 5 | Planned | SDAG references, `overlap`, `case`, loops, lifetime diagnostics, runtime equivalence tests |
| Later | Planned | Variable-size messages, richer entries, multi-file/module integration, source mapping |

## Fixed-size message slice (phase 3.1)

1. Lower top-level `message Name { ... };` to `message Name;` in `.ci` and
   `class Name : public CMessage_Name { ... };` in C++.
2. Preserve fixed-size fields and ordinary member methods; reject unsized
   arrays and inheritance until their semantics are implemented.
3. Compile and execute a program transferring a custom message in CI.

## Readonly globals (phase 3.2)

1. Lower `readonly Type name;` and `readonly Type name[N];` into the corresponding
   `.ci` metadata while preserving ordinary global C++ definitions.
2. Keep initialization explicit in the mainchare constructor: Charm++ broadcasts
   assigned values to processing elements during startup.
3. Reject pointers, initializers and multi-token types until a later slice.

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
