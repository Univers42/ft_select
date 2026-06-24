/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   navigate.c                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dlesieur <dlesieur@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/24 03:07:07 by dlesieur          #+#    #+#             */
/*   Updated: 2026/06/24 03:07:07 by dlesieur         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ft_select.h"

/*
** Pure circular navigation over the column-major grid. The cursor is an item
** index; we convert it to (row, col), step in the requested direction wrapping
** around the grid, then land on the first occupied cell so the returned index
** is always valid in [0, n). Empty trailing cells (the last column may be
** short) are skipped by walking on in the same direction. rc[0]=row, rc[1]=col;
** dir picks the axis and the wrap step.
*/

static void	axis_params(t_dir d, int *axis, int *step)
{
	if (d == D_UP)
	{
		*axis = 0;
		*step = -1;
	}
	else if (d == D_DOWN)
	{
		*axis = 0;
		*step = 1;
	}
	else if (d == D_LEFT)
	{
		*axis = 1;
		*step = -1;
	}
	else
	{
		*axis = 1;
		*step = 1;
	}
}

static size_t	nav_search(t_layout l, int rc[2], t_dir d, size_t n)
{
	int		span[2];
	int		axis;
	int		step;
	int		tries;
	long	idx;

	span[0] = l.n_rows;
	span[1] = l.n_cols;
	axis_params(d, &axis, &step);
	if (span[axis] < 1)
		span[axis] = 1;
	tries = 0;
	while (tries < span[axis])
	{
		rc[axis] = (rc[axis] % span[axis] + span[axis]) % span[axis];
		idx = index_of_cell(l, rc[0], rc[1], n);
		if (idx >= 0)
			return ((size_t)idx);
		rc[axis] += step;
		tries++;
	}
	return (0);
}

size_t	nav_move(t_layout l, size_t cur, size_t n, t_dir d)
{
	int	rc[2];
	int	axis;
	int	step;

	if (n == 0)
		return (0);
	if (cell_of_index(l, cur, &rc[0], &rc[1]) < 0)
		return (cur);
	axis_params(d, &axis, &step);
	rc[axis] += step;
	return (nav_search(l, rc, d, n));
}
