# ft_select

An interactive terminal **list selector** built with termcaps: it shows the
command-line arguments as a navigable list, lets you select some, and prints the
chosen ones back to the shell — meant for backtick use:

```sh
set files = `./ft_select *.c`
rm `./ft_select ~/Downloads/*`
```

## Build & run

```sh
make           # builds the bundled libft, then ft_select
./ft_select a b c d e
```

The libft submodule is at `vendor/libft` and is built automatically. By default
the Makefile builds it with **`SAFE=1`** (libc allocator) so valgrind can verify
every allocation; `make SAFE=` switches to libft's in-house `ft_malloc`.

## Controls

| Key | Action |
|---|---|
| Arrows ←↑↓→ | move the cursor (circular, across columns) |
| Space | select / deselect the current item, then advance |
| Enter | print the selected items (argv order, space-separated) and exit |
| Esc | quit, print nothing |
| Backspace / Delete | remove the current item from the list (empty list ⇒ quit like Esc) |
| *printable keys* | **dynamic search** — jump the cursor to the next matching item |
| Ctrl-Z / `fg` | suspend / resume (terminal restored while suspended) |
| window resize | reflow into multiple columns, keeping selections |

Display: selected = **inverse video**, cursor = **underline**, both compose;
filename items are **colorized by extension**.

## Design

- **Data model** — items are a libft `t_vec<char *>` (each `ft_strdup`'d from
  argv); a parallel `t_vec<unsigned char>` holds the selection flags; the whole
  frame is rendered into a reused `t_vec<char>`. The only global is one
  `volatile sig_atomic_t g_signo`; everything else is threaded through `t_app *`.
- **Pure/terminal split** — all the logic (arg ingest, column geometry,
  navigation, select/delete, output formatting) lives in pure, terminal-free
  functions (`args/layout/navigate/select_ops/output/search/color.c`) declared in
  `include/ft_select.h`. This is the unit-test seam: it lets ~1900 cases run
  headless. The terminal code (`term/input/render/signals/loop.c`) is exercised
  end-to-end under a pseudo-terminal.
- **Column layout** — `ls`-style column-major packing: one column when the rows
  fit, otherwise `ceil(n / rows)` columns clamped to what the width allows; a
  bijection maps each item index ↔ (row, col). Every divisor is floored at 1, so
  a 1×1 or absurd window never divides by zero — it just draws nothing until the
  window grows back.
- **Rendering** — the entire frame (cursor moves + attributes + text) is assembled
  into one buffer and emitted with a **single `write`**, so there is no flicker
  and no per-cell syscall storm (the manual equivalent of double-buffering).
- **Always restore the terminal** — raw mode is entered with `tcsetattr`, and a
  restore (show cursor + reset attrs + `tcsetattr` back to the saved termios) runs
  on *every* exit path: Enter, Esc, delete-to-empty, init failure, and from the
  signal handlers (SIGINT/SIGTERM free the heap and exit; SIGTSTP restores cooked
  mode before the stop).

## Termcap = libft's own (no `-ltermcap`)

This project uses the termcap implementation inside the bundled libft
(`tgetent/tgetstr/tgetnum/tgetflag/tgoto/tputs`), **not** the system `-ltermcap`.
Consequences: a single self-contained build, no duplicate-symbol clash, and the
caps come from `$TERMCAP` / libft's built-in fallback (which provides
`cl cm ce so us ue me` for `xterm`). To switch to the system library instead, see
the `make TERMCAP=system` contingency note in the defense section.

## Bonuses

All four: **dynamic search** (type to jump), **clean exit** (erase the drawing and
leave the prompt on the next line), **extension colorization** (filenames colored
like `ls`), and **horizontal column scroll** (columns follow the cursor when the
window is too narrow).

## Testing

```sh
make test    # ~1900 distinct pure-logic cases (invariant/property style) — µtest framework
make e2e     # 18 forkpty end-to-end scenarios against the real binary
make leaks   # valgrind leak + fd matrix across 15 contexts (real libc-allocator check)
make norm    # norminette (src + include): clean
```

The tests run on a small **custom gtest-style C framework** (`tests/framework/`):
`TEST`/`ASSERT_*`/`EXPECT_*`/`SUBCASE`, **fork-per-test isolation** (so a crash or
hang is reported, not fatal to the suite), and forkpty helpers. The leak matrix
proves real allocation balance (`definitely+indirectly lost == 0`), no leaked file
descriptor, and no invalid access — including under SIGINT, SIGTSTP, resize storms,
delete-to-empty, thousands of args, missing/unknown `$TERM`, and redirected stdout.

## Defense notes

- **Why libft termcap?** The subject mandates termcaps; the bundled libft already
  implements the full termcap API, so the project links one archive and stays
  self-contained. It is functionally complete here (fallback caps for `xterm`).
  *Contingency:* `make TERMCAP=system` (documented) swaps to `-ltermcap` with
  header isolation if a corrector requires the system library.
- **Allocator (`SAFE=1`).** Built with libft's libc-allocator mode so "no leaks"
  is genuinely valgrind-verifiable (Memcheck sees every malloc/free). libft's
  mmap-backed `ft_malloc` is still available via `make SAFE=`.
- **Ctrl-Z without `raise`/`kill`.** Those are not in the allowed function list, so
  the SIGTSTP handler restores cooked mode and re-arms the default disposition
  (`signal(SIGTSTP, SIG_DFL)`); the kernel then performs the real job-control stop.
  The terminal is always left sane while suspended, using only allowed calls.
- **Terminal restore is guaranteed** even on signal: proved out-of-band by the
  e2e harness reading the pty's termios (`ICANON`/`ECHO` restored) after the child
  dies.
