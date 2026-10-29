# Phase 7.2 — Readonly message pointers

Status: **complete for restricted local message types** (2026-10-29).

Charm# accepts `readonly M* name;` when `M` is an earlier-declared top-level message type.
It generates `readonly message M* name;` in the .ci and an ordinary `M* name;` global in C++.
The message must be allocated and initialized in the mainchare constructor using Charm++ new syntax.
Charm++ registers and distributes that initialized message; Charm# does not introduce constness or ownership rules.

## Evidence

- Parser tests cover valid output and reject arbitrary pointers, forward types, pointer arrays, multiple pointers and inline initializers.
- `tests/readonly_message_integration.sh` compiles generated Charm# and the independent handwritten .ci/C++ fixture against Charm++ v8.0.0.
- Both programs allocate a variable-size readonly message on PE 0, inspect it on PE 1 under `+p2`, and produce identical normalized results.
- Green CI: https://github.com/dmorganb/charmsharp/actions/runs/40906998440

## Deferred

Imported/forward-declared, qualified and templated message types; general pointer declarations; automatic readonly mutation checks; arbitrary initialization schemes.
Charmxi remains authoritative about message registration and distribution.
