/* leak_driver.c - PTY driver for the valgrind leak matrix (TEST CODE, any libc).
 *
 * Interactive valgrind cannot share the program's UI stream: valgrind's report
 * must land somewhere the pty does not. This driver runs valgrind AS THE PTY
 * CHILD with `--log-file=<path>` (valgrind opens its own fd, off the UI), drives
 * a scripted keystroke sequence over the pty master, drains the slave to EOF,
 * delivers an optional out-of-band signal, then reaps and forwards the child's
 * exit status. The companion leaks.sh greps each per-context log for the leak
 * verdict. valgrind's own --error-exitcode=42 is what makes a leak a failure.
 *
 * usage:  leak_driver <rows> <cols> <termval|-> <logfile> <script> [args...]
 *   termval "-"   -> spawn with NO TERM in the env (missing-TERM context)
 *   termval "x"   -> spawn with TERM=<x>
 * script: a string of tokens separated by ';' replayed after a startup delay:
 *   D=down U=up L=left R=right  SP=space  CR=enter  ESC=escape  DEL=delete(0x7f)
 *   C-C=ctrl-c byte(0x03)  C-Z=sigtstp(kill)  CONT=sigcont(kill)  INT=sigint(kill)
 *   W<r>x<c>=resize  s<ms>=sleep ms  T<text>=type literal text
 * stdout from valgrind/--log-file is NOT on the pty; we exit with the child's
 * code (so a valgrind --error-exitcode=42 surfaces as our exit 42).
 */
#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif
#include <pty.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/wait.h>

#define VG "valgrind"

/* build the valgrind argv: full leak + fd tracking, log to a file (own fd),
 * error-exitcode 42 so any error/leak makes the run fail loud. */
static char	**make_argv(const char *logfile, char **prog, int nprog)
{
	static char	*argv[64];
	static char	logopt[1024];
	int			i;
	int			n;

	snprintf(logopt, sizeof(logopt), "--log-file=%s", logfile);
	n = 0;
	argv[n++] = (char *)VG;
	argv[n++] = (char *)"--leak-check=full";
	argv[n++] = (char *)"--show-leak-kinds=all";
	argv[n++] = (char *)"--track-fds=yes";
	argv[n++] = (char *)"--errors-for-leak-kinds=all";
	argv[n++] = (char *)"--error-exitcode=42";
	argv[n++] = logopt;
	i = 0;
	while (i < nprog && n < 62)
		argv[n++] = prog[i++];
	argv[n] = NULL;
	return (argv);
}

static void	send_all(int fd, const char *b, size_t n)
{
	ssize_t	w;
	size_t	off;

	off = 0;
	while (off < n)
	{
		w = write(fd, b + off, n - off);
		if (w <= 0)
			return ;
		off += (size_t)w;
	}
}

static void	do_resize(int master, const char *tok)
{
	struct winsize	ws;
	int				r;
	int				c;

	memset(&ws, 0, sizeof(ws));
	if (sscanf(tok + 1, "%dx%d", &r, &c) == 2)
	{
		ws.ws_row = (unsigned short)r;
		ws.ws_col = (unsigned short)c;
		ioctl(master, TIOCSWINSZ, &ws);
	}
}

/* run one scripted token against the pty (master) / child (pid). */
static void	run_token(int master, pid_t pid, const char *t)
{
	if (!strcmp(t, "D"))
		send_all(master, "\033[B", 3);
	else if (!strcmp(t, "U"))
		send_all(master, "\033[A", 3);
	else if (!strcmp(t, "L"))
		send_all(master, "\033[D", 3);
	else if (!strcmp(t, "R"))
		send_all(master, "\033[C", 3);
	else if (!strcmp(t, "SP"))
		send_all(master, " ", 1);
	else if (!strcmp(t, "CR"))
		send_all(master, "\n", 1);
	else if (!strcmp(t, "ESC"))
		send_all(master, "\033", 1);
	else if (!strcmp(t, "DEL"))
		send_all(master, "\x7f", 1);
	else if (!strcmp(t, "C-C"))
		send_all(master, "\x03", 1);
	else if (!strcmp(t, "C-Z"))
		kill(pid, SIGTSTP);
	else if (!strcmp(t, "CONT"))
		kill(pid, SIGCONT);
	else if (!strcmp(t, "INT"))
		kill(pid, SIGINT);
	else if (t[0] == 'W')
		do_resize(master, t);
	else if (t[0] == 's')
		usleep((useconds_t)atoi(t + 1) * 1000);
	else if (t[0] == 'T')
		send_all(master, t + 1, strlen(t + 1));
}

/* valgrind adds heavy startup + per-instruction latency, so keystrokes sent
 * too early are dropped before the program enters raw mode and its read loop -
 * which on a delete/exit path leaves it blocked forever. A 350 ms warm-up and a
 * 150 ms inter-key gap are empirically reliable under Memcheck on this host. */
static void	play_script(int master, pid_t pid, char *script)
{
	char	*tok;
	char	*save;

	usleep(350000);
	tok = strtok_r(script, ";", &save);
	while (tok != NULL)
	{
		run_token(master, pid, tok);
		usleep(150000);
		tok = strtok_r(NULL, ";", &save);
	}
}

/* drain the pty master to EOF with a bounded select so a stuck child fails loud
 * instead of hanging the matrix. Output is discarded (UI noise). Drains and
 * reaps in one place: returns once the child has exited AND the pty has no more
 * pending data, or after a hard wall-clock cap. *out_status holds the reaped
 * wait status (or -1 if the child never exited and we gave up). Reaping the
 * child closes the slave so the master finally yields EOF; without polling the
 * child a slow valgrind teardown can keep the master readable for seconds. */
static void	drain_eof(int master, pid_t pid, int *out_status)
{
	char			buf[4096];
	fd_set			rfds;
	struct timeval	tv;
	int				reaped;
	int				idle;

	reaped = 0;
	idle = 0;
	*out_status = -1;
	while (idle < 60)
	{
		if (!reaped && waitpid(pid, out_status, WNOHANG) == pid)
			reaped = 1;
		FD_ZERO(&rfds);
		FD_SET(master, &rfds);
		tv.tv_sec = 0;
		tv.tv_usec = 200000;
		if (select(master + 1, &rfds, NULL, NULL, &tv) > 0
			&& read(master, buf, sizeof(buf)) > 0)
		{
			idle = 0;
			continue ;
		}
		if (reaped)
			break ;
		idle++;
	}
	if (!reaped)
	{
		kill(pid, SIGKILL);
		waitpid(pid, out_status, 0);
		*out_status = -1;
	}
}

static char	**build_env(const char *termval, char **out)
{
	static char	termbuf[256];

	if (!strcmp(termval, "-"))
	{
		out[0] = NULL;
		return (out);
	}
	snprintf(termbuf, sizeof(termbuf), "TERM=%s", termval);
	out[0] = termbuf;
	out[1] = NULL;
	return (out);
}

int	main(int ac, char **av)
{
	struct winsize	ws;
	char			*env[2];
	char			**argv;
	int				master;
	pid_t			pid;
	int				status;

	if (ac < 6)
		return (fprintf(stderr, "usage: leak_driver r c term log script args\n"),
			2);
	memset(&ws, 0, sizeof(ws));
	ws.ws_row = (unsigned short)atoi(av[1]);
	ws.ws_col = (unsigned short)atoi(av[2]);
	build_env(av[3], env);
	argv = make_argv(av[4], &av[6], ac - 6);
	pid = forkpty(&master, NULL, NULL, &ws);
	if (pid < 0)
		return (perror("forkpty"), 1);
	if (pid == 0)
	{
		execvpe(VG, argv, env);
		_exit(127);
	}
	play_script(master, pid, av[5]);
	drain_eof(master, pid, &status);
	if (status == -1)
		return (1);
	if (WIFEXITED(status))
		return (WEXITSTATUS(status));
	return (WIFSIGNALED(status) ? 128 + WTERMSIG(status) : 1);
}
