# Charm# for Zed (experimental)

Install this directory as a Zed **dev extension** using Extensions →
Install Dev Extension. The extension registers `.csh` as Charm# and adds
highlighting through a pinned Tree-sitter C++ grammar and Charm# keyword
queries. It requires no Rust code or language server.

**Known limitation:** this is a C++ grammar fallback, not a proper Charm#
grammar. Parallel-object declarations and SDAG blocks can produce Tree-sitter
error nodes, and some Charm# keywords may not receive consistent colors.
Ordinary C++ in the same file should retain basic highlighting. The next
step is to create `tree-sitter-charmsharp` before publishing broad support.

See `../README.md` for source examples to check manually. The `.csh` file
extension also belongs to C-shell scripts; select the language explicitly
when this ambiguity matters.
