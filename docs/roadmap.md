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
| 3 | Next | Single SDAG `when`; implicit receive entry declaration; first safe `serial` lowering |
| 4 | Planned | SDAG references, `overlap`, `case`, loops, lifetime diagnostics, runtime equivalence tests |
| 5 | Planned | Remaining `.ci` declarations (`message`, `readonly`), inheritance, richer entries, multi-file/module integration, source mapping |

## Next SDAG slice

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
