# Phase 7.3 — Initialization hooks validation

Status: complete for top-level void free-function definitions (2026-10-29).

## Semantics

- `[[charm::initnode]]` emits `initnode void name();`; `[[charm::initproc]]` emits `initproc void name();`.
- Original C++ function bodies remain unchanged; attributes are removed during lowering.
- Charmxi registers hooks through `_registerInitCall(name, isNodeCall)`.
- The runtime owns invocation and scheduling; Charm# does not add semantics.

## Evidence

- Positive and negative parser tests for both attributes, `(void)`, invalid signatures, duplicate registration, non-global placement and prototypes.
- `tests/init_hooks_integration.sh` compiles the Charm# output and a separately handwritten .ci/C++ reference using Charm++ v8.0.0.
- With multicore `+p2`, both versions record exactly one node-level hook (node 0), two proc-level hooks (PEs 0 and 1), and one mainchare constructor (PE 0).
- The script verifies hook events precede mainchare construction and compares normalized output.
- Implementation validation CI: https://github.com/dmorganb/charmsharp/actions/runs/40921950543

## Limitations

Multi-node network runtimes, static class methods, qualified/namespaced names, template functions and accelerator callbacks are untested or rejected.
Do not claim that a single-node multicore test establishes multi-node behavior.
