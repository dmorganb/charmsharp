# Charm# implementation coverage

Status: 2026-10-23. Target: Charm++ v8.0.0 and Charm# grammar v0.1.

For the **versioned, construct-by-construct comparison** against the
actual Charm++ `.ci` parser, see the
[Phase 3.4 declaration coverage audit](declaration-coverage-audit.md).
This page summarizes current implementation and its boundaries.

## Implemented (restricted subsets)

| Charm# construct | Lowering | Evidence / restriction |
| --- | --- | --- |
| `chare`, `mainchare` | `chare`/`mainchare` in `.ci`; `CBase_` C++ classes | Executable mainchare/worker test; one mainchare per input. |
| `group`, `nodegroup`, `array [ND] chare` | Parallel types in `.ci` | 1D/2D array and group/nodegroup executable tests; other positive dimension spellings parsed but untested. |
| `entry:` | Public C++ method + `.ci entry` | Simple constructors, void methods, declarations/inline bodies, and initializer lists on entry constructor definitions; parameter text copied without marshalling proof. |
| `[[charm::threaded]]`, `[[charm::sync]]`, `[[charm::reductiontarget]]` | `[threaded]`, `[sync]`, `[reductiontarget]` | Runtime threaded test, synchronous message-pointer interface, and reduction benchmark integration. |
| `message Name { ... };` | `message Name;` plus `CMessage_Name` class | Fixed-size runtime send/receive; no general serialization proof. |
| `readonly T x;`, `readonly T a[N];` | Readonly interface plus global C++ storage | Scalar runtime test; decimal array syntax checked. |
| Ordinary C++ base classes on chares/messages | Added after `CBase_Name` / `CMessage_Name` | Multiple earlier-defined local simple bases, access specifiers, runtime integration. |
| One `chare Derived : public Base` | `.ci` parallel inheritance + generated `CBase_Derived` | Inherited proxy method runtime test; base must be earlier-defined local chare. |
| Ordinary C++ outside Charm# declarations | Source-range preservation | Pass-through, **not** a C++ semantic parser. |

## Significant remaining boundaries

- **Entry attributes:** v8.0.0 recognizes many more than our `threaded`,
  `sync`, and `reductiontarget`; see the audit for the exact pinned attribute inventory.
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
- **Interface constructs:** `initnode`, `initproc`, `PUPable`,
  `extern module`, `include`, template entities, `.ci` namespaces,
  chare attributes and module dependency tracking lack native Charm# lowering.
- **Source/build model:** One module per input, no dependency discovery,
  conditional parallel declarations rejected, and no generated `#line`
  mapping.
- **SDAG:** `when`, implicit receive entries, `serial`, `overlap`,
  `case`, reference matching and SDAG loops are design proposals, not
  implemented.

## Assessment

Phase 3 provides a **common declaration subset** validated by Charm++ v8.0.0,
not complete `.ci` source compatibility. The next planned language phase is
basic SDAG. Additional declaration support should be prioritized from
benchmark blockers and runtime evidence.

- [Detailed declaration audit and priorities](declaration-coverage-audit.md)
- [Implementation roadmap](roadmap.md)
- [Charm# grammar](charmsharp-grammar-v0.1.md)
