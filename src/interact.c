/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   interact.c                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dlesieur <dlesieur@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/24 03:07:07 by dlesieur          #+#    #+#             */
/*   Updated: 2026/06/24 03:07:07 by dlesieur         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ft_select.h"

/*
** Bonus interaction glue between the run loop and the pure search/layout cores.
** scroll_to_cursor keeps the horizontal column window (app->col_off) following
** the cursor when the columns overflow the width. search_input/erase/cancel
** drive the dynamic-search query (app->search, a byte-vec): a printable key
** extends it and jumps the cursor to the first matching item; backspace edits
** it; esc clears it (and only the empty-query esc falls through to exit).
*/

/* How many whole columns fit on screen at once (>= 1). When this is < n_cols
** the layout overflows horizontally and col_off scrolls the visible window. */
int	visible_cols(t_layout l)
{
	int	fit;

	if (l.col_w < 1)
		return (1);
	fit = l.term_cols / l.col_w;
	if (fit < 1)
		fit = 1;
	if (fit > l.n_cols)
		fit = l.n_cols;
	return (fit);
}

void	scroll_to_cursor(t_app *app)
{
	int	row;
	int	col;
	int	vis;
	int	max_off;

	if (app->lay.too_small || cell_of_index(app->lay, app->cursor, &row,
			&col) != 0)
		return ;
	vis = visible_cols(app->lay);
	if (col < app->col_off)
		app->col_off = col;
	else if (col >= app->col_off + vis)
		app->col_off = col - vis + 1;
	max_off = app->lay.n_cols - vis;
	if (app->col_off > max_off)
		app->col_off = max_off;
	if (app->col_off < 0)
		app->col_off = 0;
}

void	search_input(t_app *app, unsigned char ch)
{
	long	hit;

	vec_push_byte(&app->search, ch);
	hit = search_match(&app->items, search_query(&app->search), 0);
	if (hit >= 0)
		app->cursor = (size_t)hit;
}

int	search_erase(t_app *app)
{
	long	hit;

	if (app->search.len == 0)
		return (0);
	app->search.len--;
	((char *)app->search.ctx)[app->search.len] = '\0';
	if (app->search.len == 0)
		return (1);
	hit = search_match(&app->items, search_query(&app->search), 0);
	if (hit >= 0)
		app->cursor = (size_t)hit;
	return (1);
}

int	search_cancel(t_app *app)
{
	if (app->search.len == 0)
		return (0);
	vec_clear(&app->search);
	return (1);
}
