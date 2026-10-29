# Multi-file Charm# design: shared Charm++ module

Status: **implementation milestone in progress**. This document describes a
proposed CLI, not an already implemented feature.

## Desired project layout

```text
Main.csh
Worker.csh
Physics.cpp
Particle.h
Makefile
```

`charmsharp --module Simulation Main.csh Worker.csh` should produce
`Simulation.ci`, `Main.cpp` and `Worker.cpp` with **one shared Charm++
module**. Ordinary `.cpp` and `.h` sources should remain native C++ inputs
to charmc, not be silently transpiled.

## Essential implementation constraints

1. Current `translate(source,module)` produces one complete `module` or
   `mainmodule` interface. The shared-module frontend needs a project-level
   combination of **validated per-file semantic models**, not merely
   concatenation of several complete `.ci` module wrappers.
2. Current C++ emission appends `#include "<module>.def.h"` to every
   generated `.cpp`. That is not legal for multiple translation units
   sharing the same module: registration definitions would be duplicated.
   Include the shared `.def.h` **exactly once** in a suitable registration
   translation unit, with the requisite chare class definitions visible.
   A naive standalone registration `.cpp` cannot compile if class definitions
   are not visible, so shared generated headers or another validated
   strategy are required.
3. The shared `.decl.h` must be generated before any implementation compiles.
   Cross-file references may need generated forward/type declarations and
   consistent public headers. Charm# must not infer arbitrary C++ dependencies.
4. One `mainmodule` designation across all sources; diagnose multiple
   mainchares and duplicate interface identifiers. Source-located errors
   must identify the originating `.csh` input.
5. Registration metadata, SDAG code and template specializations must be
   merged without dropping per-file source-range tracking. Explicit
   registration remains a legitimate cross-file fallback.
6. The build may need to recompile dependent translation units when a shared
   interface changes, but body-only edits should not force regeneration of
   unchanged `.ci` output.

## Initial test baseline

`tests/multifile/` is an **independently handwritten Charm++ fixture**:
one `Simulation.ci`, `Main.cpp`, `Worker.cpp`, and `Worker.h`.
The `build.sh` script compiles each C++ source separately, links them,
and runs `+p2`. This establishes the target artifact and registration
conventions before introducing a multi-file Charm# compiler path.

## Acceptance for implementation PRs

- Shared CI interface and two independent generated C++ translation units,
  both buildable using pinned Charm++ v8.0.0.
- Runtime communication across PEs equals the handwritten fixture.
- Ordinary C++ sources can link into the same application unchanged.
- Single-input CLI remains byte-equivalent on existing fixtures.
- Invalid duplicate entry/types, multiple mainchares and basename collisions
  yield actionable errors.
- Makefile integration supports declared dependencies and incremental updates.

## Current branch implementation

The CLI now accepts multiple `.csh` inputs under one module and emits
one `.ci`, a class header, one registration `.cpp`, and one implementation
`.cpp` per input. Source files are independently compiled through `--build`,
and native C++ files can be linked unchanged. Cross-file template and PUPable
registration remain explicitly unsupported; see below.

## Shared-registration implementation

The multi-input compiler path now emits one shared `<module>_classes.h`
containing transformed parallel/message class definitions, plus a single
`<module>_registry.cpp` that includes the shared header followed by
`<module>.def.h`. The per-source C++ translation units include the shared
class header but **do not** include `.def.h`. This allows each generated
C++ file to be compiled independently and linked with exactly one registration
object. The CLI's multi-input `--build` invokes that pipeline. The supported
subset excludes templated chares and PUPable registration pending shared-header
and registration validation. Ordinary cross-file C++ header dependencies remain
the user's responsibility.

The integrated multi-input `--build` path also accepts ordinary `.cpp`,
`.cc` and `.C` files, compiling them directly with `charmc` and linking
them with the generated objects. The source files are **not** rewritten or
transpiled. The end-to-end fixture verifies both explicit Make-style linking
and the integrated build driver. Build dependency tracking beyond invoking the
compiler remains the responsibility of Make/CMake.

## Validation status

A real multi-source Charm# fixture and separate native `Physics.cpp` fixture
are exercised under Charm++ v8.0.0 in the repository CI. The script tests
manual per-TU compile/link **and** the integrated multi-input `--build` path.
The full legacy compiler/regression suite is also run to guard single-file
compatibility. More language feature combinations (especially template and
PUPable registration across inputs) remain open before closing the milestone.
