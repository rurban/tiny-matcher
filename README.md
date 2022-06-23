# tiny-matcher

A small and portable recursive [Regular Expression](https://en.wikipedia.org/wiki/Regular_expression) (regex) matcher written in C.
Dynamic (no compilation), bounded recursion.

This variant uses heap memory and the string stdlib.
There should be variants for embedded without malloc
and the string lib. There should be optional (-D_UNICODE) UTF-8 support.

Initially written by Reini Urban 2003 for asterisk, see 
[rurban/asterisk/matcher](https://github.com/rurban/asterisk-matcher).

MIT License

Influenced by code by Steffen Offermann 1991 (public domain)
http://www.cs.umu.se/~isak/Snippets/xstrcmp.c

### Supported regex-operators
The following features / regex-operators will be supported.

  - `.`         Dot, matches any character
  - `^`         Start anchor, matches beginning of string
  - `$`         End anchor, matches end of string
  - `*`         Asterisk, match zero or more (greedy)
  - `+`         Plus, match one or more (greedy)
  - `?`         Question, match zero or one (non-greedy)
  - `{n}`       Exact Quantifier
  - `{n,}`      Match n or more times
  - `{,m}`      Match m or less times
  - `{n,m}`     Match n to m times
  - `*?`        Shortest zero or more match (non-greedy)
  - `+?`        Shortest one or more match (non-greedy)
  - `[abc]`     Character class, match if one of {'a', 'b', 'c'}
  - `[^abc]`   Inverted class, match if NOT one of {'a', 'b', 'c'}
  - `[a-zA-Z]` Character ranges, the character set of the ranges { a-z | A-Z }
  - `\s`       Whitespace, '\t' '\f' '\r' '\n' '\v' and spaces
  - `\S`       Non-whitespace
  - `\w`       Alphanumeric, [a-zA-Z0-9_]
  - `\W`       Non-alphanumeric
  - `\d`       Digits, [0-9]
  - `\D`       Non-digits
  - `|`        Branch Or, e.g. a|A, \w|\s
  - `(...)`    Groups and Capturing
