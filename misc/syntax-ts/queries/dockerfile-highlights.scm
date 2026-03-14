;; Tree-sitter highlight queries for Dockerfile
;; Colors aligned with MC's default dockerfile.syntax

;; Instructions -> yellow (@keyword) -- MC uses yellow for all instructions
[
  "FROM"
  "RUN"
  "CMD"
  "COPY"
  "ADD"
  "EXPOSE"
  "ENV"
  "WORKDIR"
  "ENTRYPOINT"
  "VOLUME"
  "USER"
  "ARG"
  "LABEL"
  "HEALTHCHECK"
  "SHELL"
  "AS"
  "ONBUILD"
  "STOPSIGNAL"
] @keyword

;; MAINTAINER -> brightred (@function.special) -- MC uses brightred (deprecated)
"MAINTAINER" @function.special

(comment) @comment

(image_spec
  name: (image_name) @function)
(image_spec
  tag: (image_tag) @string.special)
(image_spec
  digest: (image_digest) @string.special)

(double_quoted_string) @string
(single_quoted_string) @string
(unquoted_string) @string

;; Variable expansions -> brightgreen (@string.special) -- MC uses brightgreen for ${*}
(expansion) @string.special

;; Port numbers -> brightgreen (@number.builtin) -- MC uses brightgreen for numbers
(expose_port) @number.builtin

(label_pair
  key: (unquoted_string) @property)

(env_pair
  name: (unquoted_string) @variable)

(arg_instruction
  name: (unquoted_string) @variable)

;; Params (--mount, --from, etc.) -> brightmagenta (@delimiter.special)
;; MC uses brightmagenta for options
(param) @delimiter.special
