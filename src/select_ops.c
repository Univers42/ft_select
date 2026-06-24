/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   select_ops.c                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dlesieur <dlesieur@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/24 03:07:07 by dlesieur          #+#    #+#             */
/*   Updated: 2026/06/24 03:07:07 by dlesieur         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ft_select.h"

/*
** Pure selection + delete operations on the parallel vectors (items: char *,
** selected: unsigned char 0/1). sel_toggle flips one flag. item_delete frees
** the item at cur, shifts both vectors left in lockstep, shrinks both lengths
** and returns the clamped cursor; the caller treats len == 0 (empty list) as an
** Esc-equivalent exit.
*/

void	sel_toggle(t_vec *selected, size_t i)
{
	unsigned char	*flag;

	if (i >= selected->len)
		return ;
	flag = (unsigned char *)vec_idx(selected, i);
	*flag = (unsigned char)(*flag ^ 1);
}

static void	shift_left(t_vec *items, t_vec *selected, size_t cur)
{
	size_t			i;
	char			**slot;
	unsigned char	*flag;

	i = cur;
	while (i + 1 < items->len)
	{
		slot = (char **)vec_idx(items, i);
		*slot = *(char **)vec_idx(items, i + 1);
		flag = (unsigned char *)vec_idx(selected, i);
		*flag = *(unsigned char *)vec_idx(selected, i + 1);
		i++;
	}
}

size_t	item_delete(t_vec *items, t_vec *selected, size_t cur)
{
	char	**slot;

	if (cur >= items->len)
		return (cur);
	slot = (char **)vec_idx(items, cur);
	fn_free(*slot);
	shift_left(items, selected, cur);
	items->len--;
	selected->len--;
	if (items->len == 0)
		return (0);
	if (cur >= items->len)
		return (items->len - 1);
	return (cur);
}
