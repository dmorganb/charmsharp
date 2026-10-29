# Phase 7.1 — Variable-size message validation

Status: **complete for the restricted primitive-array subset** (2026-10-29).

## Supported syntax

- Standalone `char`, `short`, `int`, `long`, `float` and `double` member declarations of the form `T field[];`.
- The order of those declarations determines Charmxi's allocation sizes, as for `new (n1, n2) Message`.
- Unmodified ordinary fields/methods and locally resolved ordinary message bases.
- Generated `friend class CMessage_Name` for nonpublic variable-size fields.
- No custom pack/unpack/new logic in Charm#: Charmxi owns message lifetime and marshalling.

Other types, typedef-based and comma-separated/complex declarators, conditional fields and nested unsized arrays remain unsupported rather than guessed.

## Evidence

- Unit tests in `tests/tests.cpp` assert precise `.ci` metadata order, pointer rewriting, friend access, ordinary member preservation and actionable negative diagnostics.
- `tests/variable_message_integration.sh` runs against pinned Charm++ **v8.0.0** in the `charmpp-integration` CI job.
- `tests/fixtures/variable_message/VariableMessage.ci` and `.C` independently implement the matching Charm++ reference. The script compares the full normalized result lines from both executable variants at `+p2`.
- `examples/VariableMessageSingle.csh` exercises one array with zero and three elements, ensuring both messages arrive and are deleted.
- `examples/VariableMessageUnequal.csh` allocates two arrays with sizes 2 and 5, checks all values across remote transfer, and deletes the message.
- `examples/VariableMessage.csh` checks both arrays with equal lengths, plus ordinary fixed-size members and a member method.
- Green CI run for the implementation and reference tests: https://github.com/dmorganb/charmsharp/actions/runs/40898766957

## Limitations

This evidence establishes a **restricted executable subset**, not general C++ message-layout semantics. The translator does not validate arbitrary C++ types or deep inheritance layout, nor does it prove all possible Charm++ message ownership protocols. Errors from the underlying C++ compiler and Charmxi are still authoritative for complex definitions.
