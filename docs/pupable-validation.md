# Phase 7.4 — PUPable registration validation

Pinned reference: Charm++ v8.0.0, `src/xlat-i/xi-Member.C` and `src/util/pup.h`.
Charmxi's `.ci` `PUPable Name;` emits the out-of-class `PUPable_def(Name)`
and registration `PUPable_reg(Name)`. The C++ class must also contain
`PUPable_decl(Name)`; it must implement its own `pup(PUP::er&)`
serialization and migration constructor. Charm# adds only registration
metadata and required macro plumbing.

The supported subset is a top-level non-template C++ class directly inheriting
`public PUP::able` with a `Name(CkMigrateMessage*)` constructor and
`pup(PUP::er&)` method. The parser intentionally checks syntax/structure,
not full C++ semantic correctness, which is validated by the native compiler.

Runtime test: `make charm-pupable`. The script compiles a handwritten
reference and Charm#-generated `.ci` and C++ under pinned Charm++ v8.0.0,
then passes a polymorphic `std::shared_ptr<State>` to a group on PE 1
under `+p2`; outputs must match and report the reconstructed state (42).
No custom allocation, serialization or packing is generated.

Deferred: namespaces, nested classes, indirect or aliased PUP::able bases,
templated classes/specializations, existing manual PUP macro deduplication,
multi-TU registration ownership and full C++ signature analysis.
