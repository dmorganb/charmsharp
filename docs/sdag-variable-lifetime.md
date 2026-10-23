# SDAG suspension, variable lifetime, and loop indices

**Status:** Documented limitation in Charm# v0.1, observed with Charm++ v8.0.0 during Phase 5.3 (PR #24).

## The problem

An SDAG entry method contains asynchronous suspension points such as `when`.
It looks similar to an ordinary C++ method, but Charmxi translates the SDAG
control flow into generated continuation methods. An automatic local defined
inside one part of the original method is therefore **not necessarily in
scope** when a generated continuation resumes.

This otherwise natural-looking example is **unsupported**:

```cpp
chare Worker {
entry:
    void start() {
        for (int step = 0; step < 10; ++step) {
            when receive[step](int tag, int value) {
                process(value);
            }
        }
    }
};
```

During the Phase 5.3 integration test, the **handwritten Charm++ reference**
using the same `for (int step = ...)` syntax failed to compile against the
pinned Charm++ v8.0.0 toolchain. GCC reported that `step` was not declared
inside generated methods `Worker::_for_0_end()` and `Worker::_when_0()`
(from `control.def.h`). This is not a discrepancy caused by the Charm#
transpiler: the input was an ordinary handwritten `.ci` SDAG definition.

Conceptually (not an exact rendering of Charmxi output):

```cpp
void Worker::loop_begin() {
    int step = 0;  // local to this generated method
}
void Worker::loop_resume() {
    ++step;        // not in scope here
}
```

## Supported workaround in Charm# v0.1

Put the loop index, or any value that must survive suspension, into persistent
chare state:

```cpp
chare Worker {
    int step = 0; // member of Worker, available to generated continuations
entry:
    void start() {
        for (step = 0; step < 10; ++step) {
            when receive[step](int tag, int value) {
                process(value);
            }
        }
    }
};
```

For **parameter-marshalled** receive entries, Charm++ compares the reference
expression (`step` here) with the **first received argument** (`tag` here).
This makes out-of-order messages eligible for buffering and matching by
iteration. The Phase 5.3 integration fixture checks that property using a
noncommutative result (`2` followed by `4` produces `24`, while the wrong
order would produce `42`).

### Concurrency caution

A chare field has **one storage location per chare instance**. If multiple
overlapping invocations of the same SDAG entry method rely on the same field,
they can overwrite each other's loop index or other intermediate state.
The field workaround is appropriate only when the program's control flow
ensures those uses do not conflict (or otherwise manages separate state).
Using a field is not equivalent to per-invocation coroutine-local storage.

## What the compiler does today

- Charm# passes ordinary C++ text through as far as possible. It does **not**
  implement automatic closure capture, coroutine frames, or local-variable
  promotion to chare fields.
- Phase 5.2 rejects a conservative subset of recognizable local declarations
  in ordinary runs before an SDAG wait.
- Phase 5.3 explicitly rejects common typed `for` loop initializers such as
  `for (int step = ...)`, with the diagnostic:
  `SDAG for index must be a persistent chare field`.
- These checks are **not a complete C++ scope/lifetime analysis**; some
  unsupported local declarations or uses may still be caught only by Charmxi
  or the C++ compiler. A successful transpilation alone is not proof of
  correct asynchronous lifetime behavior.

## Potential future Charm# extension (not part of v0.1)

Charm# could make `for (int step = ...)` legal by transforming captured locals
into a generated state object associated with each asynchronous invocation.

This is more than a mechanical rename or replacement: a correct design must
address independent simultaneous invocations, state ownership, lifetime,
serialization/migration (where applicable), scope and shadowing, cleanup,
diagnostics, and compatibility with Charmxi-generated continuations. Merely
rewriting `int step` to a shared chare member would change the semantics
of overlapping invocations.

**Recommendation:** Retain the explicit persistent-state rule for v0.1,
document the limitation as inherited from Charm++ SDAG, and evaluate
per-invocation state as a separately scoped, experimentally validated
language enhancement after the initial thesis benchmarks.

## Reproducing and locating the evidence

- The failing handwritten reference and the subsequent corrected fixture were
  evaluated in [PR #24](https://github.com/dmorganb/charmsharp/pull/24).
- The supported reference and Charm# source are under
  `tests/fixtures/sdag_control/` and `examples/SdagControlFlow.csh`.
- The integration check is `tests/sdag_control_integration.sh`, executed by
  `make charm-sdag-control` in GitHub Actions with Charm++ v8.0.0.
- Charm++'s [SDAG documentation](https://charm.readthedocs.io/en/v7.0.0/charm%2B%2B/manual.html)
  describes `when`-triggered continuation behavior and reference matching.
