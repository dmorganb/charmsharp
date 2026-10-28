# Charm++ entry attributes: Phase 6.2 compatibility notes

This document records **Charm++ semantics**, not new Charm# behavior. The
runtime and Charmxi v8.0.0 remain authoritative. Reference: [Charm++ manual,
Entry Method Attributes](https://charm.readthedocs.io/en/latest/charm%2B%2B/manual.html#entry-method-attributes).
Manual text may describe later releases; every accepted declaration and
combination must additionally compile under the pinned Charm++ v8.0.0 CI.

| Attribute | Semantics / applicability | Charm# status |
| --- | --- | --- |
| `notrace` | Exclude entry method from execution tracing and Projections. | Phase 6.2a: translated and compiled in CI. |
| `appwork` | Mark entry as application work for performance analysis. | Phase 6.2a: translated and compiled in CI. |
| `nokeep` | Incoming message belongs to the runtime, not the callee. Do not delete, modify, or retain a borrowed message or a pointer into its storage after return. Broadcasts can share one buffer per process. Marshalled entries are already nokeep by default except threaded ones. | Phase 6.2b: translated and tested with a runtime-owned message in CI. |
| `exclusive` | Only meaningful for nodegroups; node-local mutual exclusion among exclusive entries, **not** all nodegroup entries. | Phase 6.2b: nodegroup-only, compiled and exercised in CI; cross-PE mutual-exclusion stress tests remain future work. |
| `inline` | If recipient is local to the current PE, invoke like a regular reentrant C++ call; otherwise ordinary remote delivery. Parameter forwarding differs from normal marshalling. | Planned delivery slice; local/remote runtime checks required. |
| `local` | Group and chare-array entries only; guaranteed immediate call on local PE, aborts for non-local destination. Permits non-PUPable inputs and non-void returns. | Planned local slice; requires separate return-type analysis. |

## Combination policy

- Existing `threaded` + `sync` and `reductiontarget` with either remain
  prohibited by the current Charm# subset.
- Phase 6.2a/b allow standalone `notrace`, `appwork`, `nokeep`, and
  `exclusive`, plus the pair `notrace` + `appwork`. The pair. The pair emits a single conventional `.ci` attribute list,
  `[notrace, appwork]`; CI checks actual Charmxi acceptance. Repeated markers
  are rejected.
- Other combinations are **currently unsupported by Charm#**, which is not
  a claim that Charm++ prohibits them. Do not infer legality simply from
  attribute spelling. Widen the allowlist only after pinned-compiler tests.
- `[threaded, nokeep]` is documented as meaningful; it shares marshalled
  buffers rather than making the default private copies. It needs a dedicated
  lifetime/ownership test in a later slice.
- Attribute marks are attached to entry methods, not constructors in the
  current Charm# parser. If pinned Charmxi supports a constructor attribute,
  it requires a separately reviewed exception.

## Test strategy

1. Generate `.ci` via the existing `[[charm::name]]` syntax, without
   leaving C++ attributes in the emitted method definition.
2. Assert supported signatures and negative diagnostics in C++20 unit tests.
3. Compile and run an executable through Charm++ v8.0.0 in CI; test behavior
   for message lifetime and local/remote scheduling where applicable.
4. Keep positive and rejected combinations recorded per slice instead of
   claiming full combinatorial compatibility.
