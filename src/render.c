/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   render.c                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dlesieur <dlesieur@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/24 03:07:07 by dlesieur          #+#    #+#             */
/*   Updated: 2026/06/24 03:07:07 by dlesieur         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ft_select.h"

/*
** Whole-frame assembly into app->frame (a t_vec<char>), flushed with one write
** so there is no flicker. Each item is positioned with tgoto(cm,x,row), wrapped
** in an attribute cap by its (selected,cursor) state, optionally tinted by file
** extension (bonus), truncated to fit the column, then closed. Horizontal
** scroll (bonus): only columns in [col_off, col_off+visible_cols) are drawn, x
** position is shifted by col_off so the visible window follows the cursor. Caps
** carry no termcap padding here, so a single write emits the frame.
*/

static void	put_cap(t_vec *f, const char *cap)
{
	if (cap != NULL)
		vec_push_str(f, cap);
}

/* Emit the (selected, cursor) attribute caps. open=1 enters them (so then us),
** open=0 leaves them in reverse order (ue then se) so they nest cleanly. */
static void	attr(t_app *app, size_t idx, int open)
{
	unsigned char	sel;

	sel = 0;
	if (idx < app->selected.len)
		sel = *(unsigned char *)vec_idx(&app->selected, idx);
	if (open)
	{
		if (sel)
			put_cap(&app->frame, app->caps.so);
		if (idx == app->cursor)
			put_cap(&app->frame, app->caps.us);
		return ;
	}
	if (idx == app->cursor)
		put_cap(&app->frame, app->caps.ue);
	if (sel)
		put_cap(&app->frame, app->caps.se);
}

static void	put_text(t_app *app, char *text)
{
	const char	*color;
	size_t		max;
	size_t		len;

	color = ext_color(text);
	put_cap(&app->frame, color);
	len = ft_strlen(text);
	max = (size_t)(app->lay.term_cols - 1);
	if (len > max)
		len = max;
	vec_push_nstr(&app->frame, text, len);
	if (*color != '\0')
		put_cap(&app->frame, "\033[39m");
}

static void	put_item(t_app *app, size_t idx, int row, int col)
{
	int	x;

	x = (col - app->col_off) * app->lay.col_w;
	put_cap(&app->frame, tgoto(app->caps.cm, x, row));
	attr(app, idx, 1);
	put_text(app, *(char **)vec_idx(&app->items, idx));
	attr(app, idx, 0);
}

void	render_frame(t_app *app)
{
	size_t	i;
	int		row;
	int		col;

	vec_clear(&app->frame);
	put_cap(&app->frame, app->caps.cl);
	if (!app->lay.too_small)
	{
		i = 0;
		while (i < app->items.len)
		{
			if (cell_of_index(app->lay, i, &row, &col) == 0
				&& col >= app->col_off
				&& col < app->col_off + visible_cols(app->lay))
				put_item(app, i, row, col);
			i++;
		}
	}
	write(STDOUT_FILENO, app->frame.ctx, app->frame.len);
}
