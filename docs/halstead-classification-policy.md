# Halstead token classification policy (draft v0.1)

Status: **Phase 1 measurement instrument**, pending benchmark validation. Changes to this document require a version change and remeasurement. The metrics analyzer is separate from the Charm# transpiler.

## Scope
- Analyze **developer-authored** source: Charm++ `.ci`, `.C`/`.cpp` and applicable handwritten headers; Charm# `.csh` and applicable handwritten C++ support files.
- Exclude generated `.decl.h`, `.def.h`, transpiled C++, generated `.ci`, system headers, comments and whitespace.
- Count raw source (not preprocessed macro expansions). Version 0.1 excludes **entire preprocessor directive lines**, including backslash continuations, from counts; record this exclusion in every eventual report. Source regions controlled by `#if` are *not* conditionally evaluated, so both branches are counted. This is a limitation, not a claim about compiled source.
- The unit of analysis is a complete authored program. Token frequencies merge across its included files before Halstead formulas are applied; do **not** sum nonlinear difficulty values across files.
- Keep Charm++ and Charm# inputs paired by algorithm and functionality. Do not count the translated output as authored Charm#.

## Lexical rule
- Longest-match operators: `::`, `->`, `<<=`, `[[`, `]]`, etc. are single occurrences.
- Identifiers and literal values are operands, preserving their spelling. Thus different numeric values and different identifier names increase `n2`.
- C++ reserved words and applicable Charm interface / Charm# structural keywords are operators; `entry`, `mainchare`, `readonly`, `when` and the like are not operands.
- Punctuation (`;`, `,`, parentheses, braces, brackets, colon, etc.) is counted as operators in this **syntactic-token** policy. This choice is conventional but not universal; sensitivity analysis may compare an alternative punctuation-excluding policy later.
- Operators are keyed by spelling. This deliberately means `*` (multiplication/pointer declarator) and `&` (bitwise/reference declarator) are not distinguished yet. This is a limitation of a lexical rather than AST-based metric.
- Function names, type names (unless reserved keywords), chare names, user-defined attributes and namespace members are operands. Calls have their punctuation counted; no synthetic function-call operator is added.
- String and character literals, including raw literals, are single operands. Numeric literals (including exponent signs) are single operands.
- Alternative C++ operator keywords (`and`, `or`, etc.) are operators with their written spellings; no semantic canonicalization.
- Unknown punctuation is classified as an operator and should be logged during future validation.

## Scientific safeguards
1. Publish all four fundamental counts (`n1`, `n2`, `N1`, `N2`) and the operator/operand frequency inventories, not just Difficulty or Volume.
2. Record input file paths and hashes, tool version, classification-policy version and ignored directives.
3. Report Volume and Difficulty, with Effort as a derived quantity. Halstead's Time and Bug estimates are *predictions*, not observed time or defect counts.
4. Golden-test C++, Charm++ `.ci`, Charm# `.csh`, messages, entries, marshalling, attributes, and SDAG when supported.
5. Evaluate sensitivity to the punctuation rule before interpreting effect sizes in the thesis.
6. When the compiler gains SDAG in Phase 5, update tokens and tests explicitly and version this policy.

## Next steps
Phase 2 will add frequency tables, formulas, JSON/CSV reporting and CLI integration. Phase 3 will validate paired benchmark file manifests and coverage.