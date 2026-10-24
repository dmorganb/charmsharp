# Charm# editor support

The editor integrations in this directory are deliberately syntax-only. They do
not parse or type-check Charm# and do not provide a language server.

- `vscode/`: a TextMate grammar which adds Charm# tokens to C++ highlighting.
- `zed/`: a Zed language extension using a pinned Tree-sitter C++ parser and
  Charm# highlighting queries. Its C++ parser does not yet recognize Charm#
  structural forms, so highlighting around SDAG and parallel declarations is
  best-effort until a dedicated Tree-sitter grammar is available.

Both integrations use the language name **Charm#** and the `.csh` suffix.
Note that `.csh` is also used for C-shell scripts; selecting Charm# should
be opt-in if you also edit shell scripts with that suffix.

## VS Code

Open `editors/vscode` as an extension project and use **Run Extension**
(`F5`) in VS Code. For installation without publishing, package that directory
with `vsce package` and install the resulting VSIX.

## Zed

In Zed, open the Extensions page and select **Install Dev Extension**.
Choose the `editors/zed` directory (the one containing `extension.toml`).
Zed downloads and compiles the pinned Tree-sitter grammar for the extension.

If `.csh` opens as C Shell, use Zed's language selector and choose **Charm#**.
You can also assign files via Zed's `file_types` settings.

## Validation checklist

Check these sources in both editors, using multiple themes:

- `examples/Counter.csh`: `chare`, `entry:`, C++ members and method body.
- `examples/MarshalledArrays.csh`: `message`, arrays, and entry parameters.
- `examples/SdagExtended.csh`: SDAG `when`, `overlap`, `case`, `forall`.
- `benchmarks/prefix-sum/charmsharp/prefix.csh`: nested SDAG and C++.
- An ordinary C++ body containing strings/comments with Charm# words: those
  words must not be highlighted as language keywords inside the literal/comment.

Future: introduce `tree-sitter-charmsharp` for accurate Zed parsing, and connect
both editor frontends to a common `charmsharp-lsp` when available.
