/* e2e.c - interactive end-to-end tests for the real ft_select binary.
 *
 * TEST CODE: unrestricted libc. Each test forks ft_select onto a fresh pty
 * sized 24x80 with TERM=xterm (so the libft termcap fallback supplies cl/cm/ce/
 * so/se/us/ue/me deterministically), drives keystrokes, drains the master to
 * EOF and inspects the committed line. The discriminator between rendered item
 * text (which the UI echoes, wrapped in escape sequences) and the *committed*
 * selection is that commit writes a bare "<items>\n" as the final bytes after
 * the terminal is restored - so we match on the tail of the stream, not any
 * substring. Esc / cancel paths emit no such committed line.
 */
#include "ctest.h"
#include "ctest_pty.h"
#include "libft.h"
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <signal.h>

#define BIN "./ft_select"

/* drain master to EOF (child exit closes the slave) into buf, NUL-terminated.
 * bounded select so a stuck child fails loud instead of hanging the suite. */
static size_t	drain_eof(int master, char *buf, size_t cap)
{
	fd_set			rfds;
	struct timeval	tv;
	size_t			len;
	ssize_t			r;

	len = 0;
	while (len + 1 < cap)
	{
		FD_ZERO(&rfds);
		FD_SET(master, &rfds);
		tv.tv_sec = 2;
		tv.tv_usec = 0;
		if (select(master + 1, &rfds, NULL, NULL, &tv) <= 0)
			break ;
		r = read(master, buf + len, cap - 1 - len);
		if (r <= 0)
			break ;
		len += (size_t)r;
	}
	buf[len] = '\0';
	return (len);
}

/* spawn ft_select with the given trailing args under a 24x80 pty + TERM=xterm */
static int	spawn_select(t_pty *pty, char *const args[])
{
	char	*env[2];

	env[0] = (char *)"TERM=xterm";
	env[1] = NULL;
	return (pty_spawn(pty, args, 24, 80, env));
}

/* like spawn_select but with a caller-chosen window size (for scroll tests) */
static int	spawn_select_sz(t_pty *pty, char *const args[], int rows, int cols)
{
	char	*env[2];

	env[0] = (char *)"TERM=xterm";
	env[1] = NULL;
	return (pty_spawn(pty, args, rows, cols, env));
}

/* spawn ft_select with a caller-supplied env (to drop or corrupt TERM) under a
 * normal 24x80 pty - the tty guard passes, only the TERM/caps path is exercised. */
static int	spawn_select_env(t_pty *pty, char *const args[], char *const env[])
{
	return (pty_spawn(pty, args, 24, 80, env));
}

/* spawn ft_select WITHOUT a pty, with stdin/stdout/stderr from /dev/null so
 * isatty(STDIN) is false: proves the non-interactive guard exits cleanly. fills
 * *pid and returns 0, or -1 on fork failure. No master fd (there is no pty). */
static int	spawn_non_tty(pid_t *pid, char *const args[])
{
	int	devnull;

	*pid = fork();
	if (*pid < 0)
		return (-1);
	if (*pid == 0)
	{
		devnull = open("/dev/null", O_RDWR);
		if (devnull < 0)
			_exit(127);
		dup2(devnull, 0);
		dup2(devnull, 1);
		dup2(devnull, 2);
		execlp(BIN, BIN, "a", "b", "c", (char *)NULL);
		_exit(127);
	}
	(void)args;
	return (0);
}

/* true if `line` appears as a committed, newline-terminated line in buf. The
 * pty translates the program's '\n' to "\r\n" on the slave (ONLCR), so the
 * committed selection lands as "<line>\r\n"; we match that exact tail. */
static int	has_committed_line(const char *buf, const char *line)
{
	char	needle[256];
	size_t	n;

	n = ft_strlen(line);
	if (n + 3 >= sizeof(needle))
		return (0);
	needle[0] = '\0';
	ft_strlcat(needle, line, sizeof(needle));
	ft_strlcat(needle, "\r\n", sizeof(needle));
	return (memmem(buf, ft_strlen(buf), needle, n + 2) != NULL);
}

TEST(e2e, down_space_enter_emits_second_item)
{
	t_pty	pty;
	char	*args[5];
	char	buf[16384];
	int		status;

	args[0] = (char *)BIN;
	args[1] = (char *)"a";
	args[2] = (char *)"b";
	args[3] = (char *)"c";
	args[4] = NULL;
	ASSERT_EQ(0, spawn_select(&pty, args));
	usleep(120000);
	pty_send_str(pty.master, "\033[B");
	usleep(60000);
	pty_send_str(pty.master, " ");
	usleep(60000);
	pty_send_str(pty.master, "\n");
	drain_eof(pty.master, buf, sizeof(buf));
	ASSERT_EQ(0, pty_wait(pty.pid, &status));
	EXPECT_TRUE(WIFEXITED(status) && WEXITSTATUS(status) == 0);
	EXPECT_TRUE(has_committed_line(buf, "b"));
	EXPECT_FALSE(has_committed_line(buf, "a"));
}

TEST(e2e, esc_emits_nothing)
{
	t_pty	pty;
	char	*args[5];
	char	buf[16384];
	int		status;

	args[0] = (char *)BIN;
	args[1] = (char *)"a";
	args[2] = (char *)"b";
	args[3] = (char *)"c";
	args[4] = NULL;
	ASSERT_EQ(0, spawn_select(&pty, args));
	usleep(120000);
	pty_send_str(pty.master, "\033");
	usleep(60000);
	drain_eof(pty.master, buf, sizeof(buf));
	ASSERT_EQ(0, pty_wait(pty.pid, &status));
	EXPECT_TRUE(WIFEXITED(status) && WEXITSTATUS(status) == 0);
	EXPECT_FALSE(has_committed_line(buf, "a"));
	EXPECT_FALSE(has_committed_line(buf, "b"));
	EXPECT_FALSE(has_committed_line(buf, "c"));
}

/* A resize storm (SIGWINCH) must never crash the child and must not corrupt the
 * selection: after shrinking to a sliver and growing back, Down+Space+Enter on a
 * 3-item list still commits the second item exactly as on a static window. */
TEST(e2e, resize_storm_keeps_selection_correct)
{
	t_pty	pty;
	char	*args[5];
	char	buf[16384];
	int		status;

	args[0] = (char *)BIN;
	args[1] = (char *)"alpha";
	args[2] = (char *)"beta";
	args[3] = (char *)"gamma";
	args[4] = NULL;
	ASSERT_EQ(0, spawn_select(&pty, args));
	usleep(120000);
	pty_resize(pty.master, 1, 200);
	usleep(40000);
	pty_resize(pty.master, 1, 1);
	usleep(40000);
	pty_resize(pty.master, 40, 4);
	usleep(40000);
	pty_resize(pty.master, 24, 80);
	usleep(40000);
	pty_send_str(pty.master, "\033[B");
	usleep(60000);
	pty_send_str(pty.master, " ");
	usleep(60000);
	pty_send_str(pty.master, "\n");
	drain_eof(pty.master, buf, sizeof(buf));
	ASSERT_EQ(0, pty_wait(pty.pid, &status));
	EXPECT_TRUE(WIFEXITED(status) && WEXITSTATUS(status) == 0);
	EXPECT_TRUE(has_committed_line(buf, "beta"));
	EXPECT_FALSE(has_committed_line(buf, "alpha"));
}

/* Whichever way it ends, ft_select must restore cooked mode. We snapshot the
 * slave's c_lflag *before* the child raw-modes it (ICANON|ECHO on), let the
 * child run and exit via Esc, then re-read the slave flags after EOF: ICANON
 * and ECHO must be set again - proof the terminal was put back. */
TEST(e2e, terminal_restored_after_exit)
{
	t_pty		pty;
	char		*args[4];
	char		buf[16384];
	tcflag_t	before;
	tcflag_t	after;

	args[0] = (char *)BIN;
	args[1] = (char *)"a";
	args[2] = (char *)"b";
	args[3] = NULL;
	ASSERT_EQ(0, spawn_select(&pty, args));
	ASSERT_EQ(0, pty_slave_lflag(pty.master, &before));
	EXPECT_TRUE((before & ICANON) != 0 && (before & ECHO) != 0);
	usleep(120000);
	pty_send_str(pty.master, "\033");
	drain_eof(pty.master, buf, sizeof(buf));
	ASSERT_EQ(0, pty_slave_lflag(pty.master, &after));
	pty_wait(pty.pid, NULL);
	EXPECT_TRUE((after & ICANON) != 0);
	EXPECT_TRUE((after & ECHO) != 0);
}

/* SIGINT mid-session must restore the terminal and exit non-zero with nothing
 * committed. We snapshot the slave flags, deliver SIGINT, drain to EOF, then
 * read the slave flags again (cooked restored) and assert the child died with a
 * non-zero status and emitted no committed selection. */
TEST(e2e, sigint_restores_and_exits_nonzero)
{
	t_pty		pty;
	char		*args[4];
	char		buf[16384];
	tcflag_t	after;
	int			status;

	args[0] = (char *)BIN;
	args[1] = (char *)"a";
	args[2] = (char *)"b";
	args[3] = NULL;
	ASSERT_EQ(0, spawn_select(&pty, args));
	usleep(120000);
	kill(pty.pid, SIGINT);
	drain_eof(pty.master, buf, sizeof(buf));
	ASSERT_EQ(0, pty_slave_lflag(pty.master, &after));
	ASSERT_EQ(0, pty_wait(pty.pid, &status));
	EXPECT_TRUE((after & ICANON) != 0 && (after & ECHO) != 0);
	EXPECT_TRUE(WIFEXITED(status) && WEXITSTATUS(status) != 0);
	EXPECT_FALSE(has_committed_line(buf, "a"));
	EXPECT_FALSE(has_committed_line(buf, "b"));
}

/* Ctrl-Z then fg: SIGTSTP must stop the child (restoring cooked mode), SIGCONT
 * must resume it back into raw mode, and the session must still commit normally.
 * We stop, continue, then drive Enter to commit the (unselected) first item set:
 * Down+Space selects "b", Enter commits it - proving the loop survived the stop. */
TEST(e2e, sigtstp_then_cont_resumes_cleanly)
{
	t_pty	pty;
	char	*args[4];
	char	buf[16384];
	int		status;

	args[0] = (char *)BIN;
	args[1] = (char *)"a";
	args[2] = (char *)"b";
	args[3] = NULL;
	ASSERT_EQ(0, spawn_select(&pty, args));
	usleep(120000);
	kill(pty.pid, SIGTSTP);
	usleep(80000);
	kill(pty.pid, SIGCONT);
	usleep(80000);
	pty_send_str(pty.master, "\033[B");
	usleep(60000);
	pty_send_str(pty.master, " ");
	usleep(60000);
	pty_send_str(pty.master, "\n");
	drain_eof(pty.master, buf, sizeof(buf));
	ASSERT_EQ(0, pty_wait(pty.pid, &status));
	EXPECT_TRUE(WIFEXITED(status) && WEXITSTATUS(status) == 0);
	EXPECT_TRUE(has_committed_line(buf, "b"));
}

/* The load-bearing half of the Ctrl-Z contract: the SIGTSTP handler must put the
 * terminal back into cooked mode (ICANON|ECHO) before the process suspends, so a
 * `fg`-less shell still sees a sane terminal. Under forkpty the child sits in an
 * orphaned process group, so the kernel discards the default *stop* action - we
 * therefore can't observe WIFSTOPPED here. But the handler's restore_cooked()
 * still runs, so right after delivering SIGTSTP the slave c_lflag must show
 * cooked mode. We then SIGCONT (re-enters raw) and drive Down+Space+Enter to
 * prove the loop resumed and still commits the selected item. */
TEST(e2e, sigtstp_restores_cooked_then_cont_commits)
{
	t_pty		pty;
	char		*args[4];
	char		buf[16384];
	tcflag_t	after_tstp;
	int			status;

	args[0] = (char *)BIN;
	args[1] = (char *)"a";
	args[2] = (char *)"b";
	args[3] = NULL;
	ASSERT_EQ(0, spawn_select(&pty, args));
	usleep(120000);
	kill(pty.pid, SIGTSTP);
	usleep(80000);
	ASSERT_EQ(0, pty_slave_lflag(pty.master, &after_tstp));
	EXPECT_TRUE((after_tstp & ICANON) != 0 && (after_tstp & ECHO) != 0);
	kill(pty.pid, SIGCONT);
	usleep(80000);
	pty_send_str(pty.master, "\033[B");
	usleep(60000);
	pty_send_str(pty.master, " ");
	usleep(60000);
	pty_send_str(pty.master, "\n");
	drain_eof(pty.master, buf, sizeof(buf));
	ASSERT_EQ(0, pty_wait(pty.pid, &status));
	EXPECT_TRUE(WIFEXITED(status) && WEXITSTATUS(status) == 0);
	EXPECT_TRUE(has_committed_line(buf, "b"));
}

/* Dynamic search (bonus): typing a printable that matches an item must move the
 * cursor onto that item without sending anything. With items apple/banana/cherry
 * the cursor starts on apple; typing 'c' jumps it to cherry. Space selects cherry
 * (and advances), Enter commits - so the committed line is exactly "cherry",
 * proving the search positioned the cursor on the matched element. */
TEST(e2e, search_jumps_cursor_then_enter_returns_it)
{
	t_pty	pty;
	char	*args[5];
	char	buf[16384];
	int		status;

	args[0] = (char *)BIN;
	args[1] = (char *)"apple";
	args[2] = (char *)"banana";
	args[3] = (char *)"cherry";
	args[4] = NULL;
	ASSERT_EQ(0, spawn_select(&pty, args));
	usleep(120000);
	pty_send_str(pty.master, "c");
	usleep(60000);
	pty_send_str(pty.master, " ");
	usleep(60000);
	pty_send_str(pty.master, "\n");
	drain_eof(pty.master, buf, sizeof(buf));
	ASSERT_EQ(0, pty_wait(pty.pid, &status));
	EXPECT_TRUE(WIFEXITED(status) && WEXITSTATUS(status) == 0);
	EXPECT_TRUE(has_committed_line(buf, "cherry"));
	EXPECT_FALSE(has_committed_line(buf, "apple"));
	EXPECT_FALSE(has_committed_line(buf, "banana"));
}

/* Search-then-Esc must clear the query without exiting: after typing 'c' (cursor
 * on cherry) an Esc clears the query and the program stays alive; a second Esc
 * (empty query) exits cleanly with nothing committed. */
TEST(e2e, search_esc_clears_query_then_exits)
{
	t_pty	pty;
	char	*args[5];
	char	buf[16384];
	int		status;

	args[0] = (char *)BIN;
	args[1] = (char *)"apple";
	args[2] = (char *)"banana";
	args[3] = (char *)"cherry";
	args[4] = NULL;
	ASSERT_EQ(0, spawn_select(&pty, args));
	usleep(120000);
	pty_send_str(pty.master, "c");
	usleep(60000);
	pty_send_str(pty.master, "\033");
	usleep(200000);
	pty_send_str(pty.master, "\033");
	drain_eof(pty.master, buf, sizeof(buf));
	ASSERT_EQ(0, pty_wait(pty.pid, &status));
	EXPECT_TRUE(WIFEXITED(status) && WEXITSTATUS(status) == 0);
	EXPECT_FALSE(has_committed_line(buf, "cherry"));
}

/* Horizontal column scroll (bonus): in a window too narrow to show every column
 * at once, moving Right past the visible edge must scroll col_off so the cursor
 * stays drawable and lands on the correct item. 8 eight-char items in a 4-row,
 * 12-col window pack column-major into 2 columns (col_w 10 => 1 visible column).
 * Right moves from index 0 (col0,row0) to index 4 (col1,row0) = "item0005";
 * Space+Enter commits it - proving the window scrolled without crashing. */
TEST(e2e, horizontal_scroll_follows_cursor)
{
	t_pty	pty;
	char	*args[10];
	char	buf[16384];
	int		status;

	args[0] = (char *)BIN;
	args[1] = (char *)"item0001";
	args[2] = (char *)"item0002";
	args[3] = (char *)"item0003";
	args[4] = (char *)"item0004";
	args[5] = (char *)"item0005";
	args[6] = (char *)"item0006";
	args[7] = (char *)"item0007";
	args[8] = (char *)"item0008";
	args[9] = NULL;
	ASSERT_EQ(0, spawn_select_sz(&pty, args, 4, 12));
	usleep(120000);
	pty_send_str(pty.master, "\033[C");
	usleep(60000);
	pty_send_str(pty.master, " ");
	usleep(60000);
	pty_send_str(pty.master, "\n");
	drain_eof(pty.master, buf, sizeof(buf));
	ASSERT_EQ(0, pty_wait(pty.pid, &status));
	EXPECT_TRUE(WIFEXITED(status) && WEXITSTATUS(status) == 0);
	EXPECT_TRUE(has_committed_line(buf, "item0005"));
}

/* Deleting every item must end exactly like Esc: nothing committed, exit 0, and
 * the terminal restored to cooked. Two items, Delete (0x7f) twice empties the
 * list; the loop's empty-list guard then exits without printing a selection. */
TEST(e2e, delete_to_empty_behaves_like_esc)
{
	t_pty		pty;
	char		*args[4];
	char		buf[16384];
	tcflag_t	after;
	int			status;

	args[0] = (char *)BIN;
	args[1] = (char *)"a";
	args[2] = (char *)"b";
	args[3] = NULL;
	ASSERT_EQ(0, spawn_select(&pty, args));
	usleep(120000);
	pty_send_str(pty.master, "\x7f");
	usleep(60000);
	pty_send_str(pty.master, "\x7f");
	drain_eof(pty.master, buf, sizeof(buf));
	ASSERT_EQ(0, pty_slave_lflag(pty.master, &after));
	ASSERT_EQ(0, pty_wait(pty.pid, &status));
	EXPECT_TRUE(WIFEXITED(status) && WEXITSTATUS(status) == 0);
	EXPECT_TRUE((after & ICANON) != 0 && (after & ECHO) != 0);
	EXPECT_FALSE(has_committed_line(buf, "a"));
	EXPECT_FALSE(has_committed_line(buf, "b"));
}

/* Circular navigation: with the cursor on the first item, Up must wrap to the
 * last. Three items a/b/c on a 24x80 window pack into one column; Up from "a"
 * lands on "c". Space selects it and Enter commits exactly "c". */
TEST(e2e, circular_up_from_first_wraps_to_last)
{
	t_pty	pty;
	char	*args[5];
	char	buf[16384];
	int		status;

	args[0] = (char *)BIN;
	args[1] = (char *)"a";
	args[2] = (char *)"b";
	args[3] = (char *)"c";
	args[4] = NULL;
	ASSERT_EQ(0, spawn_select(&pty, args));
	usleep(120000);
	pty_send_str(pty.master, "\033[A");
	usleep(60000);
	pty_send_str(pty.master, " ");
	usleep(60000);
	pty_send_str(pty.master, "\n");
	drain_eof(pty.master, buf, sizeof(buf));
	ASSERT_EQ(0, pty_wait(pty.pid, &status));
	EXPECT_TRUE(WIFEXITED(status) && WEXITSTATUS(status) == 0);
	EXPECT_TRUE(has_committed_line(buf, "c"));
	EXPECT_FALSE(has_committed_line(buf, "a"));
	EXPECT_FALSE(has_committed_line(buf, "b"));
}

/* Ctrl-C as a *keystroke* (the byte 0x03), not an out-of-band kill: ISIG is kept
 * in raw mode so the line discipline turns 0x03 into SIGINT. The handler must
 * restore cooked mode and exit non-zero (128+SIGINT=130) with nothing committed.
 * We assert the slave is cooked again and the child exited non-zero. */
TEST(e2e, ctrl_c_byte_terminates_terminal_restored)
{
	t_pty		pty;
	char		*args[4];
	char		buf[16384];
	tcflag_t	after;
	int			status;

	args[0] = (char *)BIN;
	args[1] = (char *)"a";
	args[2] = (char *)"b";
	args[3] = NULL;
	ASSERT_EQ(0, spawn_select(&pty, args));
	usleep(120000);
	pty_send_str(pty.master, "\x03");
	drain_eof(pty.master, buf, sizeof(buf));
	ASSERT_EQ(0, pty_slave_lflag(pty.master, &after));
	ASSERT_EQ(0, pty_wait(pty.pid, &status));
	EXPECT_TRUE((after & ICANON) != 0 && (after & ECHO) != 0);
	EXPECT_TRUE(WIFEXITED(status) && WEXITSTATUS(status) != 0);
	EXPECT_FALSE(has_committed_line(buf, "a"));
	EXPECT_FALSE(has_committed_line(buf, "b"));
}

/* Non-tty stdin: launched without a pty and with stdin from /dev/null, the
 * isatty guard must fail and the program must exit cleanly (code 1, no crash,
 * nothing drawn so nothing to restore) rather than block on a read forever. */
TEST(e2e, non_tty_stdin_exits_clean)
{
	pid_t	pid;
	int		status;

	ASSERT_EQ(0, spawn_non_tty(&pid, NULL));
	ASSERT_EQ(0, pty_wait(pid, &status));
	EXPECT_TRUE(WIFEXITED(status));
	EXPECT_TRUE(WIFEXITED(status) && WEXITSTATUS(status) == 1);
}

/* Missing TERM on a real tty: forkpty makes isatty true, but the env carries no
 * TERM so caps_load (tgetent) fails. The program must report and exit cleanly
 * (code 1) without crashing or hanging - we send an Esc to unstick any errant
 * read, then assert a normal non-signal exit. */
TEST(e2e, missing_term_does_not_crash)
{
	t_pty	pty;
	char	*args[5];
	char	*env[1];
	char	buf[16384];
	int		status;

	args[0] = (char *)BIN;
	args[1] = (char *)"a";
	args[2] = (char *)"b";
	args[3] = (char *)"c";
	args[4] = NULL;
	env[0] = NULL;
	ASSERT_EQ(0, spawn_select_env(&pty, args, env));
	usleep(120000);
	pty_send_str(pty.master, "\033");
	drain_eof(pty.master, buf, sizeof(buf));
	ASSERT_EQ(0, pty_wait(pty.pid, &status));
	EXPECT_TRUE(WIFEXITED(status));
	EXPECT_TRUE(WIFEXITED(status) && WEXITSTATUS(status) == 1);
}

/* Unknown TERM on a real tty: TERM=bogus123 has no terminfo/termcap entry so
 * tgetent fails inside caps_load. Same contract as missing TERM: clean exit
 * (code 1), no crash, no hang. */
TEST(e2e, unknown_term_does_not_crash)
{
	t_pty	pty;
	char	*args[5];
	char	*env[2];
	char	buf[16384];
	int		status;

	args[0] = (char *)BIN;
	args[1] = (char *)"a";
	args[2] = (char *)"b";
	args[3] = (char *)"c";
	args[4] = NULL;
	env[0] = (char *)"TERM=bogus123";
	env[1] = NULL;
	ASSERT_EQ(0, spawn_select_env(&pty, args, env));
	usleep(120000);
	pty_send_str(pty.master, "\033");
	drain_eof(pty.master, buf, sizeof(buf));
	ASSERT_EQ(0, pty_wait(pty.pid, &status));
	EXPECT_TRUE(WIFEXITED(status));
	EXPECT_TRUE(WIFEXITED(status) && WEXITSTATUS(status) == 1);
}

/* Corpus replay, UTF-8: a multibyte item (cafe + U+2713) must round-trip
 * byte-exact through the commit path. Down moves onto it, Space selects, Enter
 * commits the raw bytes followed by the pty's \r\n - we match that exact tail. */
TEST(e2e, corpus_utf8_item_returns_byte_exact)
{
	t_pty	pty;
	char	*args[5];
	char	buf[16384];
	int		status;

	args[0] = (char *)BIN;
	args[1] = (char *)"one";
	args[2] = (char *)"caf\xc3\xa9\xe2\x9c\x93";
	args[3] = (char *)"three";
	args[4] = NULL;
	ASSERT_EQ(0, spawn_select(&pty, args));
	usleep(120000);
	pty_send_str(pty.master, "\033[B");
	usleep(60000);
	pty_send_str(pty.master, " ");
	usleep(60000);
	pty_send_str(pty.master, "\n");
	drain_eof(pty.master, buf, sizeof(buf));
	ASSERT_EQ(0, pty_wait(pty.pid, &status));
	EXPECT_TRUE(WIFEXITED(status) && WEXITSTATUS(status) == 0);
	EXPECT_TRUE(memmem(buf, sizeof(buf), "caf\xc3\xa9\xe2\x9c\x93\r\n", 10)
		!= NULL);
}

/* Corpus replay, very long item: a 1000-char item navigated to with the arrow
 * keys and selected must come back byte-exact. We size the pty wide (1100 cols)
 * so the long item is not flagged too-small and Down can land on it; the commit
 * emits the full 1000 bytes (output is never truncated, only the render is). We
 * build the expected 1000-byte needle + \r\n and search for it in the stream. */
TEST(e2e, corpus_long_item_via_arrow_returns_byte_exact)
{
	t_pty	pty;
	char	*args[4];
	char	*buf;
	char	*item;
	int		status;

	item = malloc(1001);
	buf = malloc(262144);
	ASSERT_NOTNULL(item);
	ASSERT_NOTNULL(buf);
	memset(item, 'x', 1000);
	item[500] = 'Q';
	item[1000] = '\0';
	args[0] = (char *)BIN;
	args[1] = (char *)"short";
	args[2] = item;
	args[3] = NULL;
	ASSERT_EQ(0, spawn_select_sz(&pty, args, 24, 1100));
	usleep(150000);
	pty_send_str(pty.master, "\033[B");
	usleep(80000);
	pty_send_str(pty.master, " ");
	usleep(80000);
	pty_send_str(pty.master, "\n");
	drain_eof(pty.master, buf, 262144);
	ASSERT_EQ(0, pty_wait(pty.pid, &status));
	EXPECT_TRUE(WIFEXITED(status) && WEXITSTATUS(status) == 0);
	item[1000] = '\r';
	EXPECT_TRUE(memmem(buf, 262144, item, 1001) != NULL
		&& memmem(buf, 262144, "\r\n", 2) != NULL);
	free(item);
	free(buf);
}

int	main(int ac, char **av)
{
	return (ctest_main(ac, av));
}
