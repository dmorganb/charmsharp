# Charm# for VS Code

Syntax-only language support for `.csh` files. This is a TextMate grammar,
not an LSP server, and it reuses VS Code's C++ grammar for normal C++ code.

Development: open this directory in VS Code and press F5 to launch an extension
development host. Open `../../examples/SdagExtended.csh` and select Charm#.

To package: run `npx @vscode/vsce package` in this directory (Node/npm needed).
The project root's `editors/README.md` contains a common validation checklist.

Note: `.csh` is also a C-shell extension; change the language selector/file
association if needed.
