# Charm# implementation coverage

Status: 2026-10-29. Target: Charm++ v8.0.0 and Charm# grammar v0.1.

For the **versioned, construct-by-construct comparison** against the
actual Charm++ `.ci` parser, see the
[Phase 3.4 declaration coverage audit](declaration-coverage-audit.md).
This page summarizes current implementation and its boundaries.

## Implemented (restricted subsets)

| Charm# construct | Lowering | Evidence / restriction |
| --- | --- | --- |
| `chare`, `mainchare` | `chare`/`mainchare` in `.ci`; `CBase_` C++ classes | Explicit or implicit scalar `chare Main` mainchare; one per input; executable tests. |
| `CProxy_Worker<int>` (known template chare) | Inferred supplemental `.ci` specialization registration, deduplicated with explicit registrations | Recognized built-in concrete type arguments only; one input translation unit; cross-TU proxy use validated using an explicit defining-unit registration. |
| `template chare Worker<int>;` | Supplemental `<module>_specializations.ci`, compiled with the primary `.ci` | Explicit concrete specialization; executable Charm++ v8.0.0 validation; arbitrary argument spellings are only structurally parsed, not broadly validated. |
| `template<typename T> chare Worker` | Templated `.ci` chare + `CBase_Worker<T>` in generated `<module>_templates.h` | One `typename`/`class` parameter; executable registration, proxy construction and cross-TU tests against Charm++ v8.0.0. Inheritance on templated chares is currently rejected. |
| `group`, `nodegroup`, `array [ND] chare` | Parallel types in `.ci` | 1D/2D array and group/nodegroup executable tests; other positive dimension spellings parsed but untested. |
| `entry:` | Public C++ method + `.ci entry` | Simple constructors, void methods, declarations/inline bodies, and initializer lists on entry constructor definitions; parameter text copied without marshalling proof. |
| `[[charm::threaded]]`, `[[charm::sync]]`, `[[charm::reductiontarget]]`, `[[charm::notrace]]`, `[[charm::appwork]]`, `[[charm::nokeep]]`, `[[charm::exclusive]]`, `[[charm::inline]]`, `[[charm::local]]` | `[threaded]`, `[sync]`, `[reductiontarget]` | Runtime threaded test, synchronous message-pointer interface, reduction benchmark, instrumentation, ownership/exclusion, and direct-call integration. |
| `message Name { ... };` | `message Name;` plus `CMessage_Name` class | Fixed-size runtime send/receive; no general serialization proof. |
| `readonly T x;`, `readonly T a[N];` | Readonly interface plus global C++ storage | Scalar runtime test; decimal array syntax checked. |
| Ordinary C++ base classes on chares/messages | Added after `CBase_Name` / `CMessage_Name` | Multiple earlier-defined local simple bases, access specifiers, runtime integration. |
| One `chare Derived : public Base` | `.ci` parallel inheritance + generated `CBase_Derived` | Inherited proxy method runtime test; base must be earlier-defined local chare. |
| Ordinary C++ outside Charm# declarations | Source-range preservation | Pass-through, **not** a C++ semantic parser. |
| SDAG `when`, implicit `serial`, sequential receives | SDAG entry bodies in `.ci` and generated `_SDAG_CODE` glue | Phase 5.1–5.2 handwritten Charm++ comparisons; early and late arrivals. |
| SDAG `if`/`else`, `for`, `while`, `forall`, `overlap`, `case`, and `when name[ref](...)` | Source-ranged recursive SDAG lowering and receive registration | Phases 5.3–5.5 compare against handwritten Charm++ v8.0.0; suspension-sensitive loop indices must be persistent chare fields. |

## Significant remaining boundaries

- **Entry attributes:** v8.0.0 recognizes many more than the supported
  `threaded`, `sync`, `reductiontarget`, `notrace`, `appwork`, `nokeep`, `exclusive`, `inline`, and `local`; see the audit and
  [compatibility notes](entry-attribute-semantics.md) for remaining semantics.
  `expedited` appears in current documentation but needs a pinned-version
  compatibility check.
- **Marshalled entry arrays (Phase 4.3):** The language design now accepts `T data[n]` in entry signatures to retain the Charm++ `.ci` element-count metadata while lowering the C++ signature to `T* data`. Recognition, transformation, and runtime verification are implemented for simple `int`/`float`/`double` array parameters with positive decimal literal or earlier named `int` bounds. Bare pointers do not imply an array length; unsupported complex declarators and bounds are rejected. Jacobi2D exercises remote halo transfer.
- **Entry methods:** Nontrivial return types, qualifiers, constructor
  templates, default arguments and local entry semantics
  are deferred. Constructor initializer lists are supported for entry definitions with simple or qualified base/member designators and parenthesized or braced initializers. We do not perform full C++ type resolution or PUP validation.
- **Messages/readonly:** Variable-size messages, readonly message pointers
  and complicated declarations need their own serialization/initialization
  tests. Fixed-size declarations with unsupported members may still fail in
  Charm++ because the translator is structural, not semantic.
- **Inheritance:** External/later-defined bases, multiple parallel bases,
  non-chare parallel inheritance and complex base specifiers are deferred.
  Any inheritance on templated chares is explicitly rejected until shared-header
  base resolution and Charmxi template inheritance semantics are verified.
- **Interface constructs:** `initnode`, `initproc`, `PUPable`,
  `extern module`, `include`, general template entities (beyond scalar chare declarations), `.ci` namespaces,
  chare attributes and module dependency tracking lack native Charm# lowering.
- **Source/build model:** One module per input, no dependency discovery,
  conditional parallel declarations rejected, and no generated `#line`
  mapping. Template definitions are moved into `<module>_templates.h`; it does
  not automatically import arbitrary dependencies from the original `.csh`.
- **SDAG remaining:** `when`, implicit `serial`, sequential waits, `if`/`else`, `for`, `while`, `forall`, `overlap`, `case`, and reference matching have validated restricted implementations (Phases 5.1–5.5). Nested SDAG receives, comma-separated multi-receive dependencies, general local-variable lifetime analysis, and other SDAG forms remain deferred. Local declarations before suspension are conservatively rejected in some common cases, and typed `for` loop indices are explicitly rejected. See [SDAG variable lifetime](sdag-variable-lifetime.md) for the observed Charmxi limitation, the persistent-field workaround, and concurrency concerns.

## Assessment

Phase 3 provides a **common declaration subset** validated by Charm++ v8.0.0,
not complete `.ci` source compatibility. The four benchmark applications are now ported and validated (including Prefix Sum in Phase 5.4). Additional declaration support should be prioritized from
observed benchmark blockers and runtime evidence.

- [Detailed declaration audit and priorities](declaration-coverage-audit.md)
- [Implementation roadmap](roadmap.md)
- [Charm# grammar](charmsharp-grammar-v0.1.md)

### Chare template validation status (Phase 6.3.4e)

Phase 6.3 is **complete for its restricted scalar-chare template subset**:
single type parameter, explicit and inferred concrete registrations, multiple
instantiations, generated shared-header ordering, and independent C++ consumer
use have executable pinned Charm++ v8.0.0 evidence. The translator does not
perform general C++ template instantiation or multi-file inference.

See the [Phase 6.3 template audit](chare-template-coverage-audit.md) for the
test-by-test evidence matrix, deferred user-defined/nested/dependent arguments,
header dependencies, inheritance restrictions, and the resolved
`template chare Main` convention interaction (now explicitly rejected).
