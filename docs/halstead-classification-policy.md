# Halstead token classification policy (v0.2)

Status: **Versioned lexical measurement instrument**, validated against the four benchmark source pairs by CI. Changes to classification require a policy version change and remeasurement. The analyzer is separate from the Charm# transpiler.

## Scope
- Analyze **developer-authored** source: Charm++ `.ci`, `.C`/`.cpp` and applicable handwritten headers; Charm# `.csh` and applicable handwritten C++ support files.
- Exclude generated `.decl.h`, `.def.h`, transpiled C++, generated `.ci`, system headers, comments and whitespace.
- Count raw source (not preprocessed macro expansions). Version 0.2 excludes **entire preprocessor directive lines**, including backslash continuations, from counts; record this exclusion in every eventual report. Source regions controlled by `#if` are *not* conditionally evaluated, so both branches are counted. This is a limitation, not a claim about compiled source.
- The unit of analysis is a complete authored program. Token frequencies merge across its included files before Halstead formulas are applied; do **not** sum nonlinear difficulty values across files.
- Keep Charm++ and Charm# inputs paired by algorithm and functionality. Do not count the translated output as authored Charm#.

## Lexical rule
- Longest-match operators: `::`, `->`, `<<=`, `[[`, `]]`, etc. are single occurrences.
- Identifiers and literal values are operands, preserving their spelling. Thus different numeric values and different identifier names increase `n2`.
- C++ reserved words and applicable Charm interface / Charm# structural keywords are operators; `entry`, `mainchare`, `readonly`, `when` and the like are not operands **in their relevant languages**. Each `.ci` file uses Charm interface keywords, `.csh` uses Charm# keywords, and ordinary `.C`/`.cpp`/`.h` etc. uses only C++ keywords, even within a Charm++ program.
- The C++ literal values `true`, `false`, and `nullptr` count as operands, notwithstanding their keyword status in the C++ grammar.
- The default `--punctuation include` counts punctuation as operators. The sensitivity policy `--punctuation exclude` omits precisely `;`, `,`, `(`, `)`, `{`, `}`, `[`, `]`, and `:`. Other operators, including `.`, `::`, `->`, `[[`, `]]`, `?`, arithmetic, comparison, and assignment, remain counted. The sensitivity policy is not a claim that the excluded syntax has no cognitive cost.
- Operators are keyed by spelling. This deliberately means `*` (multiplication/pointer declarator) and `&` (bitwise/reference declarator) are not distinguished yet. This is a limitation of a lexical rather than AST-based metric.
- Function names, type names (unless reserved keywords), chare names, user-defined attributes and namespace members are operands. Calls have their punctuation counted; no synthetic function-call operator is added.
- String and character literals, including raw literals, are single operands. Numeric literals (including exponent signs) are single operands.
- Alternative C++ operator keywords (`and`, `or`, etc.) are operators with their written spellings; no semantic canonicalization.
- Unknown punctuation is classified as an operator and should be logged during future validation.

## Scientific safeguards
1. Publish all four fundamental counts (`n1`, `n2`, `N1`, `N2`) and the operator/operand frequency inventories, not just Difficulty or Volume.
2. Record input file paths and hashes, tool version, classification-policy version and ignored directives.
3. Report Volume and Difficulty, with Effort as a derived quantity. Halstead's Time and Bug estimates are *predictions*, not observed time or defect counts.
4. Regression-test C++, Charm++ `.ci`, Charm# `.csh`, literal values, keyword context, marshalling, attributes, and SDAG constructs; expand golden-token tests as coverage evolves.
5. Publish **both** punctuation variants before interpreting effect sizes in the thesis.
6. Maintain explicit coverage for SDAG (`when`, `overlap`, `case`, `while`, `forall`) as examples and compiler support evolve.

## Version history
- **0.1:** Initial source-token classification, punctuation included; Charm++ interface keyword set also applied to C++ files; C++ boolean/null literal values treated as operators.
- **0.2:** Correct literal classification and per-extension language selection; add optional structural-punctuation exclusion. Both modes have independently labeled JSON/CSV outputs.

## Known remaining limitations
- The analyzer is lexical: it does not distinguish unary/binary `*`, syntactic function calls, or the roles of identifiers via a C++ AST.
- The report lists analyzed input paths and CI commits, but does not yet embed per-file content hashes or a tool-binary hash. Pin the exact Git commit of each run.
- Unlike the initial four examples, larger programs may rely heavily on macro expansion or conditional compilation. Inspect the input manifest before comparing them.