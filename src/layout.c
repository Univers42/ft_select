/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   layout.c                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dlesieur <dlesieur@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/24 03:07:07 by dlesieur          #+#    #+#             */
/*   Updated: 2026/06/24 03:07:07 by dlesieur         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ft_select.h"

/*
** Pure ls-style column-major packing. Geometry is derived from the item count
** (n), the terminal size (cols x rows) and the widest item (max_w). Every
** divisor is floored at 1 so we never divide by zero; too_small flags a window
** that cannot host even one column.
*/

static int	ceil_div(int a, int b)
{
	if (b < 1)
		b = 1;
	if (a <= 0)
		return (0);
	return ((a + b - 1) / b);
}

/* Rows-first packing: use as many columns as it takes to keep every item on a
** visible row (ceil(n/rows)). The window may then be too narrow to show all of
** them at once - the bonus col_off scroll slides that window over them. */
static int	pack_cols(int n, int rows)
{
	if (n <= rows)
		return (1);
	return (ceil_div(n, rows));
}

t_layout	layout_compute(size_t n, int cols, int rows, int max_w)
{
	t_layout	l;

	l.term_cols = cols;
	l.term_rows = rows;
	l.col_w = max_w + GUTTER;
	if (l.col_w > cols)
		l.col_w = cols;
	if (l.col_w < 1)
		l.col_w = 1;
	l.too_small = (cols < 1 || rows < 1 || cols < max_w + GUTTER);
	l.n_cols = pack_cols((int)n, rows);
	l.n_rows = ceil_div((int)n, l.n_cols);
	if (l.n_rows < 1)
		l.n_rows = 1;
	return (l);
}

int	cell_of_index(t_layout l, size_t idx, int *row, int *col)
{
	int	rows;

	if (l.too_small)
		return (-1);
	rows = l.n_rows;
	if (rows < 1)
		rows = 1;
	*col = (int)idx / rows;
	*row = (int)idx % rows;
	return (0);
}

long	index_of_cell(t_layout l, int row, int col, size_t n)
{
	long	idx;

	if (l.too_small || row < 0 || col < 0)
		return (-1);
	if (row >= l.n_rows || col >= l.n_cols)
		return (-1);
	idx = (long)col * l.n_rows + row;
	if (idx >= (long)n)
		return (-1);
	return (idx);
}
