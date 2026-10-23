# Charm# language coverage and remaining work

Status: 2026-10-23. Target: Charm++ v8.0.0, Charm# grammar v0.1.
This is an implementation inventory, not a promise that all accepted C++ parameter
types or declarations are valid Charm++ interfaces. The real Charm++ toolchain
remains the authority for semantics and marshalling.

## Implemented and tested

| Charm# source | Generated interface | Coverage |
| --- | --- | --- |
| `chare Name` | `chare Name` | Basic class and `CBase_Name` |
| `mainchare Name` | `mainchare Name` inside `mainmodule` | One per file/module |
| `group Name` / `nodegroup Name` | Matching declaration | Basic entries |
| `array [ND] chare Name` | `array [ND] Name` | Parses positive numeric dimensions; Charm++ decides which shapes are valid |
| `entry:` access section | One `entry` declaration per method | Constructors, `void` methods; declarations or inline definitions |
| `[[charm::threaded]] void f(...)` | `entry [threaded] void f(...)` | Executable integration test |
| `[[charm::sync]] Msg* f(...)` | `entry [sync] Msg* f(...)` | Built-in message-pointer declaration compiled in CI |
| Ordinary C++ | Preserved source ranges | Not a C++ semantic parser |

Multiple parallel types per input are supported; duplicate type names and
more than one mainchare are rejected. The sample executables exercise a
mainchare, ordinary chare, groups, nodegroups, 1D/2D arrays, and threaded
entries. Existing `.ci` files can coexist in a project, but Charm# currently
does not automatically coordinate identifiers or link modules across files.

## Entry attributes not yet lowered

The Charm++ manual describes these additional entry attributes. None is
supported by the current Charm# `[[charm::...]]` translator:

| Attribute | Charm++ semantics / why extra validation matters |
| --- | --- |
| `exclusive` | Mutual exclusion for nodegroup entry methods; placement matters |
| `nokeep` | Different message ownership and broadcast-buffer lifetime |
| `notrace` | Excludes an entry method from tracing |
| `appwork` | Marks application work for performance analysis |
| `immediate` | Nodegroup-only fast-path execution; communication-thread constraints |
| `expedited` | Bypasses priority queueing, not ordinary scheduling entirely |
| `inline` | Locally invoked method calls directly into C++; reentrancy implications |
| `local` | Only local group/array calls; permits non-void returns and otherwise non-marshallable parameters |
| `python` | Python-facing entry behavior |
| `reductiontarget` | Entry method as a reduction destination |
| `aggregate` | Aggregation-specific entry behavior |

The manual also describes `whenidle` for idle callbacks (requiring a
`bool` return and no arguments), although it is omitted from its main
attribute enumeration. Its availability and spelling should be verified
against the pinned compiler before implementation.

Other attribute gaps:

- Multiple attributes on a single method in supported Charm++ combinations
  (for example `threaded` plus `nokeep`). Charm# currently rejects the
  `threaded`/`sync` combination; other attributes are not recognized.
- Attribute arguments / advanced annotation syntax, and constraints specific
  to object kind and parameter ownership.
- Explicit negative tests using Charm++ v8.0.0 for invalid combinations.

**Important:** Attributes affect runtime behavior. A future implementation
must not translate an unfamiliar annotation by silently dropping it.

## Return types, parameters, and method declarations still missing

- Regular asynchronous entries support only `void` returns. This is
  intentional: arbitrary C++ return values do not turn an asynchronous proxy
  invocation into a synchronous call.
- `[[charm::sync]]` currently permits only a simple `Type*` return shape.
  Charm++ also supports synchronous returns of PUP-serializable value types;
  qualified, namespaced, templated, pointer/reference, and other compound
  spellings need syntax and semantic rules.
- `local` entry methods can return non-void values, but the attribute and
  corresponding return types have not been implemented.
- Trailing `const`, `noexcept`, ref qualifiers, trailing return types,
  constructor initializer lists, default arguments, operator/function-pointer
  declarators, template entry methods, C++20 concepts/requires, and advanced
  annotations are outside the accepted signature subset.
- Ordinary parameter text is copied into the generated `.ci` signature;
  Charm# does not currently prove that a type is a Charm++ message, a valid
  marshalled parameter, or PUP-serializable. Overloading and signature
  equivalence are checked textually, not with full C++ type resolution.
- The translator distinguishes inline definitions and bare declarations,
  but it does not resolve or validate separate out-of-line definitions.
- Constructors with `threaded` or `sync` annotations are rejected. Ordinary
  migration constructors may still be written as non-entry C++ members.

## Other declaration / module features still missing

| Feature | Grammar v0.1 status | Implementation gap |
| --- | --- | --- |
| `message` declarations | Included | Generate interface declaration, C++ `CMessage_...` base, and verify allocation/serialization semantics; variable-size messages need separate handling |
| `readonly` declarations | Included | Generate `.ci` metadata and C++ storage while respecting Charm++ startup initialization |
| User-provided inheritance | Proposed in [grammar Section 3.1](charmsharp-grammar-v0.1.md) | Classify ordinary C++ vs. parallel bases, normalize `CBase_...`, diagnose unresolved external bases and preserve constructor forwarding |
| Namespaces and nested declarations | Deferred decision | Define naming / generated proxy policy; currently rejected for parallel types |
| Module declarations / imports | Partly design issue | `mainmodule` auto-selection and one module per source work; `extern module`, multiple files, imports, registration, and collisions need design |
| Additional Charm++ interface directives | Not fully enumerated in v0.1 | Examples include initialization hooks and template/module-related constructs; require separate scoping |
| Preprocessor-conditional parallel types | Deferred | Reject currently; conditionally generated metadata needs matching C++ and `.ci` behavior |
| Source mapping | Deferred | Generate `#line` mappings or an external source map for diagnostics |

## SDAG: the largest remaining language transformation

The *proposed* v0.1 language includes the following, none of which is yet
implemented:

1. Identify an SDAG-aware entry method and move its SDAG definition into
   generated `.ci`, leaving valid generated C++ declarations/hook code.
2. Recognize `when receive(...)` and create/validate implicit or explicit
   receive entry declarations.
3. Partition adjacent ordinary C++ statements into maximal safe `serial`
   blocks; preserve their semantics.
4. Support `when name[reference](...)`, multiple receive signatures,
   `overlap`, `case`, and the specified `if`, `for`, `while`, and
   `forall` controls.
5. Detect unsafe local-variable lifetimes across suspension points and
   reject them with useful diagnostics; do not silently promote locals.
6. Test message arrival ordering, early buffering, matching, and completion
   against equivalent handwritten SDAG programs.

**Implementation priority:** one `when` and implicit receive declaration
first; then automatic `serial` grouping, reference matching, and complex
control flow. In parallel, finish the easier `message`/`readonly` declaration
translation before claiming grammar-v0.1 coverage.

## References

- [Charm++ manual — Entry Method Attributes](https://charm.readthedocs.io/en/latest/charm%2B%2B/manual.html#entry-method-attributes)
- [Charm++ tutorial — Threaded Entry Methods](https://charmplusplus.org/charm-tutorial/ch9_threaded/)
- [Charm++ tutorial — Structured Dagger](https://charmplusplus.org/charm-tutorial/ch6_sdag/)
- `docs/roadmap.md` and `charmsharp-grammar-v0.1.md` for the project's planned syntax.
