# Charmxi diagnostic source mapping (PR #65)

## Goal

When `charmsharp --build` invokes Charmxi on an aggregated module interface,
show the originating `.csh` input and source position for errors reported in
the generated `.ci`. Keep the emitted Charm++ interface semantically unchanged.

## Approach

- Build a line mapping alongside `emit_ci_body`, using semantic source ranges.
  Map each emitted message, readonly, init hook, parallel declaration and entry
  to its original input file and line. For multi-line SDAG bodies, conservatively attribute generated lines to
  the enclosing SDAG body; exact statement mapping is deferred.
- Preserve Charmxi's output verbatim where a line cannot be mapped reliably;
  never fabricate a source location. Preserve secondary diagnostic lines.
- In the `--build` path, capture only the Charmxi invocation's stderr, map
  recognized `<module>.ci:<line>[:<column>]` diagnostic prefixes, and relay
  all other lines unchanged. Restore its original nonzero exit code.
- Keep a standalone generated `.ci` usable with `charmc` independently of
  the Charm# driver. Do not insert speculative C/C++ `#line` directives into
  the Charmxi grammar or change the generated interface solely for diagnostics.
- Maintain the existing API and byte-equivalence behavior when source paths
  are omitted; single-file translation must remain unchanged.

## Validation required before merge

1. Real pinned Charmxi v8.0.0 negative fixtures attribute malformed generated
   declarations to both `Main.csh` and `Worker.csh`.
2. A synthetic Charmxi fixture tests preservation of unknown lines and
   oversized line numbers.
3. A source path containing spaces maps correctly without shell execution.
4. GCC, Clang, macOS, sanitizers, Charm++ v8.0.0 integration and
   single-file output-equivalence all pass.

Follow-ups: exact SDAG statement mapping, specialization `.ci` diagnostics,
quoted-path edge cases, and additional multiline declaration cases.

## Scope boundary

This is diagnostic attribution, not generalized Charmxi semantic analysis.
Do not suppress original errors or imply exact columns when a transformation
changes token layout. Mark approximate locations as such when necessary.
