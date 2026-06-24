/* pty.c - forkpty helpers (see pty.h). TEST CODE: unrestricted libc.
 * Phase 1 requires this to build/link clean; it is driven for real in the
 * E2E phase. _DEFAULT_SOURCE pulls in forkpty/openpty prototypes from <pty.h>.
 */
#ifndef _DEFAULT_SOURCE
# define _DEFAULT_SOURCE
#endif
#include "ctest_pty.h"
#include <pty.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <errno.h>

static void	winsize_set(struct winsize *ws, int rows, int cols)
{
	memset(ws, 0, sizeof(*ws));
	ws->ws_row = (unsigned short)rows;
	ws->ws_col = (unsigned short)cols;
}

int	pty_spawn(t_pty *out, char *const argv[], int rows, int cols,
		char *const envp[])
{
	struct winsize	ws;
	int				master;
	pid_t			pid;

	winsize_set(&ws, rows, cols);
	pid = forkpty(&master, NULL, NULL, &ws);
	if (pid < 0)
		return (-1);
	if (pid == 0)
	{
		if (envp)
			execve(argv[0], argv, envp);
		else
			execvp(argv[0], argv);
		_exit(127);
	}
	out->master = master;
	out->pid = pid;
	return (0);
}

int	pty_send(int master, const void *bytes, size_t n)
{
	size_t		off;
	ssize_t		w;

	off = 0;
	while (off < n)
	{
		w = write(master, (const char *)bytes + off, n - off);
		if (w < 0)
		{
			if (errno == EINTR)
				continue ;
			return (-1);
		}
		off += (size_t)w;
	}
	return (0);
}

int	pty_send_str(int master, const char *s)
{
	return (pty_send(master, s, strlen(s)));
}

/* drain until substr seen or timeout. keeps a sliding accumulator capped so a
 * chatty child cannot grow it without bound (only the tail matters). */
static int	scan_buf(char *acc, size_t *len, const char *sub, size_t sublen)
{
	if (*len >= sublen
		&& memmem(acc, *len, sub, sublen) != NULL)
		return (1);
	if (*len > 8192)
	{
		memmove(acc, acc + (*len - sublen), sublen);
		*len = sublen;
	}
	return (0);
}

static int	wait_readable(int fd, int timeout_ms)
{
	fd_set			rfds;
	struct timeval	tv;

	FD_ZERO(&rfds);
	FD_SET(fd, &rfds);
	tv.tv_sec = timeout_ms / 1000;
	tv.tv_usec = (timeout_ms % 1000) * 1000;
	return (select(fd + 1, &rfds, NULL, NULL, &tv));
}

int	pty_expect(int master, const char *substr, int timeout_ms)
{
	char	acc[8192 + 256];
	size_t	len;
	ssize_t	r;
	size_t	sublen;
	int		sel;

	len = 0;
	sublen = strlen(substr);
	while (1)
	{
		sel = wait_readable(master, timeout_ms);
		if (sel <= 0)
			return (sel == 0 ? 0 : -1);
		r = read(master, acc + len, sizeof(acc) - len);
		if (r <= 0)
			return (-1);
		len += (size_t)r;
		if (scan_buf(acc, &len, substr, sublen))
			return (1);
	}
}

int	pty_resize(int master, int rows, int cols)
{
	struct winsize	ws;

	winsize_set(&ws, rows, cols);
	if (ioctl(master, TIOCSWINSZ, &ws) < 0)
		return (-1);
	return (0);
}

int	pty_wait(pid_t pid, int *status)
{
	if (waitpid(pid, status, 0) < 0)
		return (-1);
	return (0);
}

int	pty_slave_lflag(int master, tcflag_t *out_lflag)
{
	struct termios	tio;

	if (tcgetattr(master, &tio) < 0)
		return (-1);
	*out_lflag = tio.c_lflag;
	return (0);
}
