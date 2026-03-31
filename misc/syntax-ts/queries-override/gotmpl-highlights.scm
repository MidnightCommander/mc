;; Tree-sitter highlight queries for Go Template language
;; Colors chosen to match Go conventions where applicable

;; Keywords -> yellow (keyword)
[
  "if"
  "else"
  "range"
  "with"
  "end"
  "template"
  "define"
  "block"
  "break"
  "continue"
] @keyword

;; Comments -> brown (comment)
(comment) @comment

;; Builtin functions (Go template + Sprig) -> brown (function.builtin)
;; Must be before @function: for equal ranges, first match wins
(function_call
  function: (identifier) @function.builtin
  (#any-of? @function.builtin
    ;; Go template builtins
    "and" "call" "eq" "ge" "gt" "html" "index" "js" "le" "len" "lt"
    "ne" "not" "or" "print" "printf" "println" "urlquery"
    ;; Sprig: strings
    "abbrev" "abbrevboth" "camelcase" "cat" "contains" "hasPrefix"
    "hasSuffix" "indent" "initials" "kebabcase" "lower" "nindent"
    "nospace" "plural" "quote" "repeat" "replace" "shuffle"
    "snakecase" "squote" "substr" "swapcase" "title" "trim" "trimAll"
    "trimPrefix" "trimSuffix" "trimall" "trunc" "untitle" "upper"
    "wrap" "wrapWith"
    ;; Sprig: strings conversion
    "atoi" "float64" "int" "int64" "toString" "toStrings" "toDecimal"
    ;; Sprig: defaults
    "coalesce" "compact" "default" "empty" "fail" "fromJson"
    "mustFromJson" "mustToDate" "mustToJson" "mustToPrettyJson"
    "mustToRawJson" "ternary" "toDate" "toJson" "toPrettyJson"
    "toRawJson"
    ;; Sprig: lists
    "append" "chunk" "concat" "first" "has" "initial" "last" "list"
    "mustAppend" "mustChunk" "mustCompact" "mustFirst" "mustHas"
    "mustInitial" "mustLast" "mustPrepend" "mustPush" "mustRest"
    "mustReverse" "mustSlice" "mustUniq" "mustWithout" "prepend"
    "push" "rest" "reverse" "seq" "slice" "sortAlpha" "uniq"
    "until" "untilStep" "without"
    ;; Sprig: dicts
    "deepCopy" "deepEqual" "dict" "dig" "get" "hasKey" "keys"
    "merge" "mergeOverwrite" "mustDeepCopy" "mustMerge"
    "mustMergeOverwrite" "omit" "pick" "pluck" "set" "unset" "values"
    ;; Sprig: math
    "add" "add1" "add1f" "addf" "biggest" "ceil" "div" "divf"
    "floor" "max" "maxf" "min" "minf" "mod" "mul" "mulf" "round"
    "sub" "subf"
    ;; Sprig: dates
    "ago" "date" "dateInZone" "dateModify" "date_in_zone"
    "date_modify" "duration" "durationRound" "htmlDate"
    "htmlDateInZone" "must_date_modify" "mustDateModify" "now"
    "unixEpoch"
    ;; Sprig: crypto
    "adler32sum" "bcrypt" "buildCustomCert" "decryptAES"
    "derivePassword" "encryptAES" "genCA" "genCAWithKey"
    "genPrivateKey" "genSelfSignedCert" "genSelfSignedCertWithKey"
    "genSignedCert" "genSignedCertWithKey" "htpasswd" "sha1sum"
    "sha256sum" "sha512sum"
    ;; Sprig: encoding
    "b32dec" "b32enc" "b64dec" "b64enc"
    ;; Sprig: paths
    "base" "clean" "dir" "ext" "isAbs" "osBase" "osClean" "osDir"
    "osExt" "osIsAbs"
    ;; Sprig: regex
    "regexFind" "regexFindAll" "regexMatch" "regexQuoteMeta"
    "regexReplaceAll" "regexReplaceAllLiteral" "regexSplit"
    "mustRegexFind" "mustRegexFindAll" "mustRegexMatch"
    "mustRegexReplaceAll" "mustRegexReplaceAllLiteral" "mustRegexSplit"
    ;; Sprig: reflection
    "kindIs" "kindOf" "typeIs" "typeIsLike" "typeOf"
    ;; Sprig: misc
    "all" "any" "env" "expandenv" "getHostByName" "hello" "join"
    "randAlpha" "randAlphaNum" "randAscii" "randBytes" "randInt"
    "randNumeric" "semver" "semverCompare" "split" "splitList"
    "splitn" "tuple" "urlJoin" "urlParse" "uuidv4"
    ;; Helm-specific functions
    "fromJsonArray" "fromYaml" "fromYamlArray" "include" "lookup"
    "mustToToml" "required" "toToml" "toYaml" "toYamlPretty" "tpl"))

;; Function calls -> brightcyan (function)
(function_call
  function: (identifier) @function)

;; Method calls -> brightcyan (function)
(method_call
  method: (selector_expression
    field: (field_identifier) @function))

;; Fields (.Values, .Release, .Name) -> white (property)
[
  (field)
  (field_identifier)
] @property

;; Variables ($name, $pool) -> brightred (variable.builtin)
(variable) @variable.builtin

;; Strings -> green (string)
[
  (interpreted_string_literal)
  (raw_string_literal)
  (rune_literal)
] @string

;; Escape sequences -> brightgreen (string.special)
(escape_sequence) @string.special

;; Numbers -> lightgray (number)
[
  (int_literal)
  (float_literal)
  (imaginary_literal)
] @number

;; Constants -> brightmagenta (constant.builtin)
[
  (true)
  (false)
  (nil)
] @constant.builtin

;; Operators -> brightcyan (operator)
"|" @operator
":=" @operator
"=" @operator

;; Delimiters -> brightcyan (delimiter)
"." @delimiter
"," @delimiter
"{{" @delimiter
"}}" @delimiter
"{{-" @delimiter
"-}}" @delimiter
"(" @delimiter
")" @delimiter
