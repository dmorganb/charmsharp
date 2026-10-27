; Charm# syntax overlay for the pinned tree-sitter-cpp parser.
; The parser does not yet know Charm# declarations/SDAG. Keywords are highlighted
; when the C++ grammar recognizes them as identifiers or type identifiers.

(comment) @comment
(string_literal) @string
(raw_string_literal) @string
(char_literal) @string
(number_literal) @number
(primitive_type) @type.builtin
(type_identifier) @type
(field_identifier) @property
(preproc_include) @preproc
(preproc_def) @preproc

(call_expression
  function: (identifier) @function)

(function_declarator
  declarator: (identifier) @function)

[
  "class" "struct" "namespace" "public" "private" "protected"
  "template" "typename" "return" "if" "else" "for" "while"
  "switch" "case" "break" "continue" "new" "delete"
] @keyword

((identifier) @keyword
  (#match? @keyword "^(chare|mainchare|array|group|nodegroup|message|readonly|entry|when|overlap|forall|serial)$"))

((type_identifier) @keyword
  (#match? @keyword "^(chare|mainchare|array|group|nodegroup|message|readonly|entry|when|overlap|forall|serial)$"))
