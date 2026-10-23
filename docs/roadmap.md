# Incremental implementation roadmap

This roadmap follows `charmsharp-grammar-v0.1.md` without treating its
unspecified C++ recognition boundaries as implemented facts.

| Milestone | Scope | Acceptance criteria |
| --- | --- | --- |
| 1 — current | One `chare`, simple `entry:` method declarations/definitions, `.ci` and `.cpp` generation | Golden translation, lexical preservation, diagnostics, CLI tests |
| 1b — validation | Compile generated files with pinned Charm++ release | `charmc` accepts `.ci` and `.cpp` for example; one existing `mainchare` invokes the chare |
| 2 | `mainchare`, `array [nD] chare`, module naming / multi-file build integration | Cross-file examples compile and run; no declaration collisions |
| 3 | SDAG `when` with implicit `.ci` receive entries; infer `serial` for straight-line runs | Compare generated output to handwritten SDAG; reject cross-suspension locals |
| 4 | `overlap`, `case`, loops, explicit receive signature validation | Compilation and runtime checks for ordering and matching |
| 5 | Attributes, messages, readonly, groups, inheritance, complex signatures | Grammar conformance suite and backwards-compatible pass-through cases |

## Suggested first integration fixture

Pair the generated `Counter` chare with a conventional handwritten Charm++
`mainchare` declared in an existing `.ci` file and a C++ implementation that
constructs `CProxy_Counter` and invokes `increment`. This tests the generated
proxy interface while keeping phase 1 limited to exactly one source-level
`chare`. Lock the Charm++ version for reproducibility before making runtime
claims.

## Known parser boundaries to revisit

1. Balanced scanning is lexical, not full C++ parsing. Test templates,
   lambdas, requires clauses, attributes, operators, and macro-heavy classes
   before permitting those signatures.
2. Separate preprocessing policy from C++ pass-through behavior: generated
   Charm# declarations inside `#if` are intentionally rejected for now.
3. Source-range edits preserve normal C++ formatting but need line mappings
   for useful diagnostics in generated `.cpp` and `.ci` files.
4. SDAG support must distinguish ordinary C++ control flow from suspension
   points, and must never invent automatic variable lifetime extension.
