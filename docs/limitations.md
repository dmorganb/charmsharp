# Charm# limitations and compatibility boundaries

**Status:** 2026-10-29. **Target:** Charm++ v8.0.0, Charm# grammar v0.1.
This is the central reference for **remaining** restrictions of the current
implementation. For supported syntax, see [language coverage](language-coverage.md).
For why a restriction exists and its test evidence, follow the links below.

Charm# is a lightweight, structural C++20 source-to-source transpiler. It
recognizes Charm# declarations and selected SDAG structure, preserves ordinary
C++ as source ranges, and generates conventional Charm++ `.ci` and C++ files.
A translated program is still subject to Charmxi and the native C++ compiler.

## How to interpret the statuses

- **Rejected:** Charm# intentionally produces a diagnostic for this form.
- **Not implemented:** Charm# does not generate the corresponding Charm++
  interface/registration metadata. Ordinary C++ pass-through is not support
  for that Charm++ interface feature.
- **Structurally accepted, not proven:** Text can be preserved or emitted but
  Charm# does not perform enough C++ semantic analysis to establish correctness.
- **Not covered by the pinned runtime tests:** A form may work, but the current
  executable test suite does not establish that behavior.

These are limitations of **Charm#** unless explicitly identified as a
Charm++/Charmxi constraint. They are not a statement that Charm++ itself
prohibits a construct.

## Parsing and project/build model

| Boundary | Current status | Practical consequence |
| --- | --- | --- |
| General C++ parsing, type inference, overload resolution or ABI/layout checking | **Not implemented** | The structural parser recognizes only targeted Charm# patterns; use Charmxi and the chosen C++ compiler for semantic validation. |
| Parallel declarations inside namespaces, class scopes, or preprocessor conditionals | **Rejected** | Put the supported Charm# interface declaration at file scope outside conditional branches. Ordinary C++ constructs remain pass-through. |
| Multiple Charm++ modules, `.ci extern module`/`include` and dependency graph discovery | **Not implemented** | Multiple `.csh` inputs can now form **one shared module**, but cross-module interface discovery is still not implemented. The template specialization helper module is generated only for its narrow use case. |
| Whole-program analysis and cross-file declaration discovery | **Not implemented** | A shared-module invocation accepts several `.csh` inputs, validates declared-name collisions, and merges interface metadata. It does not infer arbitrary C++ cross-file dependencies; explicit scalar-template specialization registration and PUPable classes are supported in the multi-input path, but arbitrary cross-file discovery is not. |
| Source maps and generated `#line` directives | **Not implemented** | Downstream C++/Charmxi diagnostics can refer to generated source locations. |
| Native Charmxi/XI-Builder backend | **Not implemented** | Charm# emits text-based `.ci` and C++ then invokes the ordinary Charm++ toolchain; there is no new runtime. |

## Parallel objects, inheritance, and templates

| Boundary | Current status | Practical consequence |
| --- | --- | --- |
| Implicit `chare Main` for templated chares, or two conflicting mainchares | **Rejected** | The convention applies only to a non-template scalar `chare Main`; an explicit `mainchare` remains supported. Translation units without a mainchare remain valid. |
| Multiple parallel bases, non-chare parallel inheritance, external/later-defined bases, virtual/dependent/complex bases | **Rejected / not implemented** | One earlier-declared, local public chare base is supported; ordinary earlier-defined local C++ bases have a separate restricted implementation. |
| Inheritance on templated chares | **Rejected** | Shared-header visibility and Charmxi template inheritance have not been sufficiently validated. |
| More than one template type parameter, default/non-type/template-template parameters, packs, templates of arrays/groups/nodegroups/mainchares/messages | **Rejected / not implemented** | Supported templates are scalar chares with exactly one `typename` or `class` parameter. |
| Automatic specialization discovery outside the input file | **Not implemented** | Register explicitly in the defining unit, e.g. `template chare Worker<int>;`, for proxy use in another translation unit. |
| Inferred specializations with user-defined, nested, dependent or arbitrary template arguments | **Not implemented** | Automatic discovery recognizes straightforward built-in concrete types. Explicit complex argument spellings are parsed but **not broadly runtime-validated**. |
| Arbitrary template-header dependencies or out-of-line definitions | **Not implemented** | The generated `<module>_templates.h` must have all required type definitions/includes visible to every consumer. Do not assume arbitrary source-local declarations are moved with it. |

See [template coverage audit](chare-template-coverage-audit.md) for the
test-by-test matrix, including validated cross-translation-unit use.

## Entry methods, attributes, and marshalling

| Boundary | Current status | Practical consequence |
| --- | --- | --- |
| General method signatures (default arguments, many qualified/trailing/templated forms, complex returns) | **Rejected / structurally accepted, not proven** | Supported entry declarations are intentionally narrower than arbitrary C++ signatures. Simple constructor initializer lists and selected synchronous returns are supported. |
| Entry attribute spellings beyond `threaded`, `sync`, `reductiontarget`, `notrace`, `appwork`, `nokeep`, `exclusive`, `inline`, `local` | **Not implemented** | The Charm++ v8.0.0 parser has additional attributes, but Charm# does not expose them yet. |
| Arbitrary combinations of supported entry attributes | **Rejected** | A conservative allowlist implements separately tested cases and `notrace + appwork`. Rejection by Charm# does **not** necessarily mean the pair is illegal in Charm++. |
| General pointer/array marshalling and complex length expressions | **Rejected / not implemented** | `T data[n]` supports only simple `int`/`float`/`double` arrays with positive decimal literal or earlier named integer bound; a bare pointer does not imply a marshalled array. |
| Arbitrary C++ object marshalling, PUP, and pointer lifetime analysis | **Structurally accepted, not proven** | The native toolchain controls serialization, buffers, and ownership; Charm# does not synthesize marshallers. |

See [entry attribute semantics](entry-attribute-semantics.md) and the
[declaration audit](declaration-coverage-audit.md).

## Messages and readonly storage

| Boundary | Current status | Practical consequence |
| --- | --- | --- |
| Variable-size message arrays of aliased, compound or unsupported element types; comma-separated/ambiguous declarators, conditional or nested unsized arrays | **Rejected** | The validated subset is ordered standalone primitive `T field[];` (char, short, int, long, float, double) transformed into pointer fields. |
| Arbitrary message members, custom packing and deep layout/ownership semantics | **Structurally accepted, not proven** | Charmxi owns allocation and serialization. Test nontrivial member types and ownership explicitly; Charm# does not provide custom packing. |
| `readonly` general pointers, complex/qualified/templated or forward-defined message types, initialized declarations, multidimensional arrays | **Rejected / not implemented** | Readonly message pointers require an earlier `message M` in the same unit; scalar/fixed-array forms accept simple declarations and decimal bounds. |
| Compile-time readonly mutation or arbitrary initialization protocols | **Not implemented** | `readonly` is a Charm++ distribution construct, **not** C++ `const`/`constexpr`; runtime initialization remains under Charm++ rules. |
| Message modifiers such as `packed`/`varsize`, conditional members | **Not implemented** | These are outside the currently validated message-declaration subset. |

The supported variable-size message and readonly-message pointer paths
**have executable cross-PE tests**; they are not wholly missing features.
See [variable message validation](variable-message-validation.md) and
[readonly message validation](readonly-message-validation.md).

## SDAG suspension and control flow

| Boundary | Current status | Practical consequence |
| --- | --- | --- |
| Nested `when` receives and comma-separated multi-receive dependencies | **Not implemented** | Supported `when`, `overlap`, `case`, `while`, `forall`, reference matching, `if` and `for` cover restricted structures only. |
| General lifetime/scope analysis across asynchronous suspension | **Not implemented** | Local variables do not automatically survive a Charmxi-generated continuation. Charm# rejects recognizable unsafe declarations conservatively, but cannot prove arbitrary C++ lifetime correctness. |
| Typed `for` indices crossing a `when` suspension | **Rejected** | Use a persistent chare field only when concurrent invocations cannot interfere. |
| Automatic coroutine frames, closure capture, or state promotion | **Not implemented** | No runtime/scheduling semantics beyond Charm++ SDAG are introduced. |

**Charmxi-specific limitation:** A handwritten Charm++ SDAG `for (int step
= ...)` containing a reference-matched `when` was observed to fail when
generated continuations referenced `step` outside its original C++ scope.
This was reproduced independently of Charm# in the pinned toolchain. Making
`step` a chare field avoids that compilation error but can introduce shared
state interference between overlapping invocations. See
[SDAG variable lifetime](sdag-variable-lifetime.md).

## Initialization hooks and PUPable registration

| Boundary | Current status | Practical consequence |
| --- | --- | --- |
| Class-static, namespaced/qualified, templated or accelerator `initnode`/`initproc` hooks | **Rejected / not implemented** | Supported forms are annotated, global, zero-parameter `void` free-function definitions. |
| Hook behavior across multiple physical nodes | **Not covered by the pinned runtime tests** | The `+p2` fixture runs two processing elements on one node, validating one `initnode` and two `initproc` invocations—not multi-node networking. |
| Namespaced, nested, conditional, or templated PUPable declarations; indirect or aliased `PUP::able` bases | **Rejected / not implemented** | The supported annotation targets a top-level non-template class directly deriving `public PUP::able`. |
| Automatic migration constructors, `pup()` generation or automatic PUP registration discovery across files | **Not implemented** | Supply `Name(CkMigrateMessage*)` and `pup(PUP::er&)` manually; Charm# only adds `PUPable_decl` and `.ci` metadata. |
| Deduplication with manually written PUP registration macros or nontrivial C++ semantic signature checking | **Not implemented / structurally accepted, not proven** | Avoid mixing generated registration with manual `PUPable_decl`/`PUPable_def` for the same class; the native C++ compiler remains authoritative. |

See [init hooks validation](init-hooks-validation.md) and
[PUPable validation](pupable-validation.md). Phase 7.4's restricted
serialization fixture compares a handwritten implementation with Charm#
generated output and transfers a polymorphic object to PE 1 against pinned
Charm++ v8.0.0.

## Evidence and scope

The four paired benchmarks (Primes, Reduction, Jacobi2D, Prefix Sum), targeted
unit/negative tests, and pinned Charm++ v8.0.0 runtime fixtures validate the
**implemented subsets**. They do not prove full Charm++ source compatibility,
whole-program analysis, migration/checkpoint-restart behavior, all PE/node
layouts, or arbitrary PUP serialization.

If a missing construct blocks an application such as LeanMD, document the
smallest reproducer and whether it fails in Charm# translation, Charmxi,
native C++ compilation, or runtime behavior before expanding the grammar.

- [Current language coverage](language-coverage.md)
- [Versioned interface declaration audit](declaration-coverage-audit.md)
- [Implementation roadmap](roadmap.md)
- [Chare template coverage audit](chare-template-coverage-audit.md)
