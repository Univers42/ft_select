/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   loop.c                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dlesieur <dlesieur@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/24 03:07:07 by dlesieur          #+#    #+#             */
/*   Updated: 2026/06/24 03:07:07 by dlesieur         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ft_select.h"

/*
** The interactive run loop: render a frame, read one decoded key, dispatch it,
** re-derive the layout and clamp the cursor, repeat until the user commits
** (Enter) or cancels (Esc / deletes the last item). relayout re-queries the
** terminal size after any mutation so a resize or delete reflows correctly;
** commit prints the selection in argv order and restores the terminal.
*/

static void	relayout(t_app *app)
{
	term_dims(&app->lay.term_rows, &app->lay.term_cols);
	app->lay = layout_compute(app->items.len, app->lay.term_cols,
			app->lay.term_rows, (int)max_item_width(&app->items));
	if (app->items.len == 0)
		app->cursor = 0;
	else if (app->cursor >= app->items.len)
		app->cursor = app->items.len - 1;
	scroll_to_cursor(app);
}

/* A SIGWINCH (resize) or SIGCONT (resume from Ctrl-Z) interrupts the blocking
** read; the handler only flags g_signo. Re-enter raw mode after a stop, then
** reflow and redraw. Returns the signal consumed (0 if none was pending). */
static int	drain_signal(t_app *app)
{
	int	signo;

	signo = g_signo;
	if (signo == 0)
		return (0);
	g_signo = 0;
	if (signo == SIGCONT)
		term_raw(app);
	relayout(app);
	render_frame(app);
	return (signo);
}

static int	commit(t_app *app)
{
	char	*out;

	out = build_output(&app->items, &app->selected);
	if (out != NULL)
	{
		write(STDOUT_FILENO, out, ft_strlen(out));
		write(STDOUT_FILENO, "\n", 1);
		fn_free(out);
	}
	term_restore(app);
	return (1);
}

static int	dispatch(t_app *app, t_key k)
{
	if (k == K_UP || k == K_DOWN || k == K_LEFT || k == K_RIGHT)
		app->cursor = nav_move(app->lay, app->cursor, app->items.len,
				(t_dir)(k - K_UP));
	else if (k == K_SPACE)
	{
		sel_toggle(&app->selected, app->cursor);
		app->cursor = nav_move(app->lay, app->cursor, app->items.len, D_DOWN);
	}
	else if (k == K_DEL && !search_erase(app))
		app->cursor = item_delete(&app->items, &app->selected, app->cursor);
	else if (k == K_ENTER)
		return (commit(app));
	else if (k == K_ESC && !search_cancel(app))
		return (term_restore(app), 1);
	scroll_to_cursor(app);
	return (0);
}

int	run_loop(t_app *app)
{
	t_key			k;
	unsigned char	ch;

	while (1)
	{
		render_frame(app);
		k = key_read(&ch);
		if (k == K_NONE && drain_signal(app))
			continue ;
		if (k == K_PRINT)
			search_input(app, ch);
		else if (k != K_NONE && dispatch(app, k))
			return (0);
		if (app->items.len == 0)
			return (term_restore(app), 0);
		relayout(app);
	}
}
