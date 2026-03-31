strace syntax highlighting: TS vs Legacy comparison report
===========================================================

Sample file: `tests/syntax/samples/strace.strace`
Legacy reference: `misc/syntax/strace.syntax`
TS query: `misc/syntax-ts/queries-override/strace-highlights.scm`
TS colors: `misc/syntax-ts/colors.ini` `[strace]`

Aligned with legacy
-------------------

- Syscall names (`execve`, `brk`, `access`, `open`, `close`, `read`, `write`,
  `fstat64`, `mmap2`, `old_mmap`, `munmap`, `uname`, `stat64`, `fcntl64`,
  `select`, `ioctl`, `rt_sigaction`, `rt_sigprocmask`, `alarm`, `statfs`,
  `wait4`, `kill`, `exit_group`, `geteuid32`, `getegid32`, `getuid32`,
  `getgid32`, `gettimeofday`, `setresuid32`, `setresgid32`, `chmod`, `chown32`,
  `writev`, `readv`) -> TS uses `brightred` (`@function.special`) uniformly.
  Legacy uses per-syscall coloring (see differences below).
- Strings (`"/usr/bin/ls"`, `"hello world\n"`, etc.) -> `green` via `@string` in
  TS. Legacy does not highlight strings (they are part of the syscall argument
  pattern match).
- Comments (`/* 42 vars */`) -> `brown` via `@comment` in TS. Legacy does not
  recognize comments.

Intentional improvements over legacy
-------------------------------------

- TS highlights every component of strace output structurally: syscall names,
  arguments, strings, integers, pointers, return values, error names, error
  descriptions, PIDs, and punctuation each get distinct colors. Legacy only
  matches entire `syscall(...)` patterns as a single color.
- TS highlights return values after `=` as `yellow` via `@keyword`, making them
  visually distinct from arguments.
- TS highlights PIDs (e.g., `12345` before a syscall) as `brightcyan` via
  `@tag`, distinguishing process-prefixed lines.
- TS highlights pointers (`0x7ffd8a2e3f10`, `0x55a3c8e41000`) as `lightgray` via
  `@constant`, and integers as `lightgray` as well, providing consistent numeric
  coloring.
- TS highlights error names (`ENOENT`) as `brightred` via `@function.special`
  and error descriptions (`No such file or directory`) as `brown` via
  `@comment`, making error information visually distinct.
- TS highlights signal information (`SIGCHLD`, `SIGTERM`) as `brightred` via
  `@signal`/`@function.special`.
- TS highlights macro/flag names (like `WIFEXITED`, `WEXITSTATUS`) as `yellow`
  via `@keyword`.
- TS highlights punctuation (`(`, `)`, `,`, `=`) as `brightcyan` via
  `@delimiter`, improving structural readability.

Known shortcomings
------------------

- Legacy uses per-category syscall coloring: file I/O (`open`, `close`) ->
  `cyan`, read/write (`read`, `write`, `select`) -> `magenta`, memory
  (`old_mmap`, `mmap2`, `munmap`) -> `red`, process (`execve`) -> `brightgreen`,
  system info (`uname`) -> `brightblue`, permissions (`geteuid32`, `chmod`,
  `chown32`) -> `yellow`, signals (`rt_sigaction`, `kill`, `exit_group`) ->
  `brightred`. TS colors all syscalls uniformly as `brightred`, losing this
  per-category distinction.
- Legacy matches syscall-with-arguments as a single pattern like `open(*)`, so
  the entire line up to the closing paren gets the syscall's color. TS only
  colors the syscall name itself, leaving arguments to be colored by their own
  types.
- TS shows some structural artifacts in the signal line (`--- SIGCHLD {...}
  ---`) where the entire signal block is colored as `brightred`, which is
  correct but differs from legacy's uncolored treatment of that line.
- The `ioctl` line shows some `RED` (error/uncolored) spans in TS for
  struct-like arguments (`{B38400 opost isig...}`) that the parser does not
  fully understand.
- Legacy does not highlight `fstat64` closing paren correctly when the argument
  contains `}` before `)` -- the pattern `fstat64(*)` stops at the first `)`
  inside the struct. TS handles this correctly via structural parsing.
