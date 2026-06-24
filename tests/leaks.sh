#!/usr/bin/env bash
# leaks.sh - valgrind leak + fd matrix for ft_select (Phase 6).
#
# Runs valgrind --leak-check=full --show-leak-kinds=all --track-fds=yes
# --error-exitcode=42 across ~14 contexts. Interactive contexts run valgrind AS
# THE PTY CHILD via build/leak_driver, with valgrind's report on its OWN fd
# (--log-file), so the report never mixes with the program's UI stream.
# Non-interactive contexts (0 args, redirected stdout, non-tty stdin, the
# guard-exit TERM cases) run valgrind directly. For each context we assert
# "definitely lost == 0", "indirectly lost == 0", and "ERROR SUMMARY: 0 errors"
# and print a per-context PASS/FAIL table.
#
# The project Makefile builds libft with SAFE=1 (the libc allocator), so every
# allocation is real malloc/free that Memcheck sees and accounts for. This matrix
# therefore proves genuine allocation balance (definitely+indirectly lost == 0),
# no leaked file descriptor (--track-fds is real), and no invalid access / crash
# under valgrind. (Build with `make SAFE=` to use libft's mmap ft_malloc instead,
# which is invisible to Memcheck.)

set -u

cd "$(dirname "$0")/.." || exit 2
ROOT="$(pwd)"
BIN="$ROOT/ft_select"
DRIVER="$ROOT/build/leak_driver"
LOGDIR="$ROOT/build/leaks"
TERM_OK="xterm"

VG_OPTS=(--leak-check=full --show-leak-kinds=all --track-fds=yes
	--errors-for-leak-kinds=all --error-exitcode=42)

mkdir -p "$LOGDIR"

# build the pty driver if missing (test tool, unrestricted libc).
if [ ! -x "$DRIVER" ]; then
	cc -Wall -Wextra -Werror "$ROOT/tests/leak_driver.c" -o "$DRIVER" -lutil \
		|| { echo "FATAL: cannot build leak_driver"; exit 2; }
fi
if [ ! -x "$BIN" ]; then
	echo "FATAL: $BIN not built (run make first)"; exit 2
fi

PASS=0
FAIL=0
declare -a ROWS

# verdict <logfile> -> echoes PASS/FAIL after checking lost==0 and 0 errors.
verdict() {
	local log="$1" def ind err
	[ -f "$log" ] || { echo "FAIL(nolog)"; return 1; }
	# "definitely lost: N bytes" — absent when nothing was lost (treat as 0).
	def=$(grep -oE 'definitely lost: [0-9,]+ bytes' "$log" | grep -oE '[0-9,]+' | tr -d ',' | head -1)
	ind=$(grep -oE 'indirectly lost: [0-9,]+ bytes' "$log" | grep -oE '[0-9,]+' | tr -d ',' | head -1)
	err=$(grep -oE 'ERROR SUMMARY: [0-9]+ errors' "$log" | grep -oE '[0-9]+' | head -1)
	def=${def:-0}; ind=${ind:-0}; err=${err:-0}
	if [ "$def" = 0 ] && [ "$ind" = 0 ] && [ "$err" = 0 ]; then
		echo "PASS"; return 0
	fi
	echo "FAIL(def=$def ind=$ind err=$err)"; return 1
}

record() {
	local name="$1" v="$2"
	ROWS+=("$(printf '  %-22s %s' "$name" "$v")")
	if [[ "$v" == PASS ]]; then PASS=$((PASS+1)); else FAIL=$((FAIL+1)); fi
}

# interactive context: name, term, script, args...  (valgrind = pty child)
run_pty() {
	local name="$1" term="$2" script="$3"; shift 3
	local log="$LOGDIR/$name.log"
	rm -f "$log"
	"$DRIVER" 24 80 "$term" "$log" "$script" "$BIN" "$@" >/dev/null 2>&1
	record "$name" "$(verdict "$log")"
}

# interactive context with a caller-chosen pty size (for resize / wide items).
run_pty_sz() {
	local name="$1" rows="$2" cols="$3" term="$4" script="$5"; shift 5
	local log="$LOGDIR/$name.log"
	rm -f "$log"
	"$DRIVER" "$rows" "$cols" "$term" "$log" "$script" "$BIN" "$@" >/dev/null 2>&1
	record "$name" "$(verdict "$log")"
}

# non-interactive context: valgrind runs the binary directly (stdin from
# /dev/null or a redirect); used for the guard-exit and 0-arg paths.
run_plain() {
	local name="$1"; shift
	local log="$LOGDIR/$name.log"
	rm -f "$log"
	# remaining args: env assignments, then -- , then program args
	env "$@" valgrind "${VG_OPTS[@]}" --log-file="$log" \
		"$BIN" </dev/null >/dev/null 2>&1
	record "$name" "$(verdict "$log")"
}

# helper: generate N short args.
many_args() { seq 1 "$1" | sed 's/^/item/' | tr '\n' ' '; }

echo "ft_select leak + fd matrix (valgrind $(valgrind --version 2>/dev/null))"
echo "Built SAFE=1 (libc allocator), so Memcheck sees every allocation: this"
echo "proves real allocation balance (definitely+indirectly lost == 0), no fd"
echo "leak (--track-fds), and no invalid access / crash across all contexts."
echo

# ---- interactive contexts (valgrind as the pty child) -------------------
run_pty enter        "$TERM_OK" "D;SP;CR"          a b c
run_pty esc          "$TERM_OK" "ESC"              a b c
run_pty sigint       "$TERM_OK" "s50;INT"          a b c
run_pty sigtstp      "$TERM_OK" "C-Z;s60;CONT;D;SP;CR" a b c
run_pty ctrl_c_byte  "$TERM_OK" "C-C"              a b c
run_pty delete_empty "$TERM_OK" "DEL;DEL"          a b
run_pty_sz resize 24 80 "$TERM_OK" \
	"W1x200;W1x1;W40x4;W24x80;D;SP;CR" alpha beta gamma
run_pty search       "$TERM_OK" "Tc;SP;CR"         apple banana cherry

# ---- args-shape contexts ------------------------------------------------
run_pty thousands    "$TERM_OK" "D;SP;CR"          $(many_args 2000)
LONGARG=$(printf 'x%.0s' $(seq 1 1500))
run_pty_sz longargs 24 1600 "$TERM_OK" "D;SP;CR"   short "$LONGARG"

# ---- TERM contexts (guard / caps failure under valgrind) ----------------
run_pty no_term      "-"          "ESC"            a b c
run_pty bad_term     "bogus123"   "ESC"            a b c

# ---- non-interactive contexts (valgrind direct) -------------------------
# 0 args: nothing to select, exits 0 immediately.
{ rm -f "$LOGDIR/zero_args.log"
  valgrind "${VG_OPTS[@]}" --log-file="$LOGDIR/zero_args.log" "$BIN" \
	</dev/null >/dev/null 2>&1
  record zero_args "$(verdict "$LOGDIR/zero_args.log")"; }
# redirected stdout: stdin is /dev/null (non-tty) -> guard exit, clean.
{ rm -f "$LOGDIR/redirected_stdout.log"
  valgrind "${VG_OPTS[@]}" --log-file="$LOGDIR/redirected_stdout.log" "$BIN" a b c \
	</dev/null >/dev/null 2>&1
  record redirected_stdout "$(verdict "$LOGDIR/redirected_stdout.log")"; }
# non-tty stdin from a pipe (also the guard path).
{ rm -f "$LOGDIR/non_tty_stdin.log"
  printf 'x\n' | valgrind "${VG_OPTS[@]}" \
	--log-file="$LOGDIR/non_tty_stdin.log" "$BIN" a b c >/dev/null 2>&1
  record non_tty_stdin "$(verdict "$LOGDIR/non_tty_stdin.log")"; }

echo "Context                  Result"
echo "------------------------ ------"
for r in "${ROWS[@]}"; do echo "$r"; done
echo
echo "TOTAL: $PASS passed, $FAIL failed ($((PASS+FAIL)) contexts). Logs in $LOGDIR/"

[ "$FAIL" -eq 0 ] && exit 0 || exit 1
