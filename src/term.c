/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   term.c                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dlesieur <dlesieur@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/24 03:07:07 by dlesieur          #+#    #+#             */
/*   Updated: 2026/06/24 03:07:07 by dlesieur         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ft_select.h"

/*
** Terminal lifecycle: raw-mode entry/exit, capability loading and size query.
** term_setup guards an interactive tty + a known TERM, snapshots termios, then
** enters raw mode clearing ICANON|ECHO only (ISIG kept so signals still fire).
** term_restore is idempotent (raw_active guard): show cursor, reset attributes,
** restore the saved termios. caps_load fills t_caps from libft tgetent/tgetstr.
*/

int	term_raw(t_app *app)
{
	struct termios	raw;

	raw = app->saved;
	raw.c_lflag = raw.c_lflag & (tcflag_t) ~(ICANON | ECHO);
	raw.c_cc[VMIN] = 1;
	raw.c_cc[VTIME] = 0;
	if (tcsetattr(app->tty_fd, TCSADRAIN, &raw) != 0)
		return (-1);
	app->raw_active = 1;
	return (0);
}

int	term_setup(t_app *app)
{
	if (!isatty(STDIN_FILENO) || getenv("TERM") == NULL)
	{
		ft_putstr_fd("ft_select: not a terminal or TERM unset\n", 2);
		return (-1);
	}
	app->tty_fd = STDIN_FILENO;
	if (tcgetattr(app->tty_fd, &app->saved) != 0)
	{
		ft_putstr_fd("ft_select: tcgetattr failed\n", 2);
		return (-1);
	}
	if (term_raw(app) != 0)
	{
		ft_putstr_fd("ft_select: cannot enter raw mode\n", 2);
		return (-1);
	}
	return (0);
}

void	term_restore(t_app *app)
{
	if (!app->raw_active)
		return ;
	write(STDOUT_FILENO, "\033[?25h", 6);
	if (app->caps.me != NULL)
		write(STDOUT_FILENO, app->caps.me, ft_strlen(app->caps.me));
	if (app->caps.cl != NULL)
		write(STDOUT_FILENO, app->caps.cl, ft_strlen(app->caps.cl));
	tcsetattr(app->tty_fd, TCSADRAIN, &app->saved);
	app->raw_active = 0;
}

int	caps_load(t_caps *caps)
{
	char	*area;

	if (tgetent(caps->entry, getenv("TERM")) != 1)
		return (-1);
	area = caps->area;
	caps->cm = tgetstr("cm", &area);
	caps->cl = tgetstr("cl", &area);
	caps->ce = tgetstr("ce", &area);
	caps->cd = tgetstr("cd", &area);
	caps->so = tgetstr("so", &area);
	caps->se = tgetstr("se", &area);
	caps->us = tgetstr("us", &area);
	caps->ue = tgetstr("ue", &area);
	caps->me = tgetstr("me", &area);
	if (caps->so == NULL)
		caps->so = tgetstr("mr", &area);
	if (caps->cm == NULL || caps->cl == NULL)
		return (-1);
	return (0);
}

void	term_dims(int *rows, int *cols)
{
	struct winsize	ws;

	*rows = 0;
	*cols = 0;
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0)
	{
		*rows = ws.ws_row;
		*cols = ws.ws_col;
	}
	if (*rows < 1)
		*rows = tgetnum("li");
	if (*cols < 1)
		*cols = tgetnum("co");
	if (*rows < 1)
		*rows = 1;
	if (*cols < 1)
		*cols = 1;
}
