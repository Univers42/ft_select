/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   signals.c                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dlesieur <dlesieur@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/24 03:07:07 by dlesieur          #+#    #+#             */
/*   Updated: 2026/06/24 03:07:07 by dlesieur         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ft_select.h"
#include <unistd.h>

/*
** Signal handling with signal() only (never sigaction). The sole global in the
** whole program is g_signo: handlers set it and the run loop drains it. A small
** file-scope struct (covered by the signal exception) stashes the restore
** essentials - the saved termios, the tty fd, the reset-attrs string and the
** t_app pointer - so a handler can put the terminal back and free the heap
** without reaching through a moving t_app. SIGWINCH/SIGCONT only flag the loop;
** SIGINT/SIGTERM restore, free the app and exit non-zero with no output (so a
** kill leaves neither a mangled terminal nor reachable heap behind).
**
** Ctrl-Z/fg: the allowed list has no self-signal primitive (raise and kill are
** both forbidden, and libft wraps neither), so we cannot programmatically stop
** the process. Instead the SIGTSTP handler restores cooked mode + shows the
** cursor and re-arms the default disposition (signal(SIGTSTP, SIG_DFL)); the
** kernel then performs the real job-control stop in cooked mode on the next
** Ctrl-Z. On resume SIGCONT re-enters raw, redraws and re-installs the handler,
** so the cycle repeats. This keeps the terminal sane while suspended with no
** non-allowed call.
*/

volatile sig_atomic_t	g_signo = 0;

static struct s_sigrestore
{
	struct termios	saved;
	int				tty_fd;
	const char		*reset;
	const char		*clear;
	t_app			*app;
	int				armed;
}	g_sig = {{0}, 0, 0, 0, 0, 0};

static void	restore_cooked(void)
{
	if (!g_sig.armed)
		return ;
	write(STDOUT_FILENO, "\033[?25h", 6);
	if (g_sig.reset != NULL)
		write(STDOUT_FILENO, g_sig.reset, ft_strlen(g_sig.reset));
	tcsetattr(g_sig.tty_fd, TCSADRAIN, &g_sig.saved);
}

void	signals_restore_term(void)
{
	restore_cooked();
}

/* disarm before teardown so a late SIGINT/SIGTERM cannot re-free the app */
void	signals_disarm(void)
{
	g_sig.armed = 0;
	g_sig.app = NULL;
}

static void	on_signal(int signo)
{
	if (signo == SIGWINCH || signo == SIGCONT)
	{
		g_signo = signo;
		if (signo == SIGCONT)
			signal(SIGTSTP, on_signal);
		return ;
	}
	if (signo == SIGTSTP)
	{
		restore_cooked();
		signal(SIGTSTP, SIG_DFL);
		return ;
	}
	if (g_sig.armed && g_sig.clear != NULL)
		write(STDOUT_FILENO, g_sig.clear, ft_strlen(g_sig.clear));
	restore_cooked();
	if (g_sig.app != NULL)
		app_free(g_sig.app);
	exit(128 + signo);
}

void	signals_install(t_app *app)
{
	g_sig.saved = app->saved;
	g_sig.tty_fd = app->tty_fd;
	g_sig.reset = app->caps.me;
	g_sig.clear = app->caps.cl;
	g_sig.app = app;
	g_sig.armed = 1;
	signal(SIGWINCH, on_signal);
	signal(SIGTSTP, on_signal);
	signal(SIGCONT, on_signal);
	signal(SIGINT, on_signal);
	signal(SIGTERM, on_signal);
}
