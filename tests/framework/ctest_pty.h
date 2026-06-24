/* pty.h - forkpty helpers for driving the interactive ft_select binary.
 *
 * TEST CODE: free to use forkpty/select/ioctl/etc. Used by the E2E layer
 * (Phase 3+). Phase 1 only needs this to compile and link cleanly; the
 * functions are exercised later.
 */
#ifndef PTY_H
# define PTY_H

# include <sys/types.h>
# include <termios.h>

typedef struct s_pty
{
	int		master;
	pid_t	pid;
}	t_pty;

/* fork a child on a fresh pty sized rows x cols, exec argv (env passed
 * through). returns 0 on success and fills *out; -1 on failure. */
int		pty_spawn(t_pty *out, char *const argv[], int rows, int cols,
			char *const envp[]);

/* raw byte / string send to the child's stdin (the pty master). */
int		pty_send(int master, const void *bytes, size_t n);
int		pty_send_str(int master, const char *s);

/* drain master with select() until `substr` is seen or timeout_ms elapses.
 * returns 1 if found, 0 on timeout, -1 on error/EOF-before-match. */
int		pty_expect(int master, const char *substr, int timeout_ms);

/* resize the pty (delivers SIGWINCH to the child). returns 0 / -1. */
int		pty_resize(int master, int rows, int cols);

/* reap the child. returns 0 and stores raw wait status into *status. */
int		pty_wait(pid_t pid, int *status);

/* capture the slave-side terminal local flags (c_lflag) so a caller can
 * prove the program restored cooked mode (ICANON|ECHO) on exit.
 * returns 0 / -1. */
int		pty_slave_lflag(int master, tcflag_t *out_lflag);

#endif
