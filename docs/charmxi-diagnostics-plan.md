# Charmxi diagnostic source mapping (PR #65)

## Goal

When `charmsharp --build` invokes Charmxi on an aggregated module interface,
show the originating `.csh` input and source position for errors reported in
the generated `.ci`. Keep the emitted Charm++ interface semantically unchanged.

## Approach

- Build a line mapping alongside `emit_ci_body`, using semantic source ranges.
  Map each emitted message, readonly, init hook, parallel declaration and entry
  to its original input file and line. For multi-line SDAG bodies, use finer
  source ranges rather than assigning every error to the enclosing entry.
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

1. Two Charm# inputs in one module; intentionally invalid Charmxi declaration
   in the second source. Assert the reported `.csh` path and source line.
2. Distinguish diagnostics from two different files and multiple declarations.
3. A multiline declaration and an SDAG-related error near the mapped region.
4. Unknown/unmappable Charmxi output passes through verbatim.
5. Paths with spaces or quoted characters do not break matching or execution.
6. GCC, Clang, macOS, sanitizers, pinned Charm++ v8.0.0 integration and
   single-file output-equivalence all pass.

## Scope boundary

This is diagnostic attribution, not generalized Charmxi semantic analysis.
Do not suppress original errors or imply exact columns when a transformation
changes token layout. Mark approximate locations as such when necessary.
